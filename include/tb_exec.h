#pragma once

#include "table.h"
#include "misc.h"

#include <memory>
#include <string>
#include <expected>
#include <string_view>
#include <algorithm>

namespace gpamgr {
class PlanBuildContext;

using Value = Table::Value;

struct RowView {
    const Table *table;
    RowId row_id;
    std::span<const Value> cols;
    std::shared_ptr<const std::vector<Value>> owner = nullptr;

    const Value &operator[] (size_t i) const {
        return cols[i];
    }

    size_t size() const {
        return cols.size();
    }
};

using ValueExpr = std::function<std::expected<Value, std::string>(const RowView &)>;

using RowConsumer = std::function<void(RowView)>;

class ExecContext {
public:
    using Consumer = std::function<void(RowView)>;

private:
    Consumer consumer;
    bool failed = false;
    std::string error;

public:
    ExecContext() = default;

    explicit ExecContext(Consumer c) : consumer(std::move(c)) {}

    void emit(RowView rv) {
        if (!failed && consumer) {
            consumer(std::move(rv));
        }
    }

    void fail(std::string msg) {
        if (!failed) {
            failed = true;
            error = std::move(msg);
        }
    }

    [[nodiscard]] bool has_failed() const {
        return failed;
    }

    [[nodiscard]] std::string_view error_msg() const {
        return error;
    }

    [[nodiscard]] ExecContext with_consumer(Consumer c) const {
        ExecContext next = *this;
        next.consumer = std::move(c);
        return next;
    }
};

class PlanNode {
    friend class PlanBuildContext;
    friend class PlanBuilder;

protected:
    std::vector<PlanNode *> child;

public:
    virtual ~PlanNode() = default;
    virtual void execute(ExecContext &) const = 0;

    virtual void dump(std::ostream &os, bool color) const = 0;

    void explain(std::ostream &os, bool color, int indent = 0) const {
        // indent
        for (int i = 0; i < indent; ++i) {
            if (i == indent - 1) {
                os << "`- ";
            } else {
                os << "   ";
            }
        }

        dump(os, color);

        for (auto *c: child) {
            if (c) {
                c->explain(os, color, indent + 1);
            }
        }
    }
};

class TableScanPlan final : public PlanNode {
    const Table *table;

public:
    explicit TableScanPlan(const Table *t) : table(t) {}

    void execute(ExecContext &ctx) const override {
        table->scan([&](const Table::Row &row) {
            RowView rv{.table = table,
                       .row_id = row.id,
                       .cols = std::span<const Value>(row.content)};
            ctx.emit(rv);
        });
    }

    void dump(std::ostream &os, bool) const override {
        os << "TableScan(" << table->get_name() << ")\n";
    }
};

using Predicate = std::function<bool(const RowView &)>;

class FilterPlan final : public PlanNode {
    Predicate pred;

public:
    explicit FilterPlan(Predicate p) : pred(std::move(p)) {}

    void execute(ExecContext &ctx) const override {
        auto next = ctx.with_consumer([&](const RowView &rv) {
            if (pred(rv)) {
                ctx.emit(rv);
            }
        });
        child[0]->execute(next);
    }

    void dump(std::ostream &os, bool) const override {
        os << "Filter\n";
    }
};

class OutputPlan final : public PlanNode {
    std::ostream &os;

public:
    explicit OutputPlan(std::ostream &os) : os(os) {}

    void execute(ExecContext &ctx) const override {
        auto next = ctx.with_consumer([&](const RowView &rv) {
            for (size_t i = 0; i < rv.size(); ++i) {
                rv[i].display(os);
                if (i + 1 < rv.size()) {
                    os << "|";
                }
            }
            os << '\n';
        });
        child[0]->execute(next);
    }

    void dump(std::ostream &os, bool) const override {
        os << "Print Results\n";
    }
};

struct ProjectItem {
    enum ProjectionKind { Avg, Max, Min, Col } kind;

    // ValueExpr expr;
    size_t col;
};

class ProjectPlan final : public PlanNode {
    std::vector<ProjectItem> indices;

public:
    explicit ProjectPlan(std::vector<ProjectItem> idx) : indices(std::move(idx)) {}

    void execute(ExecContext &ctx) const override {
        auto next = ctx.with_consumer([&](const RowView &rv) {
            auto owned = std::make_shared<std::vector<Value>>();
            owned->reserve(indices.size());
            for (auto i: indices) {
                owned->push_back(rv[i.col]);
            }
            RowView out{
                .table = nullptr,
                .row_id = rv.row_id,
                .cols = std::span<const Value>(*owned),
                .owner = owned,
            };
            ctx.emit(out);
        });
        child[0]->execute(next);
    }

