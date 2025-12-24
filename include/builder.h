#pragma once

#include "ast.h"
#include "tb_exec.h"

#include <expected>

namespace gpamgr {
namespace {
enum class CmpOp { Eq, Ne, Lt, Le, Gt, Ge, Like };
constexpr double EPS = 1e-6;
#define abs(n) ((n) > 0 ? (n) : -(n))
#define float_eq(a, b) (abs(a - b) < EPS)

std::expected<bool, std::string> compare_value(const Table::Value &lhs,
                                               const Table::Value &rhs,
                                               CmpOp op) {
    using FT = Table::FieldType;

    // LIKE: only string
    if (op == CmpOp::Like) {
        if (lhs.type != FT::STRING || rhs.type != FT::STRING) {
            return std::unexpected("`LIKE` can only be used on string");
        }
        return utils::strlike(*lhs.as_string(), *rhs.as_string());
    }

    // numeric comparison: INT / FLOAT
    auto is_numeric = [](FT t) {
        return t == FT::INT || t == FT::FLOAT;
    };

    if (is_numeric(lhs.type) && is_numeric(rhs.type)) {
        double x = (lhs.type == FT::INT) ? static_cast<double>(*lhs.as_int()) : *lhs.as_double();
        double y = (rhs.type == FT::INT) ? static_cast<double>(*rhs.as_int()) : *rhs.as_double();

        switch (op) {
            case CmpOp::Eq: return float_eq(x, y);
            case CmpOp::Ne: return !float_eq(x, y);
            case CmpOp::Lt: return x < y;
            case CmpOp::Le: return x <= y;
            case CmpOp::Gt: return x > y;
            case CmpOp::Ge: return x >= y;
            default: break;
        }
    }

    // string comparison (non-LIKE)
    if (lhs.type == FT::STRING && rhs.type == FT::STRING) {
        auto &x = *lhs.as_string();
        auto &y = *rhs.as_string();
        switch (op) {
            case CmpOp::Eq: return x == y;
            case CmpOp::Ne: return x != y;
            case CmpOp::Lt: return x < y;
            case CmpOp::Le: return x <= y;
            case CmpOp::Gt: return x > y;
            case CmpOp::Ge: return x >= y;
            default: break;
        }
    }

    return std::unexpected("Type mismatch in comparison");
}

std::expected<Value, std::string> apply_arith(BinaryExpr::BinaryOp op,
                                              const Value &lhs,
                                              const Value &rhs) {
    using FieldType = Table::FieldType;
    using BinaryOp = BinaryExpr::BinaryOp;
    if (lhs.type == FieldType::STRING || rhs.type == FieldType::STRING) {
        return std::unexpected("Arithmetic on string is not supported");
    }

    bool is_float = (lhs.type == FieldType::FLOAT || rhs.type == FieldType::FLOAT);

    if (is_float) {
        double x = (lhs.type == FieldType::FLOAT) ? *lhs.as_double() : double(*lhs.as_int());
        double y = (rhs.type == FieldType::FLOAT) ? *rhs.as_double() : double(*rhs.as_int());

        switch (op) {
            case BinaryOp::Add: return Value(x + y);
            case BinaryOp::Sub: return Value(x - y);
            case BinaryOp::Mul: return Value(x * y);
            case BinaryOp::Div:
                if (y == 0) {
                    return std::unexpected("Division by zero");
                }
                return Value(x / y);
            default: return std::unexpected("Invalid arithmetic operator");
        }
    } else {
        int64_t x = *lhs.as_int();
        int64_t y = *rhs.as_int();

        switch (op) {
            case BinaryOp::Add: return Value(x + y);
            case BinaryOp::Sub: return Value(x - y);
            case BinaryOp::Mul: return Value(x * y);
            case BinaryOp::Div:
                if (y == 0) {
                    return std::unexpected("Division by zero");
                }
                return Value(x / y);
            default: return std::unexpected("Invalid arithmetic operator");
        }
    }
}

std::expected<ValueExpr, std::string> build_value(const Expr *E, const Table &tb) {
    using ExprKind = Expr::ExprKind;
    switch (E->get_kind()) {
        case ExprKind::IdentifierExprKind: {
            auto *id = static_cast<const IdentifierExpr *>(E);
            auto idx = tb.field_index(id->name);
            if (!idx)
                return std::unexpected("Unknown column: " + std::string(id->name));

            return ValueExpr{[col = *idx](const RowView &rv) { return rv.cols[col]; }};
        }

        case ExprKind::IntLiteralKind: {
            auto *lit = static_cast<const IntegerLiteral *>(E);
            Table::Value v(lit->value);
            return ValueExpr{[v](const RowView &) { return v; }};
        }

        case ExprKind::FloatLiteralKind: {
            auto *lit = static_cast<const FloatLiteral *>(E);
            Table::Value v(lit->value);
            return ValueExpr{[v](const RowView &) { return v; }};
        }

        case ExprKind::StringLiteralKind: {
            auto *lit = static_cast<const StringLiteral *>(E);
            Table::Value v(std::string(lit->value));
            return ValueExpr{[v](const RowView &) { return v; }};
        }

        case ExprKind::BinaryExprKind: {
            auto *bin = static_cast<const BinaryExpr *>(E);

            auto lhs = build_value(bin->lhs, tb);
            auto rhs = build_value(bin->rhs, tb);
            if (!lhs || !rhs) {
                return std::unexpected(std::move(lhs ? rhs.error() : lhs.error()));
            }

            return ValueExpr{[l = *lhs, r = *rhs, op = bin->op](
                                 const RowView &rv) -> std::expected<Value, std::string> {
                auto lret = l(rv);
                auto rret = r(rv);
                if (!lret || !rret) {
                    return std::unexpected(std::move(rret ? lret.error() : rret.error()));
                }
                auto res = apply_arith(op, *lret, *rret);
                if (!res) {
                    return std::unexpected(std::move(res.error()));
                }
                return *res;
            }};
        }

        case ExprKind::UnaryExprKind: {
            using UnaryOp = UnaryExpr::UnaryOp;
            auto *un = static_cast<const UnaryExpr *>(E);
            auto rhs = build_value(un->rhs, tb);
            if (!rhs) {
                return std::unexpected(std::move(rhs.error()));
            }
            return ValueExpr(
                [r = *rhs, op = un->op](const RowView &rv) -> std::expected<Value, std::string> {
                    auto rret = r(rv);
                    if (!rret) {
                        return std::unexpected(std::move(rret.error()));
                    }
                    auto rval = rret.value();
                    if (rval.is(Table::FieldType::STRING)) {
                        return std::unexpected("Cannot Calcutate value for `STRING`");
                    } else if (rval.is(Table::FieldType::FLOAT)) {
                        int64_t val = std::get<int64_t>(rval.inner);
                        switch (op) {
                            case UnaryOp::Add: {
                                return rval;
                            }
                            case UnaryOp::Sub: {
                                return Value(int64_t(-val));
                            }
                        }
                    } else {
                        double val = std::get<double>(rval.inner);
                        switch (op) {
                            case UnaryOp::Add: {
                                return rval;
                            }
                            case UnaryOp::Sub: {
                                return Value(int64_t(-val));
                            }
                        }
                    }
                    std::abort();
                });
        }

        default: return std::unexpected("Expression not evaluatable to value");
    }
}

std::expected<Predicate, std::string> build_predicate(const Expr *expr, const Table &tb) {
    using BinaryOp = BinaryExpr::BinaryOp;
    using ExprKind = Expr::ExprKind;
    if (!expr) {
        return Predicate{[](const RowView &) { return true; }};
    }

    if (expr->get_kind() == ExprKind::BinaryExprKind) {
        auto *bin = static_cast<const BinaryExpr *>(expr);

        // AND / OR
        if (bin->op == BinaryOp::And || bin->op == BinaryOp::Or) {
            auto lhs = build_predicate(bin->lhs, tb);
            auto rhs = build_predicate(bin->rhs, tb);
            if (!lhs || !rhs) {
                return std::unexpected(lhs ? rhs.error() : lhs.error());
            }

            if (bin->op == BinaryOp::And) {
                return Predicate{
                    [l = *lhs, r = *rhs](const RowView &rv) { return l(rv) && r(rv); }};
            } else {
                return Predicate{
                    [l = *lhs, r = *rhs](const RowView &rv) { return l(rv) || r(rv); }};
            }
        }

        // comparison
        auto lhs = build_value(bin->lhs, tb);
        auto rhs = build_value(bin->rhs, tb);
        if (!lhs || !rhs) {
            return std::unexpected(lhs ? rhs.error() : lhs.error());
        }

        CmpOp cop;
        switch (bin->op) {
            case BinaryOp::Eq: cop = CmpOp::Eq; break;
            case BinaryOp::Ne: cop = CmpOp::Ne; break;
            case BinaryOp::Lt: cop = CmpOp::Lt; break;
            case BinaryOp::Le: cop = CmpOp::Le; break;
            case BinaryOp::Gt: cop = CmpOp::Gt; break;
            case BinaryOp::Ge: cop = CmpOp::Ge; break;
            case BinaryOp::Like: cop = CmpOp::Like; break;
            default: return std::unexpected("Invalid binary operator in WHERE");
        }

        return Predicate{[l = *lhs, r = *rhs, cop](const RowView &rv) {
            auto lret = l(rv);
            auto rret = r(rv);
            if (!lret || !rret) {
                std::cout << (lret ? rret.error() : lret.error()) << '\n';
                return false;
            }
            auto res = compare_value(*lret, *rret, cop);
            return res && *res;
        }};
    }

    return std::unexpected("Invalid WHERE expression");
}

using ValComparator = std::function<bool(const Value &, const Value &)>;
using FieldType = Table::FieldType;

ValComparator build_eq(FieldType ty) {
    switch (ty) {
        case FieldType::INT:
            return [](const Value &a, const Value &b) {
                return *a.as_int() == *b.as_int();
            };
        case FieldType::FLOAT:
            return [](const Value &a, const Value &b) {
                return *a.as_double() == *b.as_double();
            };
        case FieldType::STRING:
            return [](const Value &a, const Value &b) {
                return *a.as_string() == *b.as_string();
            };
    }
    std::abort();
}

ValComparator build_gt(FieldType ty) {
    switch (ty) {
        case FieldType::INT:
            return [](const Value &a, const Value &b) {
                return *a.as_int() > *b.as_int();
            };
        case FieldType::FLOAT:
            return [](const Value &a, const Value &b) {
                return *a.as_double() > *b.as_double();
            };
        case FieldType::STRING:
            return [](const Value &a, const Value &b) {
                return *a.as_string() > *b.as_string();
            };
    }
    std::abort();
}

struct OrderByItem {
    size_t col_index;
    bool asc;
};

RowComparator build_comparator(std::vector<OrderByItem> obc) {
    auto value_cmp = [](const Value &a, const Value &b) -> int {
        switch (a.type) {
            case Table::FieldType::INT: {
                auto x = std::get<int64_t>(a.inner);
                auto y = std::get<int64_t>(b.inner);
                return (x > y) - (x < y);
            }
            case Table::FieldType::FLOAT: {
                auto x = std::get<double>(a.inner);
                auto y = std::get<double>(b.inner);
                return (x > y) - (x < y);
            }
            case Table::FieldType::STRING: {
                auto &x = std::get<std::string>(a.inner);
                auto &y = std::get<std::string>(b.inner);
                if (x < y) {
                    return -1;
                }
                if (x > y) {
                    return 1;
                }
                return 0;
            }
        }
        return 0;
    };

    return [items = std::move(obc), value_cmp](RowView &lhs, RowView &rhs) -> bool {
        for (auto &it: items) {
            const Value &a = lhs[it.col_index];
            const Value &b = rhs[it.col_index];
            int cmp = value_cmp(a, b);
            if (cmp == 0) {
                continue;
            }
            if (it.asc) {
                return cmp < 0;
            } else {
                return cmp > 0;
            }
        }
        return false;
    };
}

}  // namespace

class PlanBuilder : public ASTVisitor<PlanBuilder> {
    PlanBuildContext &ctx;
    const Stmt *root;
    std::string_view src;

