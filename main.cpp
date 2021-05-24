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
	Rewriter& _rewriter;

public:
    CastCallBack(Rewriter& rewriter) : _rewriter(rewriter) { };

    void run(const MatchFinder::MatchResult &Result) override {
    	// getting whole node
        const CStyleCastExpr *cast_node = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
        // getting type name
        const std::string cast_type = cast_node->getTypeAsWritten().getAsString();
        std::string cast_expr("static_cast<" + cast_type + ">");
        // replace (type) with static_cast<type>
        _rewriter.ReplaceText(SourceRange(cast_node->getLParenLoc(), cast_node->getRParenLoc()), cast_expr);
        // check if expression for cast in brakets, if not - add brakets 
        if (!isa<ParenExpr>(cast_node->getSubExprAsWritten())) {
		_rewriter.InsertText(cast_node->getSubExpr()->getBeginLoc(), "(");
		_rewriter.InsertTextAfterToken(cast_node->getSubExpr()->getEndLoc(), ")");
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
