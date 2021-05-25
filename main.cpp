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
    Rewriter& tool;

 public:
    CastCallBack(Rewriter& rewriter) : tool(rewriter) {};
    void run(const MatchFinder::MatchResult &Result) override {
        const auto* node_ = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
        auto range_ch = CharSourceRange::getCharRange(node_->getLParenLoc(),
        node_->getSubExprAsWritten()->getBeginLoc());

        StringRef type_ch = Lexer::getSourceText(CharSourceRange::getTokenRange(
        node_->getLParenLoc().getLocWithOffset(1),
        node_->getRParenLoc().getLocWithOffset(-1)),
        *Result.SourceManager, Result.Context->getLangOpts());

        std::string changed = ("static_cast<" + type_ch + ">").str();
        if (!isa<ParenExpr>(node_->getSubExprAsWritten())) {
            changed.push_back('(');
            tool.InsertText(Lexer::getLocForEndOfToken(node_->getSubExprAsWritten()->getEndLoc(), 0, *Result.SourceManager, Result.Context->getLangOpts()), ")");
        }
        tool.ReplaceText(range_ch, changed);
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
