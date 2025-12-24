#pragma once

#include <string>
#include <format>
#include <concepts>

namespace ut {

template <typename T>
concept is_expr_v = requires { typename T::expr_tag; };

template <typename Derived>
struct default_formatter : std::formatter<std::string_view> {
    using Base = std::formatter<std::string_view>;

    template <typename FormatContext>
    auto format(const auto &value, FormatContext &ctx) const {
        std::string buffer;
        static_cast<const Derived *>(this)->format_to(std::back_inserter(buffer), value);
        return Base::format(std::string_view(buffer), ctx);
    }
};

template <typename Expr>
decltype(auto) compute(const Expr &expr) {
    if constexpr (requires { typename Expr::expr_tag; }) {
        return expr();
    } else {
        return expr;
    }
}

}  // namespace ut

#define BINARY_PREDICATE(name, op)                                                                 \
    namespace ut {                                                                                 \
    decltype(auto) name##_impl(auto &&lhs, auto &&rhs);                                            \
                                                                                                   \
    template <typename LHS, typename RHS>                                                          \
    struct name {                                                                                  \
        const LHS &lhs;                                                                            \
        const RHS &rhs;                                                                            \
                                                                                                   \
        using expr_tag = int;                                                                      \
                                                                                                   \
        auto operator() () const {                                                                 \
            return name##_impl(compute(lhs), compute(rhs));                                        \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename LHS, typename RHS>                                                          \
    name(const LHS &, const RHS &) -> name<LHS, RHS>;                                              \
    }                                                                                              \
                                                                                                   \
    template <typename LHS, typename RHS>                                                          \
    struct std::formatter<ut::name<LHS, RHS>> :                                                    \
        ut::default_formatter<std::formatter<ut::name<LHS, RHS>>> {                                \
        void format_to(auto &&inserter, const auto &expr) const {                                  \
            std::format_to(inserter, "{} " #op " {}", expr.lhs, expr.rhs);                         \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    decltype(auto) ut::name##_impl(auto &&lhs, auto &&rhs)

BINARY_PREDICATE(add, +) {
    return lhs + rhs;
};

BINARY_PREDICATE(sub, -) {
    return lhs - rhs;
}

BINARY_PREDICATE(mul, *) {
    return lhs * rhs;
}

BINARY_PREDICATE(eq, ==) {
    return lhs == rhs;
}

BINARY_PREDICATE(ne, !=) {
    return lhs != rhs;
}

BINARY_PREDICATE(lt, <) {
    return lhs < rhs;
}

BINARY_PREDICATE(le, <=) {
    return lhs <= rhs;
}

BINARY_PREDICATE(gt, >) {
    return rhs > lhs;
}

BINARY_PREDICATE(ge, >=) {
    return rhs >= lhs;
}

#undef BINARY_PREDICATE
