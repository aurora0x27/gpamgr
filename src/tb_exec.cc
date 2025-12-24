#include "tb_exec.h"

#include "sql.h"
#include "tb_exec.h"
#include "builder.h"
#include "ast_dumper.h"

namespace gpamgr {
void PlanBuildContext::explain(std::ostream &os, bool color) {
    for (auto *plan: batch) {
        if (!plan) {
            continue;
        }
        plan->explain(os, color);
    }
}

std::expected<void, std::vector<utils::Diagnostic>>
    PlanBuildContext::append_sql(std::string_view sql) {
    if (auto plan = build_plan(sql); plan.has_value() && plan.value()) {
        logging::trace("Successfully compiled command");
        assert(plan.value());
        batch.emplace_back(plan.value());
        return {};
    } else {
        logging::trace("Error occurred when compiling command");
        return std::unexpected(std::move(plan.error()));
    }
}

std::expected<const PlanNode *, std::vector<utils::Diagnostic>>
    PlanBuildContext::build_plan(std::string_view sql) {
    auto lexed = lex(sql);
    if (!lexed.has_value()) {
        logging::debug("lexer error:\n`{}`\n", lexed.error().to_string());
        return std::unexpected(std::vector<utils::Diagnostic>{std::move(lexed.error())});
    }

    logging::trace("Begin to parse token stream");
    auto parser = Parser::create(lexed.value(), sql);
    auto err = parser.parse();
    if (!err.empty()) {
        logging::debug("Cannot parse");
        return std::unexpected(std::move(err));
    }

    logging::trace("Begin to generate plan");
    if (spdlog::get_level() <= spdlog::level::level_enum::debug) {
        std::cout << utils::StyledText("\nASTDump\n").bold();
        ASTDumper(std::cout).visit(parser.context().get_stmts()[0]);
    }
    auto builder = PlanBuilder::create(*this, parser.context().get_stmts()[0], sql);
    return builder.build();
}
}  // namespace gpamgr
