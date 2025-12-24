#include "driver.h"

#include "log.h"
#include "sql.h"
#include "args.h"
#include "misc.h"
#include "tb_exec.h"
#include "builder.h"
#include "ast_dumper.h"

#include "linenoise.hpp"

#include <string>
#include <ranges>
#include <format>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string_view>

namespace gpamgr {

CommandRet ScriptDriver::do_command(std::string_view cmd) {
    cmd = utils::trim(cmd);
    if (cmd.starts_with('.')) {
        // pseudo
        logging::debug("Got pseudo command `{}`", cmd);
        return handle_pseudo(cmd);
    } else if (cmd.starts_with('#')) {
        // logging::debug("Hit comment mark `#`, ignored...");
        return {CommandStat::Continue, ""};
    } else if (cmd.ends_with(';')) {
        // mini-sql
        logging::debug("Got mini-sql command `{}`", cmd);
        return handle_sql(cmd);
    } else {
        // err
        logging::debug("Illegal stmt `{}`", cmd);
        auto diag = utils::Diagnostic(cmd,
                                      "Expect `;` at the end of command",
                                      cmd.size(),
                                      cmd.size(),
                                      utils::Diagnostic::Level::Fatal);
        diag.display();
        return {CommandStat::Error, "Illegal stmt, ignored..."};
    }
}

CommandRet ScriptDriver::do_file(std::string_view path) {
    std::ifstream ifs(path.data());
    if (!ifs.good()) {
        return {CommandStat::Error,
                std::format("Cannot open file {}, please check file stat", path)};
    }
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) {
            continue;
        }
        auto [stat, msg] = do_command(line);
        if (stat == CommandStat::Exit) {
            logging::warn("`.quit` command does nothing in script mode");
            continue;
        }
        if (!msg.empty()) {
            std::cout << msg << '\n';
        }
    }
    logging::debug("Executed file {}", path);
    return {CommandStat::Continue, ""};
}

utils::opt<int> history_max_size("history-max-size",
                                 "Max line limit of interactive shell history",
                                 1000);

utils::opt<std::string> history_path("history-path",
                                     "Interactive shell history-path",
                                     ".gpamgr_history");

void ScriptDriver::run_interactive_shell(std::string_view prompt) {
    // init line noise
    // Enable the multi-line mode
    linenoise::SetMultiLine(true);
    // Set max length of the history
    linenoise::SetHistoryMaxLen(history_max_size);
    // Load history
    linenoise::LoadHistory(history_path.get().data());

    std::string line;
    std::cout << utils::StyledText(
                     "Type `.help` and read the manual if you are new to this program")
                     .cyan()
                     .bold()
                     .underline()
              << '\n';
    while (true) {
        auto quit = linenoise::Readline(prompt.data(), line);
        if (quit) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        auto [stat, msg] = do_command(line);
        linenoise::AddHistory(line.c_str());
        if (!msg.empty()) {
            std::cout << msg << '\n';
        }
        if (stat == CommandStat::Exit) {
            break;
        }
    }

    linenoise::SaveHistory(history_path.get().data());
}

const ScriptDriver::PseudoMap &pseudo_registry();

