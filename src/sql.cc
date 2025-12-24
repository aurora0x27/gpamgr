#include "sql.h"

#include "log.h"
#include "ast.h"

#include <cctype>

namespace gpamgr {
namespace {
bool is_ident_start(char c) {
    return std::isalpha(c) || c == '_';
}

bool is_ident(char c) {
    return std::isalnum(c) || c == '_';
}

TokenType keyword(std::string_view s) {
    using enum TokenType;
    if (s == "select") {
        return tk_select;
    } else if (s == "insert") {
        return tk_insert;
    } else if (s == "update") {
        return tk_update;
    } else if (s == "delete") {
        return tk_delete;
    } else if (s == "values") {
        return tk_values;
    } else if (s == "where") {
        return tk_where;
    } else if (s == "from") {
        return tk_from;
    } else if (s == "into") {
        return tk_into;
    } else if (s == "and") {
        return tk_and;
    } else if (s == "or") {
        return tk_or;
    } else if (s == "like") {
        return tk_like;
    } else if (s == "set") {
        return tk_set;
    } else if (s == "order") {
        return tk_order;
    } else if (s == "by") {
        return tk_by;
    } else if (s == "asc") {
        return tk_asc;
    } else if (s == "desc") {
        return tk_desc;
    }
    return tk_identifier;
}
}  // namespace

std::expected<std::vector<Token>, utils::Diagnostic> lex(std::string_view sql) {
    using Level = utils::Diagnostic::Level;
    std::vector<Token> tokens;
    size_t i = 0;
    const size_t n = sql.size();

    auto err = [&](size_t pos, const std::string &msg) {
        return std::unexpected(utils::Diagnostic(sql, msg, pos, pos, Level::Error));
    };

    while (i < n) {
        char c = sql[i];

        // skip whitespace
        if (std::isspace(c)) {
            ++i;
            continue;
        }

        size_t B = i;

        // identifier / keyword
        if (is_ident_start(c)) {
            ++i;
            while (i < n && is_ident(sql[i])) {
                ++i;
            }

            auto text = sql.substr(B, i - B);
            std::string lower;
            lower.reserve(text.size());
            for (char ch: text) {
                lower.push_back((char)std::tolower(ch));
            }

            TokenType ty = keyword(lower);
            logging::trace("Got {} `{}`",
                           ty == TokenType::tk_identifier ? "identifier" : "keyword",
                           sql.substr(B, i - B));
            tokens.push_back({ty, B, i});
            continue;
        }

        if (c == '"' || c == '\'') {
            char quote = c;
            size_t B = i + 1;
            ++i;  // skip opening quote

            while (i < n && sql[i] != quote) {
                if (sql[i] == '\\') {  // escape
                    ++i;
                    if (i >= n) {
                        return err(B, "unterminated string literal");
                    }
                }
                ++i;
            }

            if (i >= n) {
                return err(B, "unterminated string literal");
            }

            logging::trace("Got string literal `{}`", sql.substr(B, i - B));
            tokens.push_back({TokenType::tk_string, B, i});
            ++i;  // skip closing quote
            continue;
        }

        // number (int or float)
        if (std::isdigit(c)) {
            size_t B = i;
            bool is_float = false;

            // integer part
            while (i < n && std::isdigit(sql[i])) {
                ++i;
            }

            // fractional part
            if (i < n && sql[i] == '.') {
                // lookahead: must have at least one digit after '.'
                if (i + 1 < n && std::isdigit(sql[i + 1])) {
                    is_float = true;
                    ++i;  // skip '.'
                    while (i < n && std::isdigit(sql[i])) {
                        ++i;
                    }
                } else {
                    return err(i, "invalid floating literal");
                }
            }

            logging::trace("Got number `{}`", sql.substr(B, i - B));
            tokens.push_back({TokenType::tk_num, B, i});
            continue;
        }

        // operators
        switch (c) {
            case '=':
                logging::trace("Got op =");
                tokens.push_back({TokenType::tk_eq, i, i + 1});
                ++i;
                break;

            case '!':
                if (i + 1 < n && sql[i + 1] == '=') {
                    logging::trace("Got op !=");
                    tokens.push_back({TokenType::tk_ne, i, i + 2});
                    i += 2;
                } else {
                    return err(i, "unexpected '!'");
                }
                break;

            case '<':
                if (i + 1 < n && sql[i + 1] == '=') {
                    logging::trace("Got op <=");
                    tokens.push_back({TokenType::tk_le, i, i + 2});
                    i += 2;
                } else {
                    logging::trace("Got op >");
                    tokens.push_back({TokenType::tk_lt, i, i + 1});
                    ++i;
                }
                break;

            case '>':
                if (i + 1 < n && sql[i + 1] == '=') {
                    logging::trace("Got op >=");
                    tokens.push_back({TokenType::tk_ge, i, i + 2});
                    i += 2;
                } else {
                    logging::trace("Got op >");
                    tokens.push_back({TokenType::tk_gt, i, i + 1});
                    ++i;
                }
                break;

            case ',':
                logging::trace("Got `,`");
                tokens.push_back({TokenType::tk_comma, i, i + 1});
                ++i;
                break;

            case ';':
                logging::trace("Got `;`");
                tokens.push_back({TokenType::tk_semi, i, i + 1});
                ++i;
                break;

            case '(':
                logging::trace("Got `(`");
                tokens.push_back({TokenType::tk_lparen, i, i + 1});
                ++i;
                break;

            case ')':
                logging::trace("Got `)`");
                tokens.push_back({TokenType::tk_rparen, i, i + 1});
                ++i;
                break;

            case '*':
                logging::trace("Got `*`");
                tokens.push_back({TokenType::tk_star, i, i + 1});
                ++i;
                break;

            case '-':
                logging::trace("Got `-`");
                tokens.push_back({TokenType::tk_minus, i, i + 1});
                ++i;
                break;

            case '+':
                logging::trace("Got `+`");
                tokens.push_back({TokenType::tk_plus, i, i + 1});
                ++i;
                break;

            case '/':
                logging::trace("Got `/`");
                tokens.push_back({TokenType::tk_slash, i, i + 1});
                ++i;
                break;

            case '#':
                logging::trace("Hit comment mark `#`");
                tokens.push_back({TokenType::tk_eof, i, i + 1});
                return tokens;

            default: return err(i, std::string("unexpected character: '") + c + "'");
        }
    }

    tokens.push_back({TokenType::tk_eof, i, i + 1});
    return tokens;
}

class Parser::ParserImpl {
    std::span<const Token> tokens;
    ASTContext ctx;
    std::string_view source;
    decltype(tokens.begin()) current_tk;

