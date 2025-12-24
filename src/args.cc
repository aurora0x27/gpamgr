#include "args.h"

#include "misc.h"

#include <iostream>
#include <charconv>

namespace utils {

void print_help(std::string_view prog_name, std::string_view overview) {
    std::cout << StyledText::format("{}:", prog_name).green().bold().underline()
              << StyledText::format(" {}", overview).cyan() << "\n\n";
    std::cout << StyledText("Options:").green().bold().underline() << '\n';
    for (auto &[_, opt]: arg_registry().options) {
        std::cout << StyledText::format("-{}", opt->get_name()).cyan().bold() << "\n\t\t\t"
                  << opt->get_help() << "\n";
    }
}

void dump_args() {
    std::cout << StyledText("Parsed options:").green().underline().bold() << '\n';
    for (auto &[_, opt]: arg_registry().options) {
        opt->print();
    }
}

void dump_args(std::ostream &os) {
    os << StyledText("Parsed options:").green().underline().bold() << '\n';
    for (auto &[_, opt]: arg_registry().options) {
        opt->print(os);
    }
}

template <>
bool opt<int>::parse_impl(std::string_view arg) {
    const char *B = arg.data();
    const char *E = B + arg.size();
    int v{};
    auto [p, ec] = std::from_chars(B, E, v);
    if (ec != std::errc{}) {
        std::cerr << StyledText("Error: ").red().bold() << "Option `-" << name << "` expect int\n";
        return false;
    }
    value = v;
    return true;
}

template <>
bool opt<double>::parse_impl(std::string_view arg) {
    double d{};
    const char *B = arg.data();
    const char *E = B + arg.size();
    auto [p, ec] = std::from_chars(B, E, d);
    if (ec != std::errc{}) {
        return false;
    }
    value = d;
    return true;
}

static std::vector<std::string_view> position_arg_list;
static bool help_opt_triggered = false;

const bool help_triggered() {
    return help_opt_triggered;
}

template <>
bool opt<std::string>::parse_impl(std::string_view arg) {
    value = arg;
    return true;
}

OptionRegistry &arg_registry() {
    static OptionRegistry r;
    return r;
}

const std::vector<std::string_view> &position_args() {
    return position_arg_list;
}

bool parse_commandline_options(const int argc, const char **const argv) {
    bool all_ok = true;
    bool positional_only = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (!positional_only && (arg == "--help" || arg == "-h")) {
            // print_help(prog, overview);
            help_opt_triggered = true;
            return true;
        }

        if (!positional_only && arg == "--") {
            positional_only = true;
            continue;
        }

        if (positional_only || arg.empty() || arg[0] != '-') {
            position_arg_list.push_back(arg);
            continue;
        }

        std::string_view token = arg;
        if (token.rfind("--", 0) == 0) {
            token.remove_prefix(2);
        } else {
            token.remove_prefix(1);
        }

        std::string_view name = arg.substr(1);
        std::string_view inline_value;

        // parse `-name=val`
        auto eq = token.find('=');
        if (eq != svnpos) {
            name = token.substr(0, eq);
            inline_value = token.substr(eq + 1);
        } else {
            name = token;
        }

        // find option entry
        auto it = arg_registry().options.find(name);
        if (it == arg_registry().options.end()) {
            std::cerr << StyledText("Unknown option: ").bold() << arg << "\n";
            all_ok = false;
            continue;
        }

        auto *opt = it->second;
        std::string_view value;
        bool has_value = false;
        if (!inline_value.empty() || opt->is_flag()) {
            value = inline_value;
            has_value = true;
        } else if (i + 1 < argc && argv[i + 1][0] != '-') {
            value = argv[++i];
            has_value = true;
        }

        if (!opt->parse(has_value ? value : std::string_view{})) {
            std::cerr << StyledText("Invalid value for option: ").bold() << arg << "\n";
            all_ok = false;
        }
    }
    return all_ok;
}
}  // namespace utils
