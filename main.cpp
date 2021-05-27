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
    CastCallBack(Rewriter& rewriter) : rewriter_(rewriter) {};

    void run(const MatchFinder::MatchResult &Result) override {
    	if (const auto *Cast_Term = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")) {
		if (Cast_Term->getCastKind() == CK_ToVoid)
			return;
		if (Cast_Term->getExprLoc().isMacroID())
			return;
		
		auto Replace_Range = CharSourceRange::getCharRange(
				        Cast_Term->getLParenLoc(), Cast_Term->getSubExprAsWritten()->getBeginLoc());
		StringRef String_Type = Lexer::getSourceText(CharSourceRange::getTokenRange(
					              	Cast_Term->getLParenLoc().getLocWithOffset(1),
						        Cast_Term->getRParenLoc().getLocWithOffset(-1)),
				              		*Result.SourceManager, Result.Context->getLangOpts());
		std::string Text_Type(("static_cast<" + String_Type + ">").str());
		const auto *Cast_Ignore = Cast_Term->getSubExprAsWritten()->IgnoreImpCasts();
		if (!isa<ParenExpr>(Cast_Ignore)) {	
			Text_Type.push_back('(');
			rewriter_.InsertText(Lexer::getLocForEndOfToken(Cast_Ignore->getEndLoc(),
						   0, *Result.SourceManager, Result.Context->getLangOpts()), ")");
		}
			rewriter_.ReplaceText(Replace_Range, Text_Type);
        }
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