    [[nodiscard]] const Token &peek() const {
        return *(current_tk + 1);
    }

    void consume() {
        logging::trace("Consumed token `{}`", utils::slice(source, current_tk->B, current_tk->E));
        ++current_tk;
    }

    bool consume_if(TokenType ty) {
        if (!can_parse_next()) {
            return false;
        }
        if (current_tk->ty == ty) {
            logging::trace("Consumed token `{}`",
                           utils::slice(source, current_tk->B, current_tk->E));
            ++current_tk;
            return true;
        }
        return false;
    }

    using Level = utils::Diagnostic::Level;

    utils::Diagnostic raise_error(std::string_view msg, size_t B, size_t E) {
        return utils::Diagnostic{source, msg, B, E, Level::Error};
    }

    utils::Diagnostic raise_warn(std::string_view msg, size_t B, size_t E) {
        return utils::Diagnostic{source, msg, B, E, Level::Warning};
    }

    utils::Diagnostic raise_note(std::string_view msg, size_t B, size_t E) {
        return utils::Diagnostic{source, msg, B, E, Level::Note};
    }

    utils::Diagnostic raise_fatal(std::string_view msg, size_t B, size_t E) {
        return utils::Diagnostic{source, msg, B, E, Level::Fatal};
    }

public:
    ParserImpl(std::vector<Token> &token_stream, std::string_view source) :
        tokens(token_stream.begin(), token_stream.end()), current_tk(tokens.begin()),
        source(source) {}

