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
    Rewriter& rwr;
public:
    CastCallBack(Rewriter& rewriter) : rwr(rewriter) {};

    void run(const MatchFinder::MatchResult &Result) override {
        const auto *expr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
        if (expr != nullptr) {
            auto loc = CharSourceRange::getCharRange(expr->getLParenLoc(),
                expr->getSubExprAsWritten()->getBeginLoc());
            auto tSource = Lexer::getSourceText(CharSourceRange::getTokenRange(
                expr->getLParenLoc().getLocWithOffset(1),
                expr->getRParenLoc().getLocWithOffset(-1)), *Result.SourceManager,
                Result.Context->getLangOpts());
            auto res = ("static_cast<" + tSource + ">(").str();
            rwr.ReplaceText(loc, res);
            auto closePL = Lexer::getLocForEndOfToken(
                expr->getSubExprAsWritten()->IgnoreImpCasts()->getEndLoc(), 0,
                *Result.SourceManager, Result.Context->getLangOpts());
            rwr.InsertText(closePL, ")");
        }
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
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory);

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
