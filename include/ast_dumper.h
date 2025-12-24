#pragma once

#include "ast.h"
#include "misc.h"

#include <iostream>

namespace gpamgr {

class ASTDumper : public ASTVisitor<ASTDumper> {
    std::ostream &os;
    std::vector<char> branch_stack;

    void print_prefix() {
        for (size_t i = 0; i + 1 < branch_stack.size(); ++i) {
            os << (branch_stack[i] ? "   " : "|  ");
        }
        if (!branch_stack.empty()) {
            os << (branch_stack.back() ? "`- " : "|- ");
        }
    }

    struct BranchGuard {
        std::vector<char> &stack;

        BranchGuard(std::vector<char> &s, bool last) : stack(s) {
            stack.push_back(last);
        }

        ~BranchGuard() {
            stack.pop_back();
        }
    };

public:
    explicit ASTDumper(std::ostream &os) : os(os) {}

    // ---------- Stmt ----------
    bool visitSelect(const SelectStmt *S) {
        print_prefix();
        os << utils::StyledText("SelectStmt").cyan().bold() << "\n";

        size_t child_count =
            2 + (S->cond ? 1 : 0) + (S->sort.has_value() ? 1 : 0);  // SelectList, From, [Where]

        size_t idx = 0;

        {
            BranchGuard g(branch_stack, ++idx == child_count);
            print_prefix();
            os << utils::StyledText("SelectList").blue();
            if (S->select_list.empty()) {
                os << utils::StyledText(" (ALL)").yellow().italic();
            }
            os << '\n';

            for (size_t i = 0; i < S->select_list.size(); ++i) {
                BranchGuard gg(branch_stack, i + 1 == S->select_list.size());
                visit(S->select_list[i]);
            }
        }

        {
            BranchGuard g(branch_stack, ++idx == child_count);
            print_prefix();
            os << utils::StyledText("From").blue() << "\n";
            BranchGuard gg(branch_stack, true);
            visit(S->from);
        }

        if (S->cond) {
            BranchGuard g(branch_stack, ++idx == child_count);
            print_prefix();
            os << utils::StyledText("Where").blue() << "\n";
            BranchGuard gg(branch_stack, true);
            visit(S->cond);
        }

        if (S->sort.has_value()) {
            const auto N = S->sort->keys.size();
            BranchGuard g(branch_stack, ++idx == child_count);
            print_prefix();
            os << utils::StyledText("Sort by").green().bold().italic() << '\n';
            for (int i = 0; i < N; ++i) {
                BranchGuard g(branch_stack, i == N - 1);
                print_prefix();
                os << utils::StyledText::format("[{}] {}",
                                                S->sort->keys[i].column,
                                                S->sort->keys[i].asc ? "(ASC)" : "(DESC)")
                          .green()
                          .italic()
                   << '\n';
            }
        }

        return false;
    }

    bool visitUpdate(const UpdateStmt *S) {
        print_prefix();
        os << utils::StyledText("UpdateStmt").cyan().bold() << "\n";

        // tb_name, assigns, [cond]
        size_t child_count = 1 + S->assigns.size() + (S->cond ? 1 : 0);
        size_t idx = 0;

        // table name
        {
            BranchGuard g(branch_stack, ++idx == child_count);
            visit(S->tb_name);
        }

        if (S->cond) {
            BranchGuard g(branch_stack, ++idx == child_count);
            print_prefix();
            os << utils::StyledText("Where").blue() << "\n";
            BranchGuard gg(branch_stack, true);
            visit(S->cond);
        }

        if (!S->assigns.empty()) {
            BranchGuard g(branch_stack, true);
            print_prefix();
            const auto N = S->assigns.size();
            os << utils::StyledText("Assignments").green().bold().italic() << '\n';
            for (int i = 0; i < N; ++i) {
                BranchGuard g(branch_stack, i == N - 1);
                print_prefix();
                os << utils::StyledText::format("Assign [{}]", i).cyan().italic() << '\n';
                {
                    BranchGuard g2(branch_stack, false);
                    visit(S->assigns[i].field);
                }
                {
                    BranchGuard g2(branch_stack, true);
                    visit(S->assigns[i].value);
                }
            }
        }

        return false;
    }

    bool visitInsert(const InsertStmt *S) {
        print_prefix();
        os << utils::StyledText("InsertStmt").cyan().bold() << "\n";

        // tb_name + values
        size_t child_count = 1 + S->values.size();
        size_t idx = 0;

        // table name
        {
            BranchGuard g(branch_stack, ++idx == child_count);
            visit(S->tb_name);
        }

        // values
        for (size_t i = 0; i < S->values.size(); ++i) {
            BranchGuard g(branch_stack, ++idx == child_count);
            visit(S->values[i]);
        }

        return false;
    }