    PlanNode *current = nullptr;
    Table *curr_tbl = nullptr;

    // aggregate
    bool has_aggregate = false;

    /// enum class AggKind { Cnt, Avg, Min, Max };

    /// struct AggItem {
    ///     AggKind kind;
    ///     size_t col_idx;
    /// };

    /// std::vector<AggItem> aggs;

    // where
    const Expr *where_expr = nullptr;

    // order by
    std::optional<OrderByClause> order_by;

    // diagnostics
    std::vector<utils::Diagnostic> diags;

    PlanBuilder(PlanBuildContext &ctx, const Stmt *root, std::string_view src) :
        ctx(ctx), root(root), src(src) {}

    utils::Diagnostic emit_error(std::string_view msg, size_t B, size_t E) {
        using Level = utils::Diagnostic::Level;
        return utils::Diagnostic{src, msg, B, E, Level::Error};
    }

public:
    static PlanBuilder create(PlanBuildContext &ctx, const Stmt *root, std::string_view src) {
        return PlanBuilder{ctx, root, src};
    }

    std::expected<PlanNode *, std::vector<utils::Diagnostic>> build() {
        visit(root);
        if (!diags.empty() || current == nullptr) {
            return std::unexpected(diags);
        }
        return current;
    }

    std::expected<std::vector<OrderByItem>, utils::Diagnostic>
        parse_orderby_clause(const OrderByClause &obc) {
        std::vector<OrderByItem> ret{};
        for (auto &cond: obc.keys) {
            if (auto idx = ctx.tb.field_index(cond.column)) {
                OrderByItem obi{.col_index = *idx, .asc = cond.asc};
                ret.emplace_back(obi);
            } else {
                return std::unexpected(emit_error("Cannot find field", obc.B, obc.E));
            }
        }
        return ret;
    }