    ASTContext &context() {
        return ctx;
    }

    [[nodiscard]] bool can_parse_next() const {
        return current_tk != tokens.end() && current_tk->ty != TokenType::tk_eof;
    }

    void recover_to_next_stmt() {
        while (current_tk != tokens.end()) {
            if (current_tk->ty == TokenType::tk_semi) {
                ++current_tk;
                break;
            }
            ++current_tk;
        }
    }

    std::expected<Stmt *, utils::Diagnostic> parse_stmt() {
        switch (current_tk->ty) {
            case TokenType::tk_select: return parse_select_stmt();
            case TokenType::tk_insert: return parse_insert_stmt();
            case TokenType::tk_update: return parse_update_stmt();
            case TokenType::tk_delete: return parse_delete_stmt();
            default:
                return std::unexpected(
                    raise_error("Expected a keyword among `SELECT`, `INSERT`, `UPDATE`, `DELETE`",
                                current_tk->B,
                                current_tk->E));
        }
    }

private:
    std::expected<Stmt *, utils::Diagnostic> parse_select_stmt() {
        auto select_ky = current_tk;
        size_t B = current_tk->B;
        consume();

        std::vector<Expr *> select_list{};
        IdentifierExpr *from = nullptr;
        Expr *cond = nullptr;
        std::optional<OrderByClause> sort = std::nullopt;
        bool seen_where = false;

        // Parse select list
        bool select_all = false;
        if (current_tk->ty == TokenType::tk_star) {
            select_all = true;
            consume();
        } else {
            while (current_tk->ty != TokenType::tk_from && current_tk->ty != TokenType::tk_semi &&
                   current_tk->ty != TokenType::tk_eof) {
                if (current_tk->ty == TokenType::tk_identifier) {
                    auto selected = parse_primary();
                    if (!selected) {
                        return std::unexpected(selected.error());
                    }
                    select_list.push_back(selected.value());
                    if (!consume_if(TokenType::tk_comma)) {
                        break;
                    }
                } else {
                    return std::unexpected(
                        raise_error("Expect Identifier", current_tk->E, current_tk->E));
                }
            }
            if (current_tk->ty != TokenType::tk_from) {
                return std::unexpected(
                    raise_error("Need `,` to split fields", current_tk->E, current_tk->E));
            }
        }
        if (!select_all && select_list.empty()) {
            return std::unexpected(raise_error("Expect select list", select_ky->E, select_ky->E));
        }

        // Parse from
        if (!consume_if(TokenType::tk_from)) {
            return std::unexpected(
                raise_error("Expect keyword `FROM`", current_tk->E, current_tk->E));
        }
        if (current_tk->ty != TokenType::tk_identifier) {
            return std::unexpected(
                raise_error("Expect table name after FROM", current_tk->B, current_tk->E));
        }
        from = ctx.make_expr<IdentifierExpr>(utils::slice(source, current_tk->B, current_tk->E),
                                             current_tk->B,
                                             current_tk->E);
        consume();

        // Parse where
        if (consume_if(TokenType::tk_where)) {
            seen_where = true;
            auto cond_expr = parse_condition();
            if (!cond_expr.has_value()) {
                return std::unexpected(std::move(cond_expr.error()));
            }
            cond = cond_expr.value();
        }

        // Parse order by
        if (consume_if(TokenType::tk_order)) {

            auto order_tk = current_tk - 1;
            if (!consume_if(TokenType::tk_by)) {
                return std::unexpected(raise_error("Expect keyword `BY` after keyword `ORDER`",
                                                   order_tk->E,
                                                   order_tk->E));
            }
            OrderByClause obc;
            obc.B = current_tk->B;
            while (true) {
                if (current_tk->ty != TokenType::tk_identifier) {
                    return std::unexpected(
                        raise_error("Expect identifier in ORDER BY", current_tk->B, current_tk->E));
                }
                auto col = utils::slice(source, current_tk->B, current_tk->E);
                consume();
                bool asc = true;  // default
                if (current_tk->ty == TokenType::tk_asc) {
                    asc = true;
                    consume();
                } else if (current_tk->ty == TokenType::tk_desc) {
                    asc = false;
                    consume();
                }
                obc.keys.push_back(OrderKey{col, asc});
                if (!consume_if(TokenType::tk_comma)) {
                    break;
                }
            }
            obc.E = (current_tk - 1)->E;
            sort = std::move(obc);
        }

        if (!consume_if(TokenType::tk_semi)) {
            auto last_tk = current_tk - 1;
            return std::unexpected(raise_warn("Expect semi at the end", last_tk->E, last_tk->E));
        }

        size_t E = (current_tk - 1)->E;
        return ctx.make_stmt<SelectStmt>(std::span<Expr *>{select_list.begin(), select_list.end()},
                                         from,
                                         cond,
                                         sort,
                                         B,
                                         E);
    }

