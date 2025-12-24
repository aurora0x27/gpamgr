#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <format>

namespace utils {
class StyledText {
    constexpr static auto STYLE_RED = "\033[0;31m";
    constexpr static auto STYLE_GREEN = "\033[0;32m";
    constexpr static auto STYLE_YELLOW = "\033[0;33m";
    constexpr static auto STYLE_CYAN = "\033[36m";
    constexpr static auto STYLE_MAGENTA = "\033[35m";
    constexpr static auto STYLE_BLUE = "\033[0;34m";
    constexpr static auto STYLE_BOLD = "\033[1m";
    constexpr static auto STYLE_ITALIC = "\033[3m";
    constexpr static auto STYLE_UNDERLINE = "\033[4m";
    constexpr static auto STYLE_RESET = "\033[0m";

    std::string text_;
    std::string style_;

    friend std::ostream &operator<< (std::ostream &os, const StyledText &st) {
        if (st.style_.empty()) {
            return os << st.style_ << st.text_;
        }
        return os << st.style_ << st.text_ << STYLE_RESET;
    }

public:
    // explicit StyledText(std::string text) : text_(std::move(text)) {}

    explicit StyledText(std::string_view text) : text_(std::string(text)) {}

    template <typename... Args>
    static StyledText format(std::format_string<Args...> fmt, Args &&...args) {
        std::string buf;
        std::format_to(std::back_inserter(buf), fmt, std::forward<Args>(args)...);
        return StyledText{std::move(buf)};
    }

    StyledText &red() {
        style_ += STYLE_RED;
        return *this;
    }

    StyledText &green() {
        style_ += STYLE_GREEN;
        return *this;
    }

    StyledText &yellow() {
        style_ += STYLE_YELLOW;
        return *this;
    }

    StyledText &blue() {
        style_ += STYLE_BLUE;
        return *this;
    }

    StyledText &cyan() {
        style_ += STYLE_CYAN;
        return *this;
    }

    StyledText &magenta() {
        style_ += STYLE_MAGENTA;
        return *this;
    }

    StyledText &bold() {
        style_ += STYLE_BOLD;
        return *this;
    }

    StyledText &italic() {
        style_ += STYLE_ITALIC;
        return *this;
    }

    StyledText &underline() {
        style_ += STYLE_UNDERLINE;
        return *this;
    }

    std::string str() const {
        if (style_.empty()) {
            return text_;
        }
        return std::move(style_ + text_ + STYLE_RESET);
    }

    operator std::string() const {
        if (style_.empty()) {
            return text_;
        }
        return std::move(style_ + text_ + STYLE_RESET);
    }
};

inline bool is_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

constexpr auto BLANK_CHARS = " \t\n\r\f\v";

constexpr auto svnpos = std::string_view::npos;

std::string_view ltrim(std::string_view sv, std::string_view chars = BLANK_CHARS) noexcept;

std::string_view rtrim(std::string_view sv, std::string_view chars = BLANK_CHARS) noexcept;

std::string_view trim(std::string_view sv, std::string_view chars = BLANK_CHARS) noexcept;

std::string_view slice(std::string_view s, size_t B, size_t E) noexcept;

class Diagnostic {
public:
    enum class Level {
        Note,
        Warning,
        Error,
        Fatal,
    };

    Diagnostic(std::string_view source,
               std::string_view msg,
               size_t B,
               size_t E,
               Level L = Level::Note) : source(source), message(msg), begin(B), end(E), level(L) {
        assert(B <= E);
        assert(E <= source.size());
    }

    void render(std::ostream &os, bool color) const;

    void display() const {
        render(std::cout, /*color=*/true);
    }

    void display(std::ostream &os) const {
        render(os, /*color=*/false);
    }

    std::string to_string() const {
        std::ostringstream ss;
        render(ss, /*color=*/false);
        return ss.str();
    }

private:
    Level level;
    std::string message;

    std::string_view source;

    size_t begin = 0;
    size_t end = 0;
};

bool strlike(std::string_view s, std::string_view p);
}  // namespace utils

// Support `std::format("{}", StyledText("ciallo"));`
template <>
struct std::formatter<utils::StyledText> : std::formatter<std::string_view> {
    auto format(const utils::StyledText &st, format_context &ctx) {
        return std::formatter<std::string_view>::format(st.str(), ctx);
    }
};