    bool visitDelete(const DeleteStmt *S) {
        print_prefix();
        os << utils::StyledText("DeleteStmt").cyan().bold() << "\n";

        // tb_name, [cond]
        size_t child_count = 1 + (S->cond ? 1 : 0);
        size_t idx = 0;

        // table name
        {
            BranchGuard g(branch_stack, ++idx == child_count);
            visit(S->tb_name);
        }

        // where
        if (S->cond) {
            BranchGuard g(branch_stack, ++idx == child_count);
            visit(S->cond);
        }

        return false;
    }

    // ---------- Expr ----------

    bool visitBinary(const BinaryExpr *E) {
        print_prefix();
        os << utils::StyledText::format("BinaryExpr({})", binop_name(E->op)).yellow().bold()
           << "\n";
        {
            BranchGuard g(branch_stack, false);
            visit(E->lhs);
        }
        {
            BranchGuard g(branch_stack, true);
            visit(E->rhs);
        }
        return false;
    }

    bool visitUnary(const UnaryExpr *E) {
        print_prefix();
        os << utils::StyledText::format("UnaryExpr({})", binop_name(E->op)).yellow().bold() << "\n";
        {
            BranchGuard g(branch_stack, true);
            visit(E->rhs);
        }
        return false;
    }

    bool visitIdentifier(const IdentifierExpr *E) {
        print_prefix();
        os << utils::StyledText::format("Identifier(\"{}\")", E->name).green() << "\n";
        return false;
    }

    bool visitCall(const CallExpr *E) {
        print_prefix();
        os << utils::StyledText("Call Builtin").blue().bold().italic() << "\n";
        {
            BranchGuard callee_branch(branch_stack, false);
            print_prefix();
            os << utils::StyledText("Callee").green().italic() << '\n';
            {
                BranchGuard callee_name_branch(branch_stack, true);
                visit(E->callee);
            }
        }
        const size_t args_count = E->args.size();
        size_t idx = 0;
        {
            BranchGuard g(branch_stack, true);
            print_prefix();
            os << utils::StyledText("Args").green().italic();
            if (!args_count) {
                os << utils::StyledText(" (Empty)").magenta().italic();
            }
            os << '\n';
            for (const auto arg: E->args) {
                BranchGuard gg(branch_stack, ++idx == args_count);
                visit(arg);
            }
        }
        return false;
    }

    bool visitIntLiteral(const IntegerLiteral *E) {
        print_prefix();
        os << utils::StyledText::format("IntLiteral({})", E->value).magenta() << "\n";
        return false;
    }

    bool visitFloatLiteral(const FloatLiteral *E) {
        print_prefix();
        os << utils::StyledText::format("FloatLiteral({})", E->value).magenta() << "\n";
        return false;
    }

    bool visitStringLiteral(const StringLiteral *E) {
        print_prefix();
        os << utils::StyledText::format("StringLiteral(\"{}\")", E->value).magenta() << "\n";
        return false;
    }

private:
    const static char *binop_name(UnaryExpr::UnaryOp op) {
        switch (op) {
            case UnaryExpr::UnaryOp::Add: return "+";
            case UnaryExpr::UnaryOp::Sub: return "-";
            default: std::abort();
        }
    }

    const static char *binop_name(BinaryExpr::BinaryOp op) {
        switch (op) {
            case BinaryExpr::BinaryOp::And: return "And";
            case BinaryExpr::BinaryOp::Or: return "Or";
            case BinaryExpr::BinaryOp::Eq: return "Eq";
            case BinaryExpr::BinaryOp::Ne: return "Ne";
            case BinaryExpr::BinaryOp::Lt: return "Lt";
            case BinaryExpr::BinaryOp::Le: return "Le";
            case BinaryExpr::BinaryOp::Gt: return "Gt";
            case BinaryExpr::BinaryOp::Ge: return "Ge";
            case BinaryExpr::BinaryOp::Add: return "Add";
            case BinaryExpr::BinaryOp::Sub: return "Sub";
            case BinaryExpr::BinaryOp::Mul: return "Mul";
            case BinaryExpr::BinaryOp::Div: return "Div";
            case BinaryExpr::BinaryOp::Like: return "Like";
        }
        return "Unknown";
    }
};
}  // namespace gpamgr