    std::expected<Stmt *, utils::Diagnostic> parse_insert_stmt() {
        auto insert_tk = current_tk;
        size_t B = insert_tk->B;
        consume();  // consume INSERT

        // INTO
        if (!consume_if(TokenType::tk_into)) {
            return std::unexpected(
                raise_error("Expect keyword `into` after INSERT", insert_tk->E, insert_tk->E));
        }

        // table name
        if (current_tk->ty != TokenType::tk_identifier) {
            return std::unexpected(
                raise_error("Expect table name after INSERT INTO", current_tk->B, current_tk->E));
        }

        auto table =
            ctx.make_expr<IdentifierExpr>(utils::slice(source, current_tk->B, current_tk->E),
                                          current_tk->B,
                                          current_tk->E);
        consume();

        // VALUES
        if (!consume_if(TokenType::tk_values)) {
            return std::unexpected(raise_error("Expect keyword `values` after table name",
                                               current_tk->E,
                                               current_tk->E));
        }

        // '('
        if (!consume_if(TokenType::tk_lparen)) {
            return std::unexpected(
                raise_error("Expect '(' after VALUES", current_tk->B, current_tk->E));
        }

        // value list
        std::vector<Expr *> values;

        if (current_tk->ty == TokenType::tk_rparen) {
            return std::unexpected(
                raise_error("VALUES list cannot be empty", current_tk->B, current_tk->E));
        }

        while (true) {
            auto val = parse_primary();
            if (!val.has_value()) {
                return std::unexpected(std::move(val.error()));
            }
            values.push_back(val.value());

            if (consume_if(TokenType::tk_comma)) {
                continue;
            }
            break;
        }

        // ')'
        if (!consume_if(TokenType::tk_rparen)) {
            return std::unexpected(
                raise_error("Expect ')' after VALUES list", current_tk->B, current_tk->E));
        }

        // ';'
        if (!consume_if(TokenType::tk_semi)) {
            auto last_tk = current_tk - 1;
            return std::unexpected(
                raise_warn("Expect ';' at end of INSERT statement", last_tk->E, last_tk->E));
        }

        size_t E = (current_tk - 1)->E;
        return ctx.make_stmt<InsertStmt>(table,
                                         std::span<Expr *>{values.begin(), values.end()},
                                         B,
                                         E);
    }