    bool visitSelect(const SelectStmt *S) {
        // 1. FROM
        auto it = ctx.tb_view.find(S->from->name);
        if (it == ctx.tb_view.end()) {
            auto [B, E] = S->from->src_range();
            diags.emplace_back(emit_error("unknown table", B, E));
            return false;
        }

        curr_tbl = it->second;

        current = ctx.make_plan<TableScanPlan>(it->second);

        // 2. WHERE
        where_expr = S->cond;

        // 3. Parse SELECT LIST
        std::vector<ProjectItem> project_items{};
        std::vector<AggregateItem> agg_items{};
        for (auto *expr: S->select_list) {
            using EK = Expr::ExprKind;

            if (expr->isa(EK::IdentifierExprKind)) {
                auto *id = static_cast<const IdentifierExpr *>(expr);
                auto idx = curr_tbl->field_index(id->name);
                if (!idx) {
                    auto [b, e] = id->src_range();
                    diags.emplace_back(emit_error("unknown column", b, e));
                    return false;
                }

                project_items.push_back(ProjectItem{
                    .kind = ProjectItem::Col,
                    .col = *idx,
                });
                continue;
            }

            if (expr->isa(EK::CallExprKind)) {
                auto *call = static_cast<const CallExpr *>(expr);
                has_aggregate = true;

                AggKind kind;
                if (call->callee->name == "avg")
                    kind = AggKind::Avg;
                else if (call->callee->name == "min")
                    kind = AggKind::Min;
                else if (call->callee->name == "max")
                    kind = AggKind::Max;
                else if (call->callee->name == "count")
                    kind = AggKind::Cnt;
                else {
                    auto [b, e] = call->callee->src_range();
                    diags.emplace_back(emit_error("unknown aggregate function", b, e));
                    return false;
                }

                if (call->args.size() != 1 || !call->args[0]->isa(EK::IdentifierExprKind)) {
                    auto [b, e] = expr->src_range();
                    diags.emplace_back(emit_error("aggregate argument must be a column", b, e));
                    return false;
                }

                auto *id = static_cast<const IdentifierExpr *>(call->args[0]);
                auto idx = curr_tbl->field_index(id->name);
                if (!idx) {
                    auto [b, e] = id->src_range();
                    diags.emplace_back(emit_error("unknown column", b, e));
                    return false;
                }

                agg_items.emplace_back(kind, *idx);
                continue;
            }

            auto [b, e] = expr->src_range();
            diags.emplace_back(emit_error("invalid expression in select list", b, e));
            return false;
        }

        // 4. Semantic
        if (has_aggregate) {
            for (auto &it: project_items) {
                if (it.kind == ProjectItem::Col) {
                    auto [B, E] = S->src_range();
                    diags.emplace_back(
                        emit_error("mixing aggregate and non-aggregate columns without GROUP BY",
                                   B,
                                   E));
                    return false;
                }
            }
        }

        // 5. WHERE -> FilterPlan
        if (where_expr) {
            auto pred = build_predicate(where_expr, *curr_tbl);
            if (!pred.has_value()) {
                auto [b, e] = where_expr->src_range();
                diags.emplace_back(emit_error(pred.error(), b, e));
                return false;
            }
            auto *filter = ctx.make_plan<FilterPlan>(std::move(*pred));
            filter->child.push_back(current);
            current = filter;
        }

        if (has_aggregate) {
            auto *agg = ctx.make_plan<AggregatePlan>(std::move(agg_items));
            agg->child.push_back(current);
            current = agg;
        }

        // 9. ORDER BY
        if (S->sort) {
            auto items = parse_orderby_clause(*S->sort);
            if (!items.has_value()) {
                diags.emplace_back(std::move(items.error()));
                return false;
            }
            auto *order = ctx.make_plan<OrderByPlan>(build_comparator(std::move(*items)));
            order->child.push_back(current);
            current = order;
        }

        // 8. Output / Project fallback
        if (!has_aggregate) {
            std::vector<ProjectItem> index;
            if (project_items.empty()) {
                const auto N = curr_tbl->field_count();
                index.resize(N);
                for (size_t i = 0; i < N; ++i) {
                    index[i] = {.kind = ProjectItem::Col, .col = i};
                }
            } else {
                index = project_items;
            }
            auto *proj = ctx.make_plan<ProjectPlan>(index);
            proj->child.push_back(current);
            current = proj;
        }

        // 9. Output
        return false;
    }

