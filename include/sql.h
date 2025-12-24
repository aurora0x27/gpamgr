#pragma once

/// MiniSQL BNF Syntax
///
/// sql_stmt
///     ::= select_stmt
///      |  insert_stmt
///      |  update_stmt
///      |  delete_stmt
///      ;
///
/// select_stmt
///     ::= SELECT select_list
///         FROM identifier
///         [ WHERE condition ]
///         [ ORDER BY order_list ]
///         ;
///
/// insert_stmt
///     ::= INSERT INTO identifier
///         VALUES "(" value_list ")"
///         ;
///
/// update_stmt
///     ::= UPDATE identifier
///         SET identifier "=" value
///         [ WHERE condition ]
///         ;
///
/// delete_stmt
///     ::= DELETE FROM identifier
///         [ WHERE condition ]
///         ;
///
/// select_list
///     ::= "*" | identifier ("," identifier)* ;
///
/// order_list
///     ::= order_item ("," order_item)* ;
///
/// order_item
///     ::= identifier [ ASC | DESC ] ;
///
/// condition
///     ::= or_expr ;
///
/// or_expr
///     ::= and_expr ( OR and_expr )* ;
///
/// and_expr
///     ::= comparison ( AND comparison )* ;
///
/// comparison
///     ::= operand compare_op operand
///      |  operand LIKE string
///      ;
///
/// operand
///     ::= identifier | number | string ;
///
/// value_list
///     ::= value ("," value)* ;
///
/// value
///     ::= number | string ;

#include "misc.h"
#include "ast.h"

#include <vector>
#include <memory>
#include <ostream>
#include <expected>

namespace gpamgr {

enum class TokenType : uint8_t {
    tk_eof,

    // keywords
    tk_select,
    tk_insert,
    tk_update,
    tk_delete,
    tk_values,
    tk_where,
    tk_from,
    tk_into,
    tk_like,
    tk_set,
    tk_order,
    tk_by,
    tk_asc,
    tk_desc,
    tk_and,
    tk_or,

    // id && literals
    tk_identifier,
    tk_num,
    tk_string,

    // op
    tk_eq,
    tk_ne,
    tk_le,  // <=
    tk_ge,  // >=
    tk_lt,  // <
    tk_gt,  // >
    tk_plus,
    tk_minus,
    tk_star,
    tk_slash,

    // punctuation
    tk_comma,
    tk_lparen,
    tk_rparen,
    tk_semi,

    tk_count
};

struct Token {
    TokenType ty;
    size_t B;
    size_t E;

    [[nodiscard]] std::pair<size_t, size_t> src_range() const {
        return {B, E};
    }
};

std::expected<std::vector<Token>, utils::Diagnostic> lex(std::string_view sql);

class Parser {
    class ParserImpl;
    std::unique_ptr<ParserImpl> impl;
    void add_stmt(Stmt *S);

public:
    static Parser create(std::vector<Token> &tokens, std::string_view source);
    explicit Parser(std::unique_ptr<ParserImpl> impl);
    ~Parser();
    std::vector<utils::Diagnostic> parse();
    void dump();
    void dump(std::ostream &os);
    ASTContext &context();
};

}  // namespace gpamgr