    std::expected<Stmt *, utils::Diagnostic> parse_update_stmt() {
        auto update_tk = current_tk;
        size_t B = update_tk->B;
        consume();  // consume UPDATE

        // table name
        if (current_tk->ty != TokenType::tk_identifier) {
            return std::unexpected(
                raise_error("Expect table name after UPDATE", current_tk->B, current_tk->E));
        }

        auto table =
            ctx.make_expr<IdentifierExpr>(utils::slice(source, current_tk->B, current_tk->E),
                                          current_tk->B,
                                          current_tk->E);
        consume();

        // SET
        if (!consume_if(TokenType::tk_set)) {
            return std::unexpected(
                raise_error("Expect keyword `set` after table name", current_tk->B, current_tk->E));
        }

        // assignment list
        std::vector<Assignment> assigns;

        while (true) {
            if (current_tk->ty != TokenType::tk_identifier) {
                return std::unexpected(
                    raise_error("Expect column name in SET clause", current_tk->B, current_tk->E));
            }

            Assignment assign{};

            assign.field =
                ctx.make_expr<IdentifierExpr>(utils::slice(source, current_tk->B, current_tk->E),
                                              current_tk->B,
                                              current_tk->E);
            consume();

            if (!consume_if(TokenType::tk_eq)) {
                return std::unexpected(
                    raise_error("Expect '=' in assignment", current_tk->B, current_tk->E));
            }

            auto val = parse_primary();
            if (!val.has_value()) {
                return std::unexpected(std::move(val.error()));
            }
            assign.value = val.value();

            assigns.push_back(assign);
            if (!consume_if(TokenType::tk_comma)) {
                break;
            }
        }

        if (assigns.empty()) {
            return std::unexpected(
                raise_error("SET clause cannot be empty", update_tk->E, update_tk->E));
        }

        // WHERE
        Expr *where = nullptr;
        if (consume_if(TokenType::tk_where)) {
            auto cond = parse_condition();
            if (!cond.has_value()) {
                return std::unexpected(std::move(cond.error()));
            }
            where = cond.value();
        }

        // ';'
        if (!consume_if(TokenType::tk_semi)) {
            auto last_tk = current_tk - 1;
            return std::unexpected(
                raise_warn("Expect ';' at end of UPDATE statement", last_tk->E, last_tk->E));
        }

        size_t E = (current_tk - 1)->E;
        return ctx.make_stmt<UpdateStmt>(table,
                                         std::span<Assignment>(assigns.begin(), assigns.end()),
                                         where,
                                         B,
                                         E);
    }

    /// delete_stmt
    ///     ::= DELETE FROM identifier
    ///         [ WHERE condition ]
    ///         ;
    std::expected<Stmt *, utils::Diagnostic> parse_delete_stmt() {
        auto delete_kw = current_tk;
        size_t B = delete_kw->B;
        consume();  // consume DELETE

        // DELETE FROM
        if (!consume_if(TokenType::tk_from)) {
            return std::unexpected(
                raise_error("Expect keyword `from` after `delete`", delete_kw->E, delete_kw->E));
        }

        // table name
        if (current_tk->ty != TokenType::tk_identifier) {
            return std::unexpected(
                raise_error("Expect table name after `from`", current_tk->B, current_tk->E));
        }

        auto *table =
            ctx.make_expr<IdentifierExpr>(utils::slice(source, current_tk->B, current_tk->E),
                                          current_tk->B,
                                          current_tk->E);
        consume();

        // optional WHERE
        Expr *cond = nullptr;
        if (consume_if(TokenType::tk_where)) {
            auto cond_expr = parse_condition();
            if (!cond_expr.has_value()) {
                return std::unexpected(std::move(cond_expr.error()));
            }
            cond = cond_expr.value();
        }

        // ending semicolon
        if (!consume_if(TokenType::tk_semi)) {
            auto last_tk = current_tk - 1;
            return std::unexpected(
                raise_warn("Expect `;` at end of DELETE statement", last_tk->E, last_tk->E));
        }
        size_t E = (current_tk - 1)->E;

        return ctx.make_stmt<DeleteStmt>(table, cond, B, E);
    }

