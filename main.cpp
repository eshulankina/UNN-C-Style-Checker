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
    Rewriter& rewriter;

    CastCallBack(Rewriter& rewriter): rewriter(rewriter) {};

    void run(const MatchFinder::MatchResult &Result) override {    
        if(const auto *CastExpr = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")){
            const auto& SM = *Result.SourceManager;
            const auto& Loc = CastExpr->getExprLoc();
            StringRef type = Lexer::getSourceText(
                CharSourceRange::getTokenRange(
                        CastExpr->getLParenLoc().getLocWithOffset(1),
                        CastExpr->getRParenLoc().getLocWithOffset(-1)
                ),
                SM, 
                rewriter.getLangOpts()
            );
            StringRef param = Lexer::getSourceText(
                CharSourceRange::getTokenRange(
                        CastExpr->getRParenLoc().getLocWithOffset(1),
                        CastExpr->getEndLoc()
                ),
                SM, 
                rewriter.getLangOpts()
            );
            std::string result = std::string("static_cast<")+type.str()+std::string(">(") + param.str() +std::string(")");
            rewriter.ReplaceText(CharSourceRange::getTokenRange(
                        CastExpr->getBeginLoc(),
                        CastExpr->getEndLoc()
                ),result);
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