    bool visitInsert(const InsertStmt *S) {
        auto it = ctx.tb_view.find(S->tb_name->name);
        if (it == ctx.tb_view.end()) {
            auto [B, E] = S->tb_name->src_range();
            diags.emplace_back(emit_error("unknown table", B, E));
            return false;
        }

        Table *tb = it->second;
        if (S->values.size() != tb->field_count()) {
            auto [B, E] = S->src_range();
            diags.emplace_back(
                emit_error("number of columns does not match number of values", B, E));
            return false;
        }

        std::vector<Table::Value> values;
        for (size_t i = 0; i < S->values.size(); ++i) {
            auto *expr = S->values[i];
            Value val;
            using EK = Expr::ExprKind;
            switch (expr->get_kind()) {
                case EK::IntLiteralKind: {
                    auto *lit = static_cast<const IntegerLiteral *>(expr);
                    val = Value{lit->value};
                    break;
                }
                case EK::FloatLiteralKind: {
                    auto *lit = static_cast<const FloatLiteral *>(expr);
                    val = Value{lit->value};
                    break;
                }
                case EK::StringLiteralKind: {
                    auto *lit = static_cast<const StringLiteral *>(expr);
                    val = Value{std::string{lit->value}};
                    break;
                }
                default: {
                    auto [B, E] = expr->src_range();
                    diags.emplace_back(
                        emit_error("INSERT values must be constant expressions", B, E));
                    return false;
                }
            }

            auto schema_item = tb->find_field(i);
            if (!schema_item) {
                auto [B, E] = expr->src_range();
                diags.emplace_back(emit_error("invalid column index", B, E));
                return false;
            }
            auto dst = schema_item->type;
            auto src = val.type;

            if (src == dst) {
                values.push_back(std::move(val));
                continue;
            }

            // INT -> FLOAT
            if (src == Table::FieldType::INT && dst == Table::FieldType::FLOAT) {
                values.emplace_back(static_cast<double>(*val.as_int()));
                continue;
            }

            // FLOAT -> INT
            if (src == Table::FieldType::FLOAT && dst == Table::FieldType::INT) {
                values.emplace_back(static_cast<int64_t>(*val.as_double()));
                continue;
            }

            auto [B, E] = expr->src_range();
            diags.emplace_back(emit_error("type mismatch in INSERT value", B, E));
        }

        current = ctx.make_plan<InsertPlan>(tb, values);
        return false;
    }

