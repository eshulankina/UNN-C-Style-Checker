#include <iostream>
#include <sstream>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Frontend/CompilerInstance.h>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

class CastCallBack : public MatchFinder::MatchCallback {
public:
    CastCallBack(Rewriter& rewriter) : rewriter_(rewriter) {
    };

    void run(const MatchFinder::MatchResult &Result) override {
    const auto* item = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
	SourceManager& source_manager = *Result.SourceManager;

	auto re_range = CharSourceRange::getCharRange (item->getLParenLoc(), item->getSubExprAsWritten()->getBeginLoc());

	StringRef type_string = 
              Lexer::getSourceText(CharSourceRange::getTokenRange(item->getLParenLoc().getLocWithOffset(1),
                                   item->getRParenLoc().getLocWithOffset(-1)),
		                           source_manager, Result.Context->getLangOpts());

	std::string str = ("static_cast<" + type_string + ">").str();

	const Expr* sub_expr = item->getSubExprAsWritten()->IgnoreImpCasts();

	if(!isa<ParenExpr>(sub_expr)) {
		str.push_back('(');
		rewriter_.InsertText(Lexer::getLocForEndOfToken(sub_expr->getEndLoc(), 0, source_manager,
                             Result.Context->getLangOpts()), ")");
	}

	rewriter_.ReplaceText(re_range, str);
    }
private:
	Rewriter& rewriter_;
};

class MyASTConsumer : public ASTConsumer {
public:
    MyASTConsumer(Rewriter &rewriter) : callback_(rewriter) {
        matcher_.addMatcher(
                cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast"), &callback_);
    }

    void HandleTranslationUnit(ASTContext &Context) override {
        matcher_.matchAST(Context);
    }

private:
    CastCallBack callback_;
    MatchFinder matcher_;
};

class CStyleCheckerFrontendAction : public ASTFrontendAction {
public:
    CStyleCheckerFrontendAction() = default;
    
    void EndSourceFileAction() override {
        rewriter_.getEditBuffer(rewriter_.getSourceMgr().getMainFileID())
            .write(llvm::outs());
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef /* file */) override {
        rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MyASTConsumer>(rewriter_);
    }

private:
    Rewriter rewriter_;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char **argv) {
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