namespace {

using namespace gpamgr;

#include "doc.h"

CommandRet pp_on_quit(ScriptDriver &, std::string_view) {
    return CommandRet{CommandStat::Exit, "Bye~ uwu"};
}

CommandRet pp_on_sql_doc(ScriptDriver &, std::string_view) {
    std::cout << utils::StyledText(MiniSQLDoc).green();
    return CommandRet{CommandStat::Continue, ""};
}

CommandRet pp_on_debug(ScriptDriver &self, std::string_view args) {
    self.debug_dump();
    return CommandRet{CommandStat::Continue, ""};
}

CommandRet pp_on_status(ScriptDriver &self, std::string_view args) {
    self.dump_status();
    return CommandRet{CommandStat::Continue, ""};
}

CommandRet pp_on_help(ScriptDriver &, std::string_view args) {
    std::stringstream ss;
    const auto &reg = pseudo_registry();

    if (args.empty()) {
        ss << utils::StyledText("\nAvailable pseudo commands:\n").green().italic().bold();
        for (const auto &[name, entry]: reg) {
            ss << "  " << utils::StyledText(name).cyan().bold().underline() << '\n';
            if (!entry.help.empty()) {
                ss << "    " << utils::StyledText(entry.help).green();
            }
            ss << '\n';
        }
        return {CommandStat::Continue, ss.str()};
    }

    // clang-format on
    return {CommandStat::Continue, ss.str()};
}

CommandRet pp_on_load(ScriptDriver &self, std::string_view args) {
    auto table = utils::trim(args);
    if (table.empty()) {
        return CommandRet{CommandStat::Error, "Please give a table name"};
    }
    // NOTE: should I delete this ?
    // if (table.find(utils::BLANK_CHARS) != utils::svnpos) {
    //     return CommandRet{CommandStat::Error, "Table name should not contain white chars"};
    // }
    std::filesystem::path table_path = table;
    // table_path.replace_extension("gpa");
    auto ret = self.load_table(table_path.string());
    if (!ret.has_value()) {
        return CommandRet{CommandStat::Error, ret.error()};
    }
    auto _ = self.set_table(table);
    return CommandRet{CommandStat::Continue, ""};
}

CommandRet pp_on_use(ScriptDriver &self, std::string_view args) {
    auto table = utils::trim(args);
    if (table.empty()) {
        return CommandRet{CommandStat::Error, "Please give a table name"};
    }
    if (table.find(utils::BLANK_CHARS) != utils::svnpos) {
        return CommandRet{CommandStat::Error, "Table name should not contain white chars"};
    }
    auto ret = self.set_table(table);
    if (!ret.has_value()) {
        return CommandRet{CommandStat::Error, ret.error()};
    }
    return CommandRet{CommandStat::Continue, ""};
}

CommandRet pp_on_schema(ScriptDriver &self, std::string_view args) {
    auto tbl = self.curr_table();
    if (tbl.has_value()) {
        tbl.value()->dump_schema(std::cout);
        std::cout << '\n';
        return CommandRet{CommandStat::Continue, ""};
    }
    return CommandRet{CommandStat::Error, tbl.error()};
}

CommandRet pp_on_explain(ScriptDriver &self, std::string_view args) {
    auto curr_tbl = self.curr_table_mut();
    if (!curr_tbl) {
        logging::debug("No table selected");
        return CommandRet{CommandStat::Error, "No table selected"};
    }
    auto ctx = PlanBuildContext(*curr_tbl.value(), self.table_view());
    auto sql = utils::trim(args);
    auto lexed = lex(sql);
    if (!lexed.has_value()) {
        logging::debug("lexer error:\n`{}`\n", lexed.error().to_string());
        lexed.error().display(std::cout);
        return {CommandStat::Continue, ""};
    }
    logging::trace("Begin to parse token stream");
    auto parser = Parser::create(lexed.value(), sql);
    auto errs = parser.parse();
    if (!errs.empty()) {
        for (auto &err: errs) {
            err.display(std::cout);
        }
        logging::debug("Cannot parse");
        return {CommandStat::Continue, ""};
    }
    logging::trace("Begin to generate plan");
    std::cout << utils::StyledText("ASTDump\n").bold();
    ASTDumper(std::cout).visit(parser.context().get_stmts()[0]);
    auto builder = PlanBuilder::create(ctx, parser.context().get_stmts()[0], sql);
    auto ret = builder.build();
    if (!ret.has_value()) {
        for (auto &err: ret.error()) {
            err.display(std::cout);
        }
        return CommandRet{CommandStat::Continue, ""};
    }
    std::cout << utils::StyledText("PlanDump\n").bold();
    auto plan = ret.value();
    plan->explain(std::cout, true);
    return CommandRet{CommandStat::Continue, ""};
}

std::expected<Table::SchemaDesc, std::string> parse_schema(std::string_view sv);

CommandRet pp_on_create(ScriptDriver &self, std::string_view args) {
    args = utils::trim(args);

    auto space = args.find(' ');
    if (space == std::string_view::npos) {
        return {CommandStat::Error, "Usage: .create <table_name> <schema>"};
    }

    auto name = utils::trim(args.substr(0, space));
    auto schema_part = utils::trim(args.substr(space + 1));

    auto schema = parse_schema(schema_part);
    if (!schema) {
        return {CommandStat::Error, std::format("Create table failed: {}", schema.error())};
    }

    auto tbl = self.create_table(name, std::move(*schema));
    if (!tbl) {
        return {CommandStat::Error, tbl.error()};
    }

    return {CommandStat::Continue, std::format("Table `{}` created", name)};
}

CommandRet pp_on_drop(ScriptDriver &self, std::string_view args) {
    auto table = utils::trim(args);
    if (table.empty()) {
        return CommandRet{CommandStat::Error, "Please give a table name"};
    }
    if (table.find(utils::BLANK_CHARS) != utils::svnpos) {
        return CommandRet{CommandStat::Error, "Table name should not contain white chars"};
    }
    if (!self.has_table(table)) {
        return CommandRet{CommandStat::Error, std::format("Table `{}` is not in memory", table)};
    }
    auto ret = self.erase_table(table);
    if (ret.has_value()) {
        return CommandRet{CommandStat::Error, ret.value()};
    }
    return CommandRet{CommandStat::Continue, ""};
}

std::optional<Table::FieldType> parse_field_type(std::string_view sv) {
    if (sv == "int" || sv == "INT" || sv == "u64" || sv == "uint64") {
        return Table::FieldType::INT;
    }
    if (sv == "float" || sv == "double" || sv == "DOUBLE" || sv == "FLOAT") {
        return Table::FieldType::FLOAT;
    }
    if (sv == "str" || sv == "string" || sv == "STR" || sv == "STRING") {
        return Table::FieldType::STRING;
    }
    return std::nullopt;
}

std::expected<Table::SchemaDesc, std::string> parse_schema(std::string_view sv) {
    Table::SchemaDesc schema;
    bool has_primary = false;

    while (!sv.empty()) {
        auto comma = sv.find(',');
        auto field = utils::trim(sv.substr(0, comma));

        // split name : rest
        auto colon = field.find(':');
        if (colon == std::string_view::npos) {
            return std::unexpected(std::format("Invalid field declaration `{}`", field));
        }

        auto name = utils::trim(field.substr(0, colon));
        auto rest = utils::trim(field.substr(colon + 1));

        if (name.empty()) {
            return std::unexpected("Empty field name");
        }

        // split type and modifiers
        auto space = rest.find_first_of(" \t");
        auto type_str = utils::trim(space == std::string_view::npos ? rest : rest.substr(0, space));

        auto modifiers = space == std::string_view::npos ? std::string_view{}
                                                         : utils::trim(rest.substr(space + 1));

        auto type = parse_field_type(type_str);
        if (!type) {
            return std::unexpected(std::format("Unknown field type `{}`", type_str));
        }

        bool is_primary = false;
        if (!modifiers.empty()) {
            // normalize to lower-case for comparison
            std::string mod;
            mod.reserve(modifiers.size());
            for (char ch: modifiers) {
                mod.push_back((char)std::tolower(ch));
            }

            if (mod == "primary key") {
                if (has_primary) {
                    return std::unexpected("Multiple primary keys are not allowed");
                }
                is_primary = true;
                has_primary = true;
            } else {
                return std::unexpected(std::format("Unknown field modifier `{}`", modifiers));
            }
        }

        schema.fields.push_back({std::string(name), *type, is_primary});

        if (comma == std::string_view::npos)
            break;
        sv.remove_prefix(comma + 1);
    }

    if (schema.fields.empty()) {
        return std::unexpected("Schema must contain at least one field");
    }

    if (!has_primary) {
        return std::unexpected("Schema must contain exactly one primary key");
    }

    return schema;
}

}  // namespace

