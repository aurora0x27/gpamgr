#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <memory>
#include <span>

namespace gpamgr {

class ASTContext;

class Expr {
public:
    enum class ExprKind : unsigned short {
        BinaryExprKind,
        UnaryExprKind,
        IntLiteralKind,
        FloatLiteralKind,
        StringLiteralKind,
        IdentifierExprKind,
        CallExprKind,
        ExprKindCount
    };

    Expr(ExprKind kd, size_t B, size_t E) : kind(kd), B(B), E(E) {}

    const ExprKind get_kind() const {
        return kind;
    }

    bool isa(ExprKind kd) const {
        return kind == kd;
    }

    std::pair<size_t, size_t> src_range() const {
        return {B, E};
    }

    virtual const bool is_literal() const = 0;

    virtual ~Expr() = default;

private:
    const ExprKind kind;
    const size_t B;
    const size_t E;
};

class BinaryExpr final : public Expr {
public:
    enum class BinaryOp { And, Or, Eq, Ne, Lt, Le, Gt, Ge, Like, Add, Sub, Mul, Div };

    BinaryExpr(BinaryOp op, Expr *lhs, Expr *rhs, size_t B, size_t E) :
        Expr(ExprKind::BinaryExprKind, B, E), op(op), lhs(lhs), rhs(rhs) {}

    BinaryOp op;
    Expr *lhs;
    Expr *rhs;

    const bool is_literal() const override {
        return false;
    }
};

class UnaryExpr final : public Expr {
public:
    enum class UnaryOp { Add, Sub };
    UnaryOp op;
    Expr *rhs;

    UnaryExpr(UnaryOp op, Expr *rhs, size_t B, size_t E) :
        Expr(Expr::ExprKind::UnaryExprKind, B, E), op(op), rhs(rhs) {}

    const bool is_literal() const override {
        return rhs->is_literal();
    }
};

class FloatLiteral final : public Expr {
public:
    FloatLiteral(double val, size_t B, size_t E) :
        Expr(ExprKind::FloatLiteralKind, B, E), value(val) {}

    double value;

    const bool is_literal() const override {
        return true;
    }
};

class IntegerLiteral final : public Expr {
public:
    IntegerLiteral(int64_t val, size_t B, size_t E) :
        Expr(ExprKind::IntLiteralKind, B, E), value(val) {}

    const int64_t value;

    const bool is_literal() const override {
        return true;
    }
};

class StringLiteral final : public Expr {
public:
    StringLiteral(std::string_view val, size_t B, size_t E) :
        Expr(ExprKind::StringLiteralKind, B, E), value(val) {}

    const std::string_view value;

    const bool is_literal() const override {
        return true;
    }
};

class IdentifierExpr final : public Expr {
public:
    IdentifierExpr(std::string_view name, size_t B, size_t E) :
        Expr(ExprKind::IdentifierExprKind, B, E), name(name) {}

    std::string_view name;

    const bool is_literal() const override {
        return false;
    }
};

class CallExpr final : public Expr {
public:
    const std::vector<Expr *> args;
    const IdentifierExpr *callee;

    CallExpr(IdentifierExpr *callee, std::span<Expr *> args, size_t B, size_t E) :
        Expr(Expr::ExprKind::CallExprKind, B, E), callee(callee), args(args.begin(), args.end()) {}

    const bool is_literal() const override {
        return false;
    }
};

struct OrderKey {
    std::string_view column;
    bool asc;
};

struct OrderByClause {
    size_t B;
    size_t E;
    std::vector<OrderKey> keys;
};

class InsertStmt;
class SelectStmt;
class UpdateStmt;
class DeleteStmt;

class Stmt {
public:
    enum class StmtKind : unsigned short {
        InsertStmtKind,
        SelectStmtKind,
        UpdateStmtKind,
        DeleteStmtKind,
        StmtKindCount
    };

    Stmt(StmtKind kd, size_t B, size_t E) : kind(kd), B(B), E(E) {}

