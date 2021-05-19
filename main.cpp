#include <iostream>
#include <sstream>
#include <string>
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
    Rewriter &m_rewriter;

    bool is_correct_to_change(const CStyleCastExpr* expr) {
        return expr != nullptr;
        // ... may be other rules
    }

public:

    CastCallBack(Rewriter& rewriter) : m_rewriter(rewriter){};

    void run(const MatchFinder::MatchResult &Result) override {
        // get c-style node
        const CStyleCastExpr* c_style_cast = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
        
        if (is_correct_to_change(c_style_cast)) {            
            // get type
            SourceLocation left = c_style_cast->getLParenLoc().getLocWithOffset(1);
            SourceLocation right = c_style_cast->getRParenLoc().getLocWithOffset(-1);
            CharSourceRange range = CharSourceRange::getTokenRange(left, right);
            LangOptions res_opts  = Result.Context->getLangOpts();
            StringRef src_cast_to = Lexer::getSourceText(range, *Result.SourceManager, res_opts);
            
            std::string str_cast_expr= "static_cast<" + src_cast_to.str() + ">";
            auto part = c_style_cast->getSubExprAsWritten()->IgnoreImpCasts();
            SourceLocation loc1 = c_style_cast->getLParenLoc();
            SourceLocation loc2 = c_style_cast->getSubExprAsWritten()->getBeginLoc();
            m_rewriter.ReplaceText(CharSourceRange::getCharRange(loc1, loc2), str_cast_expr + "(");
            LangOptions opts = Result.Context->getLangOpts();
            SourceLocation part_end = part->getEndLoc();
            SourceLocation end = Lexer::getLocForEndOfToken(part_end, 0, *Result.SourceManager, opts);
            m_rewriter.InsertText(end, ")");
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
    CommonOptionsParser Parser(argc, argv, CastMatcherCategory); // ubuntu 18
    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}
