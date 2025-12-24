#include "log.h"

#include "args.h"
#include "misc.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <optional>
#include <iostream>

template <>
bool utils::opt<spdlog::level::level_enum>::parse_impl(std::string_view arg) {
    using namespace spdlog;
    if (arg == "trace") {
        value = level::trace;
    } else if (arg == "debug") {
        value = level::debug;
    } else if (arg == "info") {
        value = level::info;
    } else if (arg == "warn") {
        value = level::warn;
    } else if (arg == "error") {
        value = level::err;
    } else if (arg == "fatal") {
        value = level::critical;
    } else if (arg == "off") {
        value = level::off;
    } else {
        std::cerr << StyledText("Unexpected value, ").magenta().bold() << this->get_help() << '\n';
        return false;
    }
    return true;
}

template <>
bool utils::opt<spdlog::color_mode>::parse_impl(std::string_view arg) {
    using namespace spdlog;
    if (arg == "always") {
        value = color_mode::always;
    } else if (arg == "automatic") {
        value = color_mode::automatic;
    } else {
        value = color_mode::never;
    }
    return true;
}

namespace {
const char *to_string(spdlog::level::level_enum level) {
    using spdlog::level::level_enum;
    switch (level) {
        case level_enum::trace: return "trace";
        case level_enum::debug: return "debug";
        case level_enum::info: return "info";
        case level_enum::warn: return "warn";
        case level_enum::err: return "error";
        case level_enum::critical: return "critical";
        case level_enum::off: return "off";
        default: return "unknown";
    }
}

const char *to_string(spdlog::color_mode mode) {
    using spdlog::color_mode;
    switch (mode) {
        case color_mode::always: return "always";
        case color_mode::never: return "never";
        case color_mode::automatic: return "automatic";
        default: return "unknown";
    }
}
}  // namespace

template <>
void utils::opt<spdlog::level::level_enum>::print() const {
    std::cout << StyledText::format("-{}:", name).cyan().bold()
              << StyledText::format(" {}", to_string(value)).green() << '\n';
}

template <>
void utils::opt<spdlog::level::level_enum>::print(std::ostream &os) const {
    os << StyledText::format("-{}:", name).cyan().bold()
       << StyledText::format(" {}", to_string(value)).green() << '\n';
}

template <>
void utils::opt<spdlog::color_mode>::print() const {
    std::cout << StyledText::format("-{}:", name).cyan().bold()
              << StyledText::format(" {}", to_string(value)).green() << '\n';
}

template <>
void utils::opt<spdlog::color_mode>::print(std::ostream &os) const {
    os << StyledText::format("-{}:", name).cyan().bold()
       << StyledText::format(" {}", to_string(value)).green() << '\n';
}

constexpr static auto pattern = "[%Y-%m-%d %H:%M:%S.%e] %^[%l]%$ [%s:%#] %v";

namespace logging {
void init_log(const std::string_view name,
              const spdlog::level::level_enum level,
              spdlog::color_mode color,
              std::optional<std::string_view> log_file) {
    std::shared_ptr<spdlog::logger> logger;
    if (log_file) {
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_st>(std::string(*log_file));
        logger = std::make_shared<spdlog::logger>(std::string(name), std::move(file_sink));
    } else {
        auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_st>(color);
        logger = std::make_shared<spdlog::logger>(std::string(name), std::move(console_sink));
    }
    logger->set_level(level);
    logger->set_pattern(pattern);
    logger->flush_on(spdlog::level::level_enum::trace);
    spdlog::set_default_logger(logger);
}
}  // namespace logging