const ScriptDriver::PseudoMap &pseudo_registry() {
    // clang-format off
    const static ScriptDriver::PseudoMap table = {
        { ".quit",     { ".quit -- Quit interactive shell",               pp_on_quit    } },
        { ".debug",    { ".debug -- Debug print driver status",           pp_on_debug   } },
        { ".status",   { ".status -- Print driver status",                pp_on_status  } },
        { ".help",     { ".help -- Print help message",                   pp_on_help    } },
        { ".sql-doc",  { ".sql-doc -- Print document of mini-sql",        pp_on_sql_doc } },
        { ".load",     { ".load <path/to/table> -- load table from file", pp_on_load    } },
        { ".use",      { ".use <table> -- use a table",                   pp_on_use     } },
        { ".schema",   { ".schema -- Display schema of current table",    pp_on_schema  } },
        { ".explain",  { ".explain <sql stmt> -- Explain an sql command", pp_on_explain } },
        { ".create",   { ".create <name> <schema> -- create a new table", pp_on_create  } },
        { ".drop",     { ".drop <name> -- Drop a table in memory",        pp_on_drop    } },
    };
    // clang-format on
    return table;
}

CommandRet ScriptDriver::handle_pseudo(std::string_view cmd) {
    auto pos = cmd.find_first_of(" \t");
    auto name = pos == utils::svnpos ? cmd : cmd.substr(0, pos);
    auto args = pos == utils::svnpos ? std::string_view{} : cmd.substr(pos + 1);
    auto &reg = pseudo_registry();
    auto it = reg.find(name);
    if (it == reg.end()) {
        return {CommandStat::Error, std::format("Unknown pseudo command `{}`", name)};
    }
    logging::trace("Found pseudo command `{}`", name);
    return it->second.handler(*this, args);
}

