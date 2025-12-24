#include "misc.h"

#include <string_view>

namespace utils {
std::string_view ltrim(std::string_view sv, std::string_view chars) noexcept {
    size_t start = sv.find_first_not_of(chars);

    if (start == utils::svnpos) {
        return sv.substr(0, 0);
    }

    return sv.substr(start);
}

std::string_view rtrim(std::string_view sv, std::string_view chars) noexcept {
    size_t end = sv.find_last_not_of(chars);

    if (end == utils::svnpos) {
        return sv.substr(0, 0);
    }

    return sv.substr(0, end + 1);
}

std::string_view trim(std::string_view sv, std::string_view chars) noexcept {
    std::string_view trimmed_leading = ltrim(sv, chars);

    return rtrim(trimmed_leading, chars);
}

std::string_view slice(std::string_view s, size_t B, size_t E) noexcept {
    return s.substr(B, E - B);
}

namespace term {
constexpr const char *reset = "\033[0m";
constexpr const char *red = "\033[31m";
constexpr const char *magenta = "\033[35m";
constexpr const char *yellow = "\033[33m";
constexpr const char *blue = "\033[34m";
constexpr const char *cyan = "\033[36m";
constexpr const char *bold = "\033[1m";
}  // namespace term

void Diagnostic::render(std::ostream &os, bool color) const {
    auto level_name = [&](Level lv) {
        switch (lv) {
            case Level::Note: return "note";
            case Level::Warning: return "warning";
            case Level::Error: return "error";
            case Level::Fatal: return "fatal";
        }
        return "unknown";
    };

    auto level_color = [&]() {
        if (!color)
            return "";
        switch (level) {
            case Level::Note: return term::blue;
            case Level::Warning: return term::yellow;
            case Level::Error: return term::magenta;
            case Level::Fatal: return term::red;
            default: std::abort();
        }
    };

    if (color) {
        os << term::bold << level_color();
    }

    os << level_name(level) << ": ";

    if (color) {
        os << term::reset;
    }

    if (color) {
        os << term::bold;
    }

    os << message << '\n';

    if (color) {
        os << term::reset;
    }

    if (!source.empty()) {
        os << source << '\n';
        os << std::string(begin, ' ');
        if (color) {
            os << term::cyan << term::bold;
        }
        if (end - begin) {
            os << std::string(end - begin, '~');
        } else {
            os << std::string(1, '^');
        }
        if (color) {
            os << term::reset;
        }
        os << '\n';
    }
}

bool strlike(std::string_view s, std::string_view p) {
    size_t s_idx = 0, p_idx = 0;
    size_t s_backtrack = 0;
    size_t p_star_idx = std::string_view::npos;

    while (s_idx < s.size()) {
        if (p_idx < p.size() && (p[p_idx] == '_' || p[p_idx] == s[s_idx])) {
            s_idx++;
            p_idx++;
        } else if (p_idx < p.size() - 1 && p[p_idx] == '\\' && p[p_idx + 1] == s[s_idx]) {
            p_idx += 2;
            s_idx++;
        } else if (p_idx < p.size() && p[p_idx] == '%') {
            p_star_idx = p_idx;
            s_backtrack = s_idx;
            p_idx++;
        } else if (p_star_idx != std::string_view::npos) {
            p_idx = p_star_idx + 1;
            s_backtrack++;
            s_idx = s_backtrack;
        } else {
            return false;
        }
    }

    while (p_idx < p.size() && p[p_idx] == '%') {
        p_idx++;
    }

    return p_idx == p.size();
}
}  // namespace utils