    std::expected<Expr *, utils::Diagnostic> parse_primary() {
        switch (current_tk->ty) {
            case TokenType::tk_identifier: {
                auto name = utils::slice(source, current_tk->B, current_tk->E);
                auto *id = ctx.make_expr<IdentifierExpr>(name, current_tk->B, current_tk->E);
                consume();

                if (consume_if(TokenType::tk_lparen)) {
                    std::vector<Expr *> args;
                    if (!consume_if(TokenType::tk_rparen)) {
                        while (true) {
                            auto arg = parse_condition();
                            if (!arg)
                                return arg;
                            args.push_back(arg.value());
                            if (consume_if(TokenType::tk_comma)) {
                                continue;
                            }
                            if (!consume_if(TokenType::tk_rparen)) {
                                return std::unexpected(
                                    raise_error("Expect ')'", current_tk->B, current_tk->E));
                            }
                            break;
                        }
                    }
                    auto [B, _] = id->src_range();
                    size_t E = (current_tk - 1)->E;
                    return ctx.make_expr<CallExpr>(id,
                                                   std::span<Expr *>(args.begin(), args.end()),
                                                   B,
                                                   E);
                }

                return id;
            }

            case TokenType::tk_num: {
                auto sv = utils::slice(source, current_tk->B, current_tk->E);
                auto [B, E] = current_tk->src_range();
                consume();
                if (sv.find('.') != std::string_view::npos) {
                    return ctx.make_expr<FloatLiteral>(std::stod(std::string(sv)), B, E);
                } else {
                    return ctx.make_expr<IntegerLiteral>(std::stoll(std::string(sv)), B, E);
                }
            }

            case TokenType::tk_string: {
                auto [B, E] = current_tk->src_range();
                auto sv = utils::slice(source, current_tk->B, current_tk->E);
                consume();
                return ctx.make_expr<StringLiteral>(sv, B, E);
            }

            case TokenType::tk_lparen: {
                consume();  // '('
                auto expr = parse_condition();
                if (!expr)
                    return expr;
                if (!consume_if(TokenType::tk_rparen)) {
                    return std::unexpected(raise_error("Expect ')'", current_tk->B, current_tk->E));
                }
                return expr;
            }

            default:
                return std::unexpected(
                    raise_error("Expect expression", current_tk->B, current_tk->E));
        }
    }

    std::expected<Expr *, utils::Diagnostic> parse_unary() {
        if (current_tk->ty == TokenType::tk_plus) {
            size_t B = current_tk->B;
            consume();
            auto rhs = parse_unary();
            if (!rhs.has_value()) {
                return rhs;
            }
            auto [_, E] = (*rhs)->src_range();
            return ctx.make_expr<UnaryExpr>(UnaryExpr::UnaryOp::Add, *rhs, B, E);
        }
        if (current_tk->ty == TokenType::tk_minus) {
            size_t B = current_tk->B;
            consume();
            auto rhs = parse_unary();
            if (!rhs.has_value()) {
                return rhs;
            }
            auto [_, E] = (*rhs)->src_range();
            return ctx.make_expr<UnaryExpr>(UnaryExpr::UnaryOp::Sub, rhs.value(), B, E);
        }
        return parse_primary();
    }

    std::expected<Expr *, utils::Diagnostic> parse_cmp_expr() {
        auto lhs = parse_add_expr();
        if (!lhs) {
            return lhs;
        }

        BinaryExpr::BinaryOp op;
        switch (current_tk->ty) {
            case TokenType::tk_eq: op = BinaryExpr::BinaryOp::Eq; break;
            case TokenType::tk_ne: op = BinaryExpr::BinaryOp::Ne; break;
            case TokenType::tk_lt: op = BinaryExpr::BinaryOp::Lt; break;
            case TokenType::tk_le: op = BinaryExpr::BinaryOp::Le; break;
            case TokenType::tk_gt: op = BinaryExpr::BinaryOp::Gt; break;
            case TokenType::tk_ge: op = BinaryExpr::BinaryOp::Ge; break;
            case TokenType::tk_like: op = BinaryExpr::BinaryOp::Like; break;
            default: return lhs;
        }

        consume();  // operator

        auto rhs = parse_add_expr();
        if (!rhs) {
            return rhs;
        }

        auto [B, _] = (*lhs)->src_range();
        auto [_b, E] = (*rhs)->src_range();
        return ctx.make_expr<BinaryExpr>(op, *lhs, *rhs, B, E);
    }

