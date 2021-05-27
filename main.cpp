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
    CastCallBack(Rewriter& rewriter) : _rewriter(rewriter){
    };

    virtual void run(const MatchFinder::MatchResult &Result) override {
        auto* CastExpr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
        SourceManager &SM = *Result.SourceManager;
        if (CastExpr->getExprLoc().isMacroID())
            return;
        if (CastExpr->getCastKind() == CK_ToVoid)
            return;
        if (SM.getFilename(SM.getSpellingLoc(CastExpr->getBeginLoc())).endswith(".c"))
            return;
        StringRef DestTypeString =
            Lexer::getSourceText(CharSourceRange::getTokenRange(
                CastExpr->getLParenLoc().getLocWithOffset(1),
                CastExpr->getRParenLoc().getLocWithOffset(-1)),
                SM, Result.Context->getLangOpts());
        std::string replaceText = ("static_cast<" + DestTypeString + ">").str();
        auto ReplaceRange = CharSourceRange::getCharRange(
            CastExpr->getLParenLoc(), CastExpr->getSubExprAsWritten()->getBeginLoc());
        auto castIgnore = CastExpr->getSubExprAsWritten()->IgnoreImpCasts();
        if (!isa<ParenExpr>(castIgnore)) {
            replaceText.push_back('(');
            _rewriter.InsertText(Lexer::getLocForEndOfToken(castIgnore->getEndLoc(),
                0, *Result.SourceManager, Result.Context->getLangOpts()), ")");
        }
        _rewriter.ReplaceText(ReplaceRange, replaceText);
    }
private:
    Rewriter& _rewriter;
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
        _rewriter.getEditBuffer(_rewriter.getSourceMgr().getMainFileID())
            .write(llvm::outs());
    }

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef /* file */) override {
        _rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<MyASTConsumer>(_rewriter);
    }

private:
    Rewriter _rewriter;
};

static llvm::cl::OptionCategory CastMatcherCategory("cast-matcher options");

int main(int argc, const char **argv) {
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