    virtual ~Stmt() = default;

    const StmtKind get_kind() const {
        return kind;
    }

    std::pair<size_t, size_t> src_range() const {
        return {B, E};
    }

    bool isa(StmtKind kd) const {
        return kind == kd;
    }

private:
    const StmtKind kind;
    const size_t B;
    const size_t E;
};

/// select_stmt
///     ::= SELECT select_list
///         FROM identifier
///         [ WHERE condition ]
///         [ ORDER BY order_list ]
///         ;
class SelectStmt final : public Stmt {
public:
    SelectStmt(std::span<Expr *> sl,
               const IdentifierExpr *from,
               const Expr *where,
               std::optional<OrderByClause> order_by,
               size_t B,
               size_t E) :
        Stmt(Stmt::StmtKind::SelectStmtKind, B, E), select_list(sl.begin(), sl.end()), from(from),
        cond(where), sort(order_by) {}

    const std::vector<Expr *> select_list;
    const IdentifierExpr *from;
    const Expr *cond;
    std::optional<OrderByClause> sort;
};

/// insert_stmt
///     ::= INSERT INTO identifier
///         VALUES "(" value_list ")"
///         ;
class InsertStmt final : public Stmt {
public:
    InsertStmt(const IdentifierExpr *into, std::span<Expr *> values, size_t B, size_t E) :
        Stmt(Stmt::StmtKind::InsertStmtKind, B, E), tb_name(into),
        values(values.begin(), values.end()) {}

    const IdentifierExpr *tb_name;
    const std::vector<const Expr *> values;
};

struct Assignment {
    const IdentifierExpr *field;
    const Expr *value;
};

/// update_stmt
///     ::= UPDATE identifier
///         SET identifier "=" value
///         [ WHERE condition ]
///         ;
class UpdateStmt final : public Stmt {
public:
    UpdateStmt(const IdentifierExpr *table,
               std::span<Assignment> assignments,
               const Expr *where,
               size_t B,
               size_t E) :
        Stmt(Stmt::StmtKind::UpdateStmtKind, B, E), tb_name(table),
        assigns(std::vector<Assignment>{assignments.begin(), assignments.end()}), cond(where) {}

    const IdentifierExpr *tb_name;
    const std::vector<Assignment> assigns;
    const Expr *cond;
};

/// delete_stmt
///     ::= DELETE FROM identifier
///         [ WHERE condition ]
///         ;
class DeleteStmt final : public Stmt {
public:
    DeleteStmt(const IdentifierExpr *table, const Expr *where, size_t B, size_t E) :
        Stmt(Stmt::StmtKind::DeleteStmtKind, B, E), tb_name(table), cond(where) {}

    const IdentifierExpr *tb_name;
    const Expr *cond;
};

class ASTContext {
    friend class Parser;

    std::vector<std::unique_ptr<Stmt>> stmt_pool;
    std::vector<std::unique_ptr<Expr>> expr_pool;

    std::vector<Stmt *> stmts;

    void add_stmt(Stmt *S) {
        stmts.push_back(S);
    }

public:
    template <typename StmtTy, typename... Args>
    StmtTy *make_stmt(Args &&...args) {
        auto own_ptr = std::make_unique<StmtTy>(std::forward<Args>(args)...);
        StmtTy *raw = own_ptr.get();
        stmt_pool.emplace_back(std::move(own_ptr));
        return raw;
    }

    template <typename ExprTy, typename... Args>
    ExprTy *make_expr(Args &&...args) {
        auto own_ptr = std::make_unique<ExprTy>(std::forward<Args>(args)...);
        ExprTy *raw = own_ptr.get();
        expr_pool.emplace_back(std::move(own_ptr));
        return raw;
    }

