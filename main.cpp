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
private:
    Rewriter& rewriter_;
public:

    CastCallBack(Rewriter& rewriter) : rewriter_(rewriter) {};

    virtual void run(const MatchFinder::MatchResult &Result) {
        const auto *expr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");

        if (expr->getExprLoc().isMacroID())
            return;

        if (expr->getCastKind() == CK_ToVoid)
            return;

        auto range = CharSourceRange::getCharRange(expr->getLParenLoc(), expr->getSubExprAsWritten()->getBeginLoc());
	    auto& SM = *Result.SourceManager;
	    StringRef type = Lexer::getSourceText(CharSourceRange::getTokenRange(expr->getLParenLoc().getLocWithOffset(1),
                                                    expr->getRParenLoc().getLocWithOffset(-1)), SM, Result.Context->getLangOpts());
        std::string text(("static_cast<" + type + ">").str());
        
        auto subExpr = expr->getSubExprAsWritten()->IgnoreImpCasts();
        if(!isa<ParenExpr>(subExpr)) {
           text.push_back('(');
           rewriter_.InsertText(Lexer::getLocForEndOfToken(subExpr->getEndLoc(), 0,
                                   *Result.SourceManager, Result.Context->getLangOpts()), ")");
        }

	    rewriter_.ReplaceText(range, text);
    }
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
    //auto Parser = llvm::ExitOnError()(CommonOptionsParser::create(argc, argv, CastMatcherCategory));
    
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