    bool visitUpdate(const UpdateStmt *S) {
        // Find table
        auto it = ctx.tb_view.find(S->tb_name->name);
        if (it == ctx.tb_view.end()) {
            auto [B, E] = S->tb_name->src_range();
            diags.emplace_back(emit_error("unknown table", B, E));
            return false;
        }

        Table *tbl = it->second;

        // WHERE predicate
        Predicate pred = [](const RowView &) {
            return true;
        };
        if (S->cond) {
            auto p = build_predicate(S->cond, *tbl);
            if (!p) {
                auto [b, e] = S->cond->src_range();
                diags.emplace_back(emit_error(p.error(), b, e));
                return false;
            }
            pred = std::move(*p);
        }

        // assignments
        std::vector<UpdateItem> items;
        for (auto &assign: S->assigns) {
            auto idx = tbl->field_index(assign.field->name);
            if (!idx) {
                auto [b, e] = assign.field->src_range();
                diags.emplace_back(emit_error("unknown column", b, e));
                return false;
            }
            auto expr = build_value(assign.value, *tbl);
            if (!expr) {
                auto [b, e] = assign.value->src_range();
                diags.emplace_back(emit_error(expr.error(), b, e));
                return false;
            }
            items.push_back(UpdateItem{
                .col_idx = *idx,
                .expr = std::move(*expr),
            });
        }

        // emit UPDATE plan directly
        logging::debug("Emit UpdatePlan");
        auto *upd = ctx.make_plan<UpdatePlan>(tbl, std::move(pred), std::move(items));
        current = upd;
        return true;
    }

    bool visitDelete(const DeleteStmt *S) {
        // 1. Find table
        logging::debug("Visit DeleteStmt");
        auto it = ctx.tb_view.find(S->tb_name->name);
        if (it == ctx.tb_view.end()) {
            auto [B, E] = S->tb_name->src_range();
            diags.emplace_back(emit_error("unknown table", B, E));
            return false;
        }

        Table *tbl = it->second;

        // 2. Build WHERE predicate
        Predicate pred = [](const RowView &) {
            return true;
        };
        if (S->cond) {
            auto p = build_predicate(S->cond, *tbl);
            if (!p) {
                auto [b, e] = S->cond->src_range();
                diags.emplace_back(emit_error(p.error(), b, e));
                return false;
            }
            pred = std::move(*p);
        }

        // 3. Emit DeletePlan (terminal plan)
        logging::debug("Emit DeletePlan");
        auto *del = ctx.make_plan<DeletePlan>(tbl, std::move(pred));
        current = del;
        return true;
    }
};
}  // namespace gpamgr
