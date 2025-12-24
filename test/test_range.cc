#include "ast.h"

#include "sql.h"
#include "misc.h"
#include "test/test.h"

namespace ut {
using namespace gpamgr;

class RangeDumper : public ASTVisitor<RangeDumper> {
private:
    std::string src;

    void emit_note(std::string msg, size_t B, size_t E) {
        utils::Diagnostic(src, msg, B, E, utils::Diagnostic::Level::Note).display();
    }

public:
    RangeDumper(std::string_view src) : src(src) {}

    bool visitSelect(const SelectStmt *S) {
        auto [b, e] = S->src_range();
        emit_note("Visit SelectStmt", b, e);
        visit(S->from);
        if (S->cond) {
            visit(S->cond);
        }
        for (auto item: S->select_list) {
            visit(item);
        }
        return true;
    }

    bool visitInsert(const InsertStmt *S) {
        auto [b, e] = S->src_range();
        emit_note("Visit InsertStmt", b, e);
        visit(S->tb_name);
        for (auto val: S->values) {
            visit(val);
        }
        return true;
    }

    bool visitUpdate(const UpdateStmt *S) {
        auto [b, e] = S->src_range();
        emit_note("Visit UpdateStmt", b, e);
        visit(S->tb_name);
        visit(S->cond);
        return true;
    }

    bool visitDelete(const DeleteStmt *S) {
        auto [b, e] = S->src_range();
        emit_note("Visit DeleteStmt", b, e);
        visit(S->tb_name);
        visit(S->cond);
        return true;
    }

    bool visitBinary(const BinaryExpr *E) {
        auto [b, e] = E->src_range();
        emit_note("Visit BinaryExpr", b, e);
        visit(E->lhs);
        visit(E->rhs);
        return true;
    }

    bool visitUnary(const UnaryExpr *E) {
        auto [b, e] = E->src_range();
        emit_note("Visit UnaryExpr", b, e);
        visit(E->rhs);
        return true;
    }

    bool visitIntLiteral(const IntegerLiteral *L) {
        auto [b, e] = L->src_range();
        emit_note("Visit IntegerLiteral", b, e);
        return true;
    }

    bool visitFloatLiteral(const FloatLiteral *L) {
        auto [b, e] = L->src_range();
        emit_note("Visit FloatLiteral", b, e);
        return true;
    }

    bool visitStringLiteral(const StringLiteral *L) {
        auto [b, e] = L->src_range();
        emit_note("Visit StringLiteral", b, e);
        return true;
    }

    bool visitIdentifier(const IdentifierExpr *I) {
        auto [b, e] = I->src_range();
        emit_note("Visit IdentifierExpr", b, e);
        return true;
    }

    bool visitCall(const CallExpr *C) {
        auto [b, e] = C->src_range();
        emit_note("Visit CallExpr", b, e);
        visit(C->callee);
        for (auto arg: C->args) {
            visit(arg);
        }
        return true;
    }

    void dump_ranges() {
        auto lexed = lex(src);
        if (!lexed) {
            lexed.error().display();
            return;
        }
        for (auto &tk: lexed.value()) {
            if (tk.ty != gpamgr::TokenType::tk_eof) {
                emit_note("Got token", tk.B, tk.E);
            }
        }
        auto parser = Parser::create(lexed.value(), src);
        auto errs = parser.parse();
        if (!errs.empty()) {
            for (auto &err: errs) {
                err.display();
            }
            return;
        }
        visit(parser.context().get_stmts()[0]);
    }
};

suite<"source range"> src_range = [] {
    RangeDumper("select * from exam where sid > 1000;").dump_ranges();
};
}  // namespace ut