CommandRet ScriptDriver::handle_sql(std::string_view cmd) {
    if (!curr_tbl) {
        logging::debug("No table selected");
        return CommandRet{CommandStat::Error, "No table selected"};
    }
    auto ctx = PlanBuildContext(*curr_tbl, table_view());
    auto ret = ctx.append_sql(cmd);
    if (!ret.has_value()) {
        logging::debug("Cannot append sql");
        for (auto &e: ret.error()) {
            e.display();
        }
        return CommandRet{CommandStat::Error, ""};
    }
    ExecContext exec_ctx;
    auto printer = [](const RowView &rv) {
        for (size_t i = 0; i < rv.size(); ++i) {
            rv[i].display(std::cout);
            if (i + 1 < rv.size()) {
                std::cout << "|";
            }
        }
        std::cout << '\n';
    };
    exec_ctx = exec_ctx.with_consumer(printer);
    logging::debug("Execution Begin");
    ctx.execute_with_ctx(exec_ctx);
    logging::debug("Execution Ends");
    if (exec_ctx.has_failed()) {
        return {CommandStat::Continue,
                utils::StyledText(exec_ctx.error_msg()).red().italic().underline()};
    }
    return {CommandStat::Continue, ""};
}

std::expected<Table *, std::string> ScriptDriver::load_table(std::string_view path_view) {
    std::filesystem::path p(path_view);
    if (p.extension() != ".gpa") {
        return std::unexpected(
            std::format("Invalid file extension: '{}'. Expected '.gpa'", p.extension().string()));
    }
    std::string key = p.stem().string();
    if (auto it = tb_pool.find(key); it != tb_pool.end()) {
        return it->second.get();
    }

    auto tbl = Table::create(key, path_view);
    if (!tbl.has_value()) {
        return std::unexpected(std::move(tbl.error()));
    }

    logging::debug("Created table from file: {}", path_view);

    auto ptr = std::make_unique<Table>(std::move(*tbl));
    Table *raw = ptr.get();
    tb_pool.emplace(std::move(key), std::move(ptr));

    return raw;
}