    void dump(std::ostream &os, bool) const override {
        os << "Project\n";
    }
};

class InsertPlan final : public PlanNode {
    Table *table;
    std::vector<Table::Value> values;

public:
    explicit InsertPlan(Table *tb, std::vector<Table::Value> vals) :
        table(tb), values(std::move(vals)) {}

    void execute(ExecContext &ctx) const override {
        if (auto ret = table->insert(values); !ret) {
            ctx.fail(ret.error());
        }
    }

    void dump(std::ostream &os, bool) const override {
        os << "Insert Into (" << table->get_name() << ")\n";
    }
};

struct UpdateItem {
    size_t col_idx;
    std::function<std::expected<Value, std::string>(const RowView &)> expr;
};

class UpdatePlan final : public PlanNode {
    Table *table;
    Predicate cond;
    std::vector<UpdateItem> diffs;

public:
    UpdatePlan(Table *tb, Predicate cond, std::vector<UpdateItem> diffs) :
        table(tb), cond(std::move(cond)), diffs(std::move(diffs)) {}

    void execute(ExecContext &ctx) const override {
        logging::debug("Doing Update plan");

        std::vector<RowId> targets;
        targets.reserve(16);

        // Phase1: collect diffs
        table->scan_struct([&](Table::Row &row) -> Table::ScanAction {
            if (ctx.has_failed()) {
                return Table::ScanAction::Stop;
            }

            RowView rv{
                .table = table,
                .row_id = row.id,
                .cols = row.content,  // read-only use
            };

            if (cond(rv)) {
                targets.push_back(row.id);
            }

            return Table::ScanAction::Keep;
        });

        if (ctx.has_failed()) {
            return;
        }

        // Phase2: apply diffs
        for (auto row_id: targets) {
            auto row_ = table->find_by_id(row_id);
            if (!row_) {
                continue;
            }
            Table::Row *row = *row_;

            RowView rv{
                .table = table,
                .row_id = row->id,
                .cols = row->content,
            };

            std::stringstream ss;
            table->dump_row(ss, row->id);
            logging::debug("Update row: {}", ss.str());

            for (auto &d: diffs) {
                auto val = d.expr(rv);
                if (!val) {
                    ctx.fail(val.error());
                    return;
                }

                auto &dst = row->content[d.col_idx];
                auto dst_ty = dst.type;
                auto src_ty = val->type;

                if (src_ty == dst_ty) {
                    dst = std::move(*val);
                    continue;
                }

                if (dst_ty == Table::FieldType::FLOAT && src_ty == Table::FieldType::INT) {

                    double promoted = static_cast<double>(*val->as_int());
                    dst = Table::Value{double(promoted)};
                    continue;
                }

                if (src_ty == Table::FieldType::FLOAT && dst_ty == Table::FieldType::INT) {

                    int64_t promoted = static_cast<int64_t>(*val->as_double());
                    dst = Table::Value{int64_t(promoted)};
                    continue;
                }
                ctx.fail(std::format("Type mismatch on column `{}` ({} <- {})",
                                     table->get_schema()[d.col_idx].name,
                                     Table::field_ty_as_string(dst_ty),
                                     Table::field_ty_as_string(src_ty)));
                return;
            }
        }
    }

    void dump(std::ostream &os, bool) const override {
        os << "Update table (" << table->get_name() << ")\n";
    }
};

class DeletePlan final : public PlanNode {
    Table *table;
    Predicate cond;

public:
    explicit DeletePlan(Table *tbl, Predicate cond) : table(tbl), cond(std::move(cond)) {}

    void execute(ExecContext &ctx) const override {
        logging::debug("Doing Delete plan");
        table->scan_struct([&](Table::Row &row) -> Table::ScanAction {
            if (ctx.has_failed()) {
                logging::debug("Fail on row: {}", row.id);
                return Table::ScanAction::Stop;
            }

            RowView rv{
                .table = table,
                .row_id = row.id,
                .cols = row.content,
            };

            if (cond(rv)) {
                std::stringstream ss;
                table->dump_row(ss, row.id);
                logging::debug("Will delete row: {}", ss.str());
                return Table::ScanAction::Delete;
            }

            return Table::ScanAction::Keep;
        });
    }

    void dump(std::ostream &os, bool) const override {
        os << "Delete (" << table->get_name() << ")\n";
    }
};

using RowComparator = std::function<bool(RowView &, RowView &)>;

class OrderByPlan final : public PlanNode {
    RowComparator comp;

public:
    explicit OrderByPlan(RowComparator cmp) : comp(std::move(cmp)) {}

