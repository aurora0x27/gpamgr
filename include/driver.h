#pragma once

#include "table.h"

#include <map>
#include <string>
#include <memory>
#include <string_view>

namespace gpamgr {

class ScriptDriver {
    friend class Table;

    Table *curr_tbl = nullptr;
    std::map<std::string, std::unique_ptr<Table>> tb_pool;

public:
    enum class CommandStat : short {
        Exit = -1,
        Continue = 0,
        Error = 1,
    };

    struct CommandRet {
        CommandStat stat;
        std::string msg;
    };

    CommandRet do_command(std::string_view cmd);
    CommandRet do_file(std::string_view path);
    void run_interactive_shell(std::string_view prompt);

    TableView table_view() {
        TableView view;
        view.reserve(tb_pool.size());
        for (auto &[name, tbl]: tb_pool) {
            view.emplace(name, tbl.get());
        }
        return view;
    }

    using PseudoHandler = std::function<CommandRet(ScriptDriver &, std::string_view)>;

    struct PseudoCmdEntry {
        std::string_view help;
        PseudoHandler handler;
    };

    using PseudoMap = std::unordered_map<std::string_view, PseudoCmdEntry>;

    std::expected<Table *, std::string> load_table(std::string_view path_view);
    std::expected<Table *, std::string> create_table(std::string_view name, Table::SchemaDesc desc);
    std::expected<const Table *, std::string> curr_table();
    std::expected<Table *, std::string> curr_table_mut();
    std::expected<const Table *, std::string> set_table(std::string_view name);
    [[nodiscard]] bool has_table(std::string_view name) const;
    std::optional<std::string> erase_table(std::string_view name);
    void dump_status();

    void debug_dump();

    ~ScriptDriver();

private:
    // // For batch execution, unused now
    // void flush_sql();
    CommandRet handle_pseudo(std::string_view);
    CommandRet handle_sql(std::string_view);
};

using CommandStat = ScriptDriver::CommandStat;
using CommandRet = ScriptDriver::CommandRet;

}  // namespace gpamgr
