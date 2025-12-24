#include "args.h"
#include "log.h"
#include "misc.h"
#include "driver.h"

constexpr static auto PROG_NAME = "gpamgr";
constexpr static auto OVERVIEW = "Simple command line gpa manager";
constexpr static auto PROMPT = "\033[36m\033[1mgpamgr>\033[0m ";

utils::opt<spdlog::level::level_enum>
    log_level("log-level",
              "Asign log level among <trace|debug|info|warn|error|fatal|off>",
              spdlog::level::info);
utils::opt<std::string> log_file("log-file", "Assign log file path", "");
utils::opt<spdlog::color_mode> log_color("log-color",
                                         "Assign logger color mode among <always|automatic|never>",
                                         spdlog::color_mode::automatic);
utils::opt<bool> interactive("i", "Open up an interactive shell", false);
utils::opt<std::string> script_files("script", "Assign script files, split with ','", "");
utils::opt<std::string> command("c", "Run single command", "");
utils::opt<std::string> shell_prompt("prompt", "Interactive shell prompt", PROMPT);

void print_banner() {
    // clang-format off
    std::cout << utils::StyledText(R"BANNER_TEXT(   __________  ___    __  _____________       )BANNER_TEXT").green() << '\n';
    std::cout << utils::StyledText(R"BANNER_TEXT(  / ____/ __ \/   |  /  |/  / ____/ __ \      )BANNER_TEXT").green() << '\n';
    std::cout << utils::StyledText(R"BANNER_TEXT( / / __/ /_/ / /| | / /|_/ / / __/ /_/ /      )BANNER_TEXT").green() << '\n';
    std::cout << utils::StyledText(R"BANNER_TEXT(/ /_/ / ____/ ___ |/ /  / / /_/ / _, _/       )BANNER_TEXT").green() << '\n';
    std::cout << utils::StyledText(R"BANNER_TEXT(\____/_/   /_/  |_/_/  /_/\____/_/ |_|        )BANNER_TEXT").green() << '\n';
    std::cout << utils::StyledText(R"BANNER_TEXT(                                      By qfzl.)BANNER_TEXT").cyan().bold() << "\n\n";
    // clang-format on
}

int main(int argc, const char **argv) {

    if (!utils::parse_commandline_options(argc, argv)) {
        return 1;
    }

    if (utils::help_triggered()) {
        print_banner();
        utils::print_help(PROG_NAME, OVERVIEW);
        return 0;
    }

    logging::init_log(PROG_NAME,
                      log_level,
                      log_color,
                      log_file.get().empty() ? std::nullopt
                                             : std::optional<std::string_view>{log_file.get()});

    if (log_level <= spdlog::level::level_enum::debug) {
        std::cout << '\n';
        utils::dump_args(std::cout);
        std::cout << '\n';
    }

    auto file_list = utils::position_args();

    if (log_level <= spdlog::level::level_enum::debug) {
        std::cout << utils::StyledText("Table files:").blue().bold() << '\n';
        for (auto &file: file_list) {
            std::cout << utils::StyledText::format("  - {}", file).blue() << '\n';
        }
        std::cout << '\n';
    }

    gpamgr::ScriptDriver driver;

    std::string_view last_tbl;
    for (auto &file: file_list) {
        auto tbl = driver.load_table(file);
        if (!tbl.has_value()) {
            logging::error("Cannot load file `{}`", file);
            std::cout << tbl.error() << '\n';
            continue;
        }
        last_tbl = tbl.value()->get_name();
    }

    if (!last_tbl.empty()) {
        auto _ = driver.set_table(last_tbl);
        logging::debug("Using table `{}`", last_tbl);
    } else {
        logging::warn("No table is selected");
    }

    int ret = 0;

    if (!(*command).empty()) {
        // run command string
        auto [stat, msg] = driver.do_command(command.get());
        if (!msg.empty()) {
            std::cout << msg << '\n';
        }
        if (stat == gpamgr::CommandStat::Error) {
            ret = 1;
        }
    } else if (!(*script_files).empty()) {
        // do script files

        std::vector<std::string> files;
        std::string_view rest = *script_files;

        // split by ','
        while (true) {
            auto pos = rest.find(',');
            std::string_view token = (pos == utils::svnpos) ? rest : rest.substr(0, pos);

            if (!token.empty()) {
                files.emplace_back(token);
                logging::trace("Got script file: {}", token);
            } else {
                logging::warn("Empty script filename, ignored");
            }

            if (pos == utils::svnpos) {
                break;
            }

            rest.remove_prefix(pos + 1);
        }

        logging::trace("End splitting script files");

        for (auto &file: files) {
            logging::trace("Doing file `{}`", file);
            auto [stat, msg] = driver.do_file(file);
            if (!msg.empty()) {
                std::cout << msg << '\n';
            }
            if (stat == gpamgr::CommandStat::Error) {
                logging::trace("Error occurred when doing File `{}`", file);
                ret = 1;
            } else {
                logging::trace("File `{}` done", file);
            }
        }

    } else {
        // interactive mode
        print_banner();
        driver.run_interactive_shell(shell_prompt.get());
        return 0;
    }

    if (interactive) {
        print_banner();
        driver.run_interactive_shell(shell_prompt.get());
    }

    return ret;
}