    void execute(ExecContext &ctx) const override {
        std::vector<RowView> rows;

        auto collect = [&](const RowView &rv) {
            rows.push_back(rv);
        };

        auto next = ctx.with_consumer(collect);

        for (auto *c: child) {
            c->execute(next);
        }

        std::sort(rows.begin(), rows.end(), comp);

        for (auto &rv: rows) {
            ctx.emit(rv);
        }
    }

    void dump(std::ostream &os, bool) const override {
        os << "Sort Selected\n";
    }
};

enum AggKind {
    Max,
    Min,
    Avg,
    Cnt,
};

struct Acc {
    AggKind kind;
    double dval = 0;
    int64_t ival = 0;
    size_t count = 0;
};

struct AggregateItem {
    AggKind kind;
    size_t col;
};

class AggregatePlan final : public PlanNode {
    std::vector<AggregateItem> items;

public:
    explicit AggregatePlan(std::vector<AggregateItem> items) : items(std::move(items)) {}

    void execute(ExecContext &ctx) const override {
        std::vector<Acc> accs;
        accs.reserve(items.size());

        for (auto &it: items) {
            Acc a{};
            a.kind = it.kind;
            if (it.kind == AggKind::Min)
                a.dval = std::numeric_limits<double>::max();
            if (it.kind == AggKind::Max)
                a.dval = std::numeric_limits<double>::lowest();
            accs.push_back(a);
        }

        auto sub = ctx.with_consumer([&](const RowView &rv) {
            for (size_t i = 0; i < items.size(); ++i) {
                auto &it = items[i];
                auto &a = accs[i];

                if (it.kind != AggKind::Cnt) {
                    auto *num = rv[it.col].as_double();
                    auto *num_i = rv[it.col].as_int();
                    if (!num && !num_i) {
                        ctx.fail("aggregate expects numeric column");
                        return;
                    }
                    double val = num ? *num : double(*num_i);
                    switch (it.kind) {
                        case AggKind::Avg:
                            a.dval += val;
                            a.count++;
                            break;
                        case AggKind::Min: a.dval = std::min(a.dval, val); break;
                        case AggKind::Max: a.dval = std::max(a.dval, val); break;
                        default: break;
                    }
                } else {
                    a.count++;
                }
            }
        });

        child[0]->execute(sub);

        auto owned = std::make_shared<std::vector<Value>>();
        owned->reserve(items.size());

        for (size_t i = 0; i < items.size(); ++i) {
            auto &it = items[i];
            auto &a = accs[i];

            switch (it.kind) {
                case AggKind::Avg: owned->emplace_back(a.count ? a.dval / a.count : 0.0); break;
                case AggKind::Cnt: owned->emplace_back(int64_t(a.count)); break;
                case AggKind::Min:
                case AggKind::Max: owned->emplace_back(a.dval); break;
            }
        }

        ctx.emit(RowView{
            .table = nullptr,
            .row_id = 0,
            .cols = std::span<const Value>(*owned),
            .owner = owned,
        });
    }

    void dump(std::ostream &os, bool) const override {
        auto kd2str = [](AggKind kd) {
            switch (kd) {
                case Max: return "Max";
                case Min: return "Min";
                case Avg: return "Avg";
                case Cnt: return "Cnt";
            }
            std::abort();
        };
        std::cout << "AggregatePlan(";
        for (auto &it: items) {
            std::cout << kd2str(it.kind) << ',';
        }
        std::cout << ")\n";
    }
};

class PlanBuildContext {
    friend class PlanNode;
    friend class PlanBuilder;

    Table &tb;
    TableView tb_view;

    // Batch
    std::vector<const PlanNode *> batch;

    // pool
    std::vector<std::unique_ptr<PlanNode>> pool;

public:
    PlanBuildContext(Table &table, TableView tb_view) : tb(table), tb_view(std::move(tb_view)) {}

    void execute() {
        ExecContext ctx;
        execute_with_ctx(ctx);
    }

    void execute_with_ctx(ExecContext &ctx) {
        for (auto *plan: batch) {
            if (!plan) {
                logging::debug("Hit a nullptr in plan batch");
                continue;
            }
            plan->execute(ctx);
            if (ctx.has_failed()) {
                break;
            }
        }
    }

    void clear() {
        batch.clear();
    }

    void explain(std::ostream &os, bool color);

    std::expected<void, std::vector<utils::Diagnostic>> append_sql(std::string_view sql);

    template <typename PT, typename... Args>
    PT *make_plan(Args &&...args) {
        auto own_ptr = std::make_unique<PT>(std::forward<Args>(args)...);
        PT *ptr = own_ptr.get();
        pool.push_back(std::move(own_ptr));
        return ptr;
    }

private:
    std::expected<const PlanNode *, std::vector<utils::Diagnostic>>
        build_plan(std::string_view sql);
};

}  // namespace gpamgr