std::expected<Table *, std::string> ScriptDriver::create_table(std::string_view name,
                                                               Table::SchemaDesc desc) {
    auto key = std::string(name);
    if (tb_pool.contains(key)) {
        return std::unexpected("Table already exists");
    }
    auto tbl = std::make_unique<Table>(name, std::string(name) + ".gpa");
    tbl->schema = std::move(desc).fields;
    const auto N = tbl->schema.size();
    for (uint64_t i = 0; i < N; ++i) {
        if (tbl->schema[i].is_primary) {
            tbl->primary_field = i;
        }
    }
    tbl->dirty = true;
    Table *raw = tbl.get();
    tb_pool.emplace(std::move(key), std::move(tbl));
    curr_tbl = raw;
    return raw;
}

std::expected<const Table *, std::string> ScriptDriver::curr_table() {
    if (!curr_tbl) {
        logging::debug("No table selected");
        return std::unexpected("No table selected");
    }
    return curr_tbl;
}

std::expected<Table *, std::string> ScriptDriver::curr_table_mut() {
    if (!curr_tbl) {
        logging::debug("No table selected");
        return std::unexpected("No table selected");
    }
    return curr_tbl;
}

std::expected<const Table *, std::string> ScriptDriver::set_table(std::string_view name) {
    auto it = tb_pool.find(std::string(name));
    if (it == tb_pool.end()) {
        logging::debug("Cannot find table {}", name);
        return std::unexpected(std::format("Cannot find table {}", name));
    }
    Table *raw = it->second.get();
    curr_tbl = raw;
    return raw;
}

std::optional<std::string> ScriptDriver::erase_table(std::string_view name) {
    auto it = tb_pool.find(std::string(name));
    if (it == tb_pool.end()) {
        return "Table not found";
    }

    if (curr_tbl == it->second.get()) {
        curr_tbl = nullptr;
    }

    it->second->flush();
    // std::filesystem::remove(it->second->file_path());

    tb_pool.erase(it);
    return std::nullopt;
}

bool ScriptDriver::has_table(std::string_view name) const {
    auto it = tb_pool.find(std::string(name));
    return it != tb_pool.end();
}

ScriptDriver::~ScriptDriver() {
    for (auto &tb: tb_pool) {
        tb.second->flush();
    }
}

void ScriptDriver::debug_dump() {
    std::cout << "Table count: " << tb_pool.size() << '\n';
    if (!curr_tbl) {
        std::cout << "No current table\n";
    }
    for (auto &[name, tb]: tb_pool) {
        std::cout << utils::StyledText::format("Table `{}`", name).blue().bold().italic() << '\n';
        std::cout << utils::StyledText("Schema:").magenta().bold() << '\n';
        tb->dump_schema(std::cout);
        std::cout << '\n';
        auto cb = [&tb](const Table::Row &row) -> void {
            tb->dump_row(row.id);
        };
        tb->scan(cb);
    }
}

void ScriptDriver::dump_status() {
    std::cout << utils::StyledText::format("Table count: {}", tb_pool.size()).yellow().bold()
              << '\n';
    if (!curr_tbl) {
        std::cout << utils::StyledText("[No current table]\n").bold();
    }
    for (auto &[name, tb]: tb_pool) {
        std::cout << utils::StyledText::format("Table `{}`", name).blue().bold().italic();
        if (curr_tbl == tb.get()) {
            std::cout << utils::StyledText(" CURR").bold().magenta();
        }
        std::cout << '\n';
        std::cout << utils::StyledText("Schema:").magenta().bold() << '\n';
        tb->dump_schema(std::cout);
        std::cout << '\n';
    }
}
}  // namespace gpamgr