    std::span<const Stmt *const> get_stmts() const {
        return {stmts.data(), stmts.size()};
    }
};

template <typename Derived>
class ASTVisitor {
#define derived_this (static_cast<Derived *>(this))

public:
    void visit(const Stmt *S) {
        switch (S->get_kind()) {
            case Stmt::StmtKind::SelectStmtKind:
                derived_this->visitSelect(static_cast<const SelectStmt *>(S));
                break;
            case Stmt::StmtKind::InsertStmtKind:
                derived_this->visitInsert(static_cast<const InsertStmt *>(S));
                break;
            case Stmt::StmtKind::UpdateStmtKind:
                derived_this->visitUpdate(static_cast<const UpdateStmt *>(S));
                break;
            case Stmt::StmtKind::DeleteStmtKind:
                derived_this->visitDelete(static_cast<const DeleteStmt *>(S));
                break;
            default: std::abort();
        }
    }

    void visit(const Expr *E) {
        switch (E->get_kind()) {
            case Expr::ExprKind::BinaryExprKind:
                derived_this->visitBinary(static_cast<const BinaryExpr *>(E));
                break;
            case Expr::ExprKind::UnaryExprKind:
                derived_this->visitUnary(static_cast<const UnaryExpr *>(E));
                break;
            case Expr::ExprKind::IntLiteralKind:
                derived_this->visitIntLiteral(static_cast<const IntegerLiteral *>(E));
                break;
            case Expr::ExprKind::FloatLiteralKind:
                derived_this->visitFloatLiteral(static_cast<const FloatLiteral *>(E));
                break;
            case Expr::ExprKind::StringLiteralKind:
                derived_this->visitStringLiteral(static_cast<const StringLiteral *>(E));
                break;
            case Expr::ExprKind::CallExprKind:
                derived_this->visitCall(static_cast<const CallExpr *>(E));
                break;
            case Expr::ExprKind::IdentifierExprKind:
                derived_this->visitIdentifier(static_cast<const IdentifierExpr *>(E));
                break;
            default: std::abort();
        }
    }

protected:
    bool visitSelect(const SelectStmt *S) {
        return true;
    }

    bool visitInsert(const InsertStmt *S) {
        return true;
    }

    bool visitUpdate(const UpdateStmt *S) {
        return true;
    }

    bool visitDelete(const DeleteStmt *S) {
        return true;
    }

    bool visitBinary(const BinaryExpr *E) {
        return true;
    }

    bool visitUnary(const UnaryExpr *E) {
        return true;
    }

    bool visitIntLiteral(const IntegerLiteral *) {
        return true;
    }

    bool visitFloatLiteral(const FloatLiteral *) {
        return true;
    }

    bool visitStringLiteral(const StringLiteral *) {
        return true;
    }

    bool visitIdentifier(const IdentifierExpr *) {
        return true;
    }

    bool visitCall(const CallExpr *) {
        return true;
    }

    void traverseSelect(const SelectStmt *S) {
        for (auto *col: S->select_list) {
            derived_this->visit(col);
        }

        derived_this->visit(S->from);

        if (S->cond) {
            derived_this->visit(S->cond);
        }
    }

    void traverseInsert(const InsertStmt *S) {
        derived_this->visit(S->tb_name);

        for (auto *v: S->values) {
            derived_this->visit(v);
        }
    }

    void traverseUpdate(const UpdateStmt *S) {
        derived_this->visit(S->tb_name);
        for (auto &assign: S->assigns) {
            derived_this->visit(assign.field);
            derived_this->visit(assign.value);
        }
        if (S->cond) {
            derived_this->visit(S->cond);
        }
    }

    void traverseDelete(const DeleteStmt *S) {
        derived_this->visit(S->tb_name);
        if (S->cond) {
            derived_this->visit(S->cond);
        }
    }

    void traverseBinary(const BinaryExpr *E) {
        static_cast<Derived *>(this)->visit(E->lhs);
        static_cast<Derived *>(this)->visit(E->rhs);
    }

    void traverseUnary(const UnaryExpr *E) {
        static_cast<Derived *>(this)->visit(E->rhs);
    }

#undef derived_this
};

}  // namespace gpamgr
