#include <iostream>
#include <sstream>

#include <llvm/Support/CommandLine.h>

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
    CastCallBack(Rewriter& rewriter): rewriter_(rewriter) {
    };

    virtual void run(const MatchFinder::MatchResult &Result) {
        if (const CStyleCastExpr* ce = Result.Nodes.getNodeAs<CStyleCastExpr>("cast")) {
            if (ce->getExprLoc().isMacroID())
                return;
            
            if (ce->getCastKind() == CK_ToVoid)
                return;

            const auto DestTypeAsWritten = ce->getTypeAsWritten().getUnqualifiedType();
            const auto SourceTypeAsWritten = ce->getSubExprAsWritten()->getType().getUnqualifiedType();
            const auto SourceType = SourceTypeAsWritten.getCanonicalType();
            const auto DestType = DestTypeAsWritten.getCanonicalType();
            
            auto&& SM = *Result.SourceManager;

            auto rangeOfReplacement = 
                CharSourceRange::getCharRange(ce->getLParenLoc(), ce->getSubExprAsWritten()->getBeginLoc());

            auto destTypeStr = Lexer::getSourceText(CharSourceRange::getTokenRange(
                                                    ce->getLParenLoc().getLocWithOffset(1),
                                                    ce->getRParenLoc().getLocWithOffset(-1)),
                                                    SM, Result.Context->getLangOpts());


            auto castFunc = [&](auto&& cast) {
                const Expr* subExpr = ce->getSubExprAsWritten()->IgnoreImpCasts();
                if (!isa<ParenExpr>(subExpr)) {
                    cast.push_back('(');
                    rewriter_.InsertText(Lexer::getLocForEndOfToken(subExpr->getEndLoc(), 0, SM, Result.Context->getLangOpts()), ")");
                }
                rewriter_.ReplaceText(rangeOfReplacement, cast);
            };

            auto constCastCheck = [] (auto SourceType, auto DestType) -> bool {
                while ((SourceType->isPointerType() && DestType->isPointerType()) ||
                       (SourceType->isReferenceType() && DestType->isReferenceType())) {
                    SourceType = SourceType->getPointeeType();
                    DestType = DestType->getPointeeType();
                    if (SourceType.isConstQualified() && !DestType.isConstQualified()) {
                        return (SourceType->isPointerType() == DestType->isPointerType()) &&
                                (SourceType->isReferenceType() == DestType->isReferenceType());
                    }
                }
                return false;  
            };

            if (constCastCheck(SourceType, DestType)) {
                castFunc("const_cast<" + destTypeStr.str()+">");
            } else {
                castFunc("static_cast<" + destTypeStr.str()+">");
            }
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
    CommonOptionsParser OptionsParser(argc, argv, CastMatcherCategory);
    ClangTool Tool(OptionsParser.getCompilations(),
            OptionsParser.getSourcePathList());

    return Tool.run(newFrontendActionFactory<CStyleCheckerFrontendAction>().get());
}