    std::expected<Expr *, utils::Diagnostic> parse_and_expr() {
        auto lhs = parse_cmp_expr();
        if (!lhs) {
            return lhs;
        }

        while (current_tk->ty == TokenType::tk_and) {
            consume();  // AND
            auto rhs = parse_cmp_expr();
            if (!rhs) {
                return rhs;
            }
            auto [B, _] = (*lhs)->src_range();
            auto [_b, E] = (*rhs)->src_range();
            lhs = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::And, *lhs, *rhs, B, E);
        }
        return lhs;
    }

    std::expected<Expr *, utils::Diagnostic> parse_mul_expr() {
        auto lhs = parse_unary();
        if (!lhs) {
            return lhs;
        }

        while (current_tk->ty == TokenType::tk_star || current_tk->ty == TokenType::tk_slash) {
            BinaryExpr::BinaryOp op = current_tk->ty == TokenType::tk_star
                                          ? BinaryExpr::BinaryOp::Mul
                                          : BinaryExpr::BinaryOp::Div;
            consume();
            auto rhs = parse_unary();
            if (!rhs) {
                return rhs;
            }
            auto [B, _] = (*lhs)->src_range();
            auto [_b, E] = (*rhs)->src_range();
            lhs = ctx.make_expr<BinaryExpr>(op, *lhs, *rhs, B, E);
        }
        return lhs;
    }

    std::expected<Expr *, utils::Diagnostic> parse_add_expr() {
        auto lhs = parse_mul_expr();
        if (!lhs) {
            return lhs;
        }

        while (current_tk->ty == TokenType::tk_plus || current_tk->ty == TokenType::tk_minus) {
            BinaryExpr::BinaryOp op = current_tk->ty == TokenType::tk_plus
                                          ? BinaryExpr::BinaryOp::Add
                                          : BinaryExpr::BinaryOp::Sub;
            consume();
            auto rhs = parse_mul_expr();
            if (!rhs) {
                return rhs;
            }
            auto [B, _] = (*lhs)->src_range();
            auto [_b, E] = (*rhs)->src_range();
            lhs = ctx.make_expr<BinaryExpr>(op, *lhs, *rhs, B, E);
        }
        return lhs;
    }

    std::expected<Expr *, utils::Diagnostic> parse_or_expr() {
        auto lhs = parse_and_expr();
        if (!lhs) {
            return lhs;
        }

        while (current_tk->ty == TokenType::tk_or) {
            consume();  // OR
            auto rhs = parse_and_expr();
            if (!rhs) {
                return rhs;
            }
            auto [B, _] = (*lhs)->src_range();
            auto [_b, E] = (*rhs)->src_range();
            lhs = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Or, *lhs, *rhs, B, E);
        }
        return lhs;
    }

    std::expected<Expr *, utils::Diagnostic> parse_condition() {
        return parse_or_expr();
    }
};

Parser::Parser(std::unique_ptr<ParserImpl> impl) : impl(std::move(impl)) {}

Parser Parser::create(std::vector<Token> &tokens, std::string_view source) {
    return Parser(std::make_unique<ParserImpl>(tokens, source));
}

ASTContext &Parser::context() {
    return impl->context();
}

void Parser::add_stmt(Stmt *S) {
    impl->context().add_stmt(S);
}

Parser::~Parser() = default;

std::vector<utils::Diagnostic> Parser::parse() {
    std::vector<utils::Diagnostic> err{};
    while (impl->can_parse_next()) {
        auto stmt = impl->parse_stmt();
        if (!stmt.has_value()) {
            err.emplace_back(std::move(stmt.error()));
            impl->recover_to_next_stmt();
            continue;
        }
        add_stmt(stmt.value());
    }
    return err;
}

}  // namespace gpamgr
