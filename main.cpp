#include <iostream>
#include <string>
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
    Rewriter* rewriter__=nullptr;

public:
    CastCallBack(Rewriter& rewriter) {
        this->rewriter__ = &rewriter;
    };

    void run(const MatchFinder::MatchResult& Result) override {
        const auto* C_STYLE =
                Result.Nodes.getNodeAs<CStyleCastExpr>("cast");

        if (!C_STYLE) {
            return;
        }

        const int OFFSET_FOR_FROM = 1;
        const int OFFSET_FOR_UP_TO = -1;
        
        SourceLocation range_from  =
                C_STYLE->getLParenLoc().getLocWithOffset(OFFSET_FOR_FROM);

        SourceLocation range_up_to =
                C_STYLE->getRParenLoc().getLocWithOffset(OFFSET_FOR_UP_TO);

        const auto TOKEN_RANGE =
                CharSourceRange::getTokenRange(range_from, range_up_to);

        const StringRef CAST_FROM = Lexer::getSourceText(TOKEN_RANGE,
                                                        *Result.SourceManager,
                                                        Result.Context->getLangOpts());

       std::string CAST =
                "static_cast<" + CAST_FROM.str() + ">";

        const auto LOC =
                C_STYLE->getSubExprAsWritten()->IgnoreImpCasts();

        range_from =
                C_STYLE->getLParenLoc();

        range_up_to =
                C_STYLE->getSubExprAsWritten()->getBeginLoc();

        const auto CHAR_RANGE =
                CharSourceRange::getCharRange(range_from, range_up_to);

        CAST += "(";

        this->rewriter__->ReplaceText(CHAR_RANGE, CAST);

        const LangOptions OPTIONS = Result.Context->getLangOpts();
        
        const SourceLocation END_LOC = LOC->getEndLoc();

        const unsigned int OFFSET = 0;

        const SourceLocation END_TOKEN = Lexer::getLocForEndOfToken(END_LOC, 
                                                        OFFSET, 
                                                        *Result.SourceManager, 
                                                        OPTIONS);
        this->rewriter__->InsertText(END_TOKEN , ")");
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
