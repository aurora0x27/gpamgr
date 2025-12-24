#include "sql.h"

#include "test/test.h"

namespace ut {
namespace {
using namespace gpamgr;

static std::vector<utils::Diagnostic> parse_sql(std::string_view sql,
                                                ASTContext *out_ctx = nullptr) {
    auto lexed = lex(sql);
    assert(lexed.has_value());

    auto parser = Parser::create(lexed.value(), sql);
    auto diags = parser.parse();

    if (out_ctx) {
        *out_ctx = std::move(parser.context());
    }
    return diags;
}
}  // namespace

suite<"MiniSQL"> minisql = [] {
    using namespace gpamgr;
    test("lexer.operator") = [] {
        {
            constexpr auto cmd = R"sql(= != < <= > >=)sql";
            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 7);

            expect(t[0].ty == TokenType::tk_eq);
            expect(t[1].ty == TokenType::tk_ne);
            expect(t[2].ty == TokenType::tk_lt);
            expect(t[3].ty == TokenType::tk_le);
            expect(t[4].ty == TokenType::tk_gt);
            expect(t[5].ty == TokenType::tk_ge);
            expect(t[6].ty == TokenType::tk_eof);
        }
    };

    test("lexer.punctuation") = [] {
        {
            constexpr auto cmd = R"sql(, ( ) ; *)sql";
            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 6);

            expect(t[0].ty == TokenType::tk_comma);
            expect(t[1].ty == TokenType::tk_lparen);
            expect(t[2].ty == TokenType::tk_rparen);
            expect(t[3].ty == TokenType::tk_semi);
            expect(t[4].ty == TokenType::tk_star);
            expect(t[5].ty == TokenType::tk_eof);
        }
    };

    test("lexer.keyword_and_identifier") = [] {
        {
            constexpr auto cmd = R"sql(
            SELECT insert Into Update delete
            where FROM like and Or ORDER BY ASC DESC
            SET student_name
        )sql";

            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 17);

            expect(t[0].ty == TokenType::tk_select);
            expect(t[1].ty == TokenType::tk_insert);
            expect(t[2].ty == TokenType::tk_into);
            expect(t[3].ty == TokenType::tk_update);
            expect(t[4].ty == TokenType::tk_delete);
            expect(t[5].ty == TokenType::tk_where);
            expect(t[6].ty == TokenType::tk_from);
            expect(t[7].ty == TokenType::tk_like);
            expect(t[8].ty == TokenType::tk_and);
            expect(t[9].ty == TokenType::tk_or);
            expect(t[10].ty == TokenType::tk_order);
            expect(t[11].ty == TokenType::tk_by);
            expect(t[12].ty == TokenType::tk_asc);
            expect(t[13].ty == TokenType::tk_desc);
            expect(t[14].ty == TokenType::tk_set);

            expect(t[15].ty == TokenType::tk_identifier);

            expect(t[16].ty == TokenType::tk_eof);
        }
    };

    test("lexer.number_literal") = [] {
        {
            constexpr auto cmd = R"sql(0 42 3.14 100.001 -114 +51.4)sql";
            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 9);

            expect(t[0].ty == TokenType::tk_num);
            expect(t[1].ty == TokenType::tk_num);
            expect(t[2].ty == TokenType::tk_num);
            expect(t[3].ty == TokenType::tk_num);
            expect(t[4].ty == TokenType::tk_minus);
            expect(t[5].ty == TokenType::tk_num);
            expect(t[6].ty == TokenType::tk_plus);
            expect(t[7].ty == TokenType::tk_num);
            expect(t[8].ty == TokenType::tk_eof);
        }
    };

    test("lexer.string_literal") = [] {
        {
            constexpr auto cmd = R"sql(
            "hello world"
            'single quoted string'
        )sql";

            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 3);

            expect(t[0].ty == TokenType::tk_string);
            expect(t[1].ty == TokenType::tk_string);
            expect(t[2].ty == TokenType::tk_eof);
        }
    };

    test("lexer.like_expression") = [] {
        {
            constexpr auto cmd = R"sql(
            name LIKE "%Zhang%"
        )sql";

            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();
            expect(t.size() == 4);

            expect(t[0].ty == TokenType::tk_identifier);
            expect(t[1].ty == TokenType::tk_like);
            expect(t[2].ty == TokenType::tk_string);
            expect(t[3].ty == TokenType::tk_eof);
        }
    };

    test("lexer.full_select_statement") = [] {
        {
            constexpr auto cmd =
                R"sql(SELECT sid, name, math FROM student_scores WHERE math >= 60 AND name LIKE "Zhang%";)sql";

            auto tokens_ = lex(cmd);
            expect(tokens_.has_value());

            auto &t = tokens_.value();

            std::vector<TokenType> types;
            for (auto &tk: t) {
                types.push_back(tk.ty);
            }

            expect(types == std::vector<TokenType>{
                                TokenType::tk_select,
                                TokenType::tk_identifier,
                                TokenType::tk_comma,
                                TokenType::tk_identifier,
                                TokenType::tk_comma,
                                TokenType::tk_identifier,
                                TokenType::tk_from,
                                TokenType::tk_identifier,
                                TokenType::tk_where,
                                TokenType::tk_identifier,
                                TokenType::tk_ge,
                                TokenType::tk_num,
                                TokenType::tk_and,
                                TokenType::tk_identifier,
                                TokenType::tk_like,
                                TokenType::tk_string,
                                TokenType::tk_semi,
                                TokenType::tk_eof,
                            });
        }
    };

    test("lexer.sql.select_star") = [] {
        constexpr auto cmd = R"sql(
        SELECT * FROM students;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        auto &t = tokens_.value();
        expect(t.size() == 6);

        expect(t[0].ty == TokenType::tk_select);
        expect(t[1].ty == TokenType::tk_star);
        expect(t[2].ty == TokenType::tk_from);
        expect(t[3].ty == TokenType::tk_identifier);
        expect(t[4].ty == TokenType::tk_semi);
        expect(t[5].ty == TokenType::tk_eof);
    };

    test("lexer.sql.where_single_condition") = [] {
        constexpr auto cmd = R"sql(
        SELECT name FROM student_scores WHERE math >= 60;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        auto &t = tokens_.value();
        bool has_where = false;
        bool has_ge = false;
        bool has_num = false;

        for (auto &tk: t) {
            if (tk.ty == TokenType::tk_where)
                has_where = true;
            if (tk.ty == TokenType::tk_ge)
                has_ge = true;
            if (tk.ty == TokenType::tk_num)
                has_num = true;
        }

        expect(has_where);
        expect(has_ge);
        expect(has_num);
    };

    test("lexer.sql.where_float_condition") = [] {
        constexpr auto cmd = R"sql(
        SELECT sid FROM student_scores WHERE gpa < 3.5;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        bool has_float = false;
        for (auto &tk: tokens_.value()) {
            if (tk.ty == TokenType::tk_num) {
                auto sv = std::string_view(cmd).substr(tk.B, tk.E - tk.B);
                if (sv.find('.') != std::string_view::npos) {
                    has_float = true;
                }
            }
        }

        expect(has_float);
    };

    test("lexer.sql.where_string_eq") = [] {
        constexpr auto cmd = R"sql(
        SELECT sid FROM student_scores WHERE name = "Zhang San";
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        bool has_string = false;
        for (auto &tk: tokens_.value()) {
            if (tk.ty == TokenType::tk_string)
                has_string = true;
        }

        expect(has_string);
    };

    test("lexer.sql.where_like") = [] {
        constexpr auto cmd = R"sql(
        SELECT sid, name FROM student_scores
        WHERE name LIKE "%Zhang%";
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        std::vector<TokenType> seq;
        for (auto &tk: tokens_.value())
            seq.push_back(tk.ty);

        expect(std::find(seq.begin(), seq.end(), TokenType::tk_like) != seq.end());
        expect(std::find(seq.begin(), seq.end(), TokenType::tk_string) != seq.end());
    };

    test("lexer.sql.where_and_or") = [] {
        constexpr auto cmd = R"sql(
        SELECT * FROM student_scores
        WHERE math >= 60 AND english >= 60 OR cs >= 60;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        int and_cnt = 0, or_cnt = 0;
        for (auto &tk: tokens_.value()) {
            if (tk.ty == TokenType::tk_and)
                ++and_cnt;
            if (tk.ty == TokenType::tk_or)
                ++or_cnt;
        }

        expect(and_cnt == 1);
        expect(or_cnt == 1);
    };

    test("lexer.sql.delete") = [] {
        constexpr auto cmd = R"sql(
        DELETE FROM student_scores WHERE sid = 10001;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        expect(tokens_.value()[0].ty == TokenType::tk_delete);
        expect(tokens_.value()[1].ty == TokenType::tk_from);
    };

    test("lexer.sql.update") = [] {
        constexpr auto cmd = R"sql(
        UPDATE student_scores
        SET math = 95
        WHERE sid = 10001;
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        expect(tokens_.value()[0].ty == TokenType::tk_update);
    };

    test("lexer.sql.insert") = [] {
        constexpr auto cmd = R"sql(
        INSERT INTO student_scores VALUES (10001, "Zhang", 90, 85);
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        expect(tokens_.value()[0].ty == TokenType::tk_insert);
    };

    test("lexer.sql.complex_query") = [] {
        constexpr auto cmd = R"sql(
        SELECT sid, name, math, english
        FROM student_scores
        WHERE (math >= 60 AND english >= 60)
           OR name LIKE "Li%";
    )sql";

        auto tokens_ = lex(cmd);
        expect(tokens_.has_value());

        // sanity checks
        expect(tokens_.value().size() > 10);
    };

    test("Parser.SELECT") = [] {
        {
            ASTContext ctx;
            auto diags = parse_sql("select id from student;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }
        {
            ASTContext ctx;
            auto diags = parse_sql("select id, score, name from student;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }
        {
            ASTContext ctx;
            auto diags = parse_sql("select * from student;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }
        {
            ASTContext ctx;
            auto diags = parse_sql("select id from student where score > 90;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }
        {
            auto diags = parse_sql("select id from student order by score;");
            expect(diags.empty());
        }
        {
            auto diags = parse_sql("select id from student order by score desc, id asc;");
            expect(diags.empty());
        }
        {
            auto diags = parse_sql("select id from student");
            expect(diags.size() == 1);
            diags[0].display();
        }
        {
            auto diags = parse_sql("select id student;");
            expect(!diags.empty());
        }
        {
            auto diags = parse_sql("select id from student order by ;");
            expect(!diags.empty());
        }
        {
            auto diags = parse_sql("select id from student where;");
            expect(!diags.empty());
        }
        {
            ASTContext ctx;
            auto diags = parse_sql(
                R"(select id from student; select from student; select id, score, name from teacher;)",
                &ctx);
            expect(diags.size() == 1);
            diags[0].display();
            expect(ctx.get_stmts().size() == 2);
        }
        {
            ASTContext ctx;
            auto diags = parse_sql("select id, score from student;", &ctx);
            expect(diags.empty());

            auto stmts = ctx.get_stmts();
            expect(stmts.size() == 1);

            expect(stmts[0]->get_kind() == gpamgr::Stmt::StmtKind::SelectStmtKind);
            SelectStmt *sel = (SelectStmt *)(stmts[0]);
            expect(sel);
            expect(sel->select_list.size() == 2);
            expect(sel->from->name == "student");
        }
    };

    test("Parser.INSERT") = [] {
        {
            ASTContext ctx;
            auto diags = parse_sql("insert into student values (1, 90);", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql("insert into student values (1, 90, \"alice\");", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql("insert into student values (100);", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        // -------- syntax errors --------

        {
            // missing VALUES
            auto diags = parse_sql("insert into student (1, 90);");
            expect(!diags.empty());
            diags[0].display();
        }

        {
            // missing INTO
            auto diags = parse_sql("insert student values (1, 90);");
            expect(!diags.empty());
        }

        {
            // missing table name
            auto diags = parse_sql("insert into values (1, 90);");
            expect(!diags.empty());
        }

        {
            // empty value list
            auto diags = parse_sql("insert into student values ();");
            expect(!diags.empty());
        }

        {
            // missing closing paren
            auto diags = parse_sql("insert into student values (1, 90;");
            expect(!diags.empty());
        }

        {
            // missing semicolon → warning
            auto diags = parse_sql("insert into student values (1, 90)");
            expect(diags.size() == 1);
            diags[0].display();
        }

        // -------- recover test --------

        {
            ASTContext ctx;
            auto diags = parse_sql(
                R"( insert into student values (1, 90); insert into student values (); insert into teacher values (10, "bob");)",
                &ctx);

            expect(diags.size() == 1);
            expect(ctx.get_stmts().size() == 2);
            diags[0].display();
        }

        // -------- AST shape check --------

        {
            ASTContext ctx;
            auto diags = parse_sql("insert into student values (1, 90, 100);", &ctx);
            expect(diags.empty());

            auto stmts = ctx.get_stmts();
            expect(stmts.size() == 1);
            expect(stmts[0]->get_kind() == gpamgr::Stmt::StmtKind::InsertStmtKind);

            InsertStmt *ins = (InsertStmt *)stmts[0];
            expect(ins);
            expect(ins->values.size() == 3);
            expect(ins->tb_name->name == "student");
        }
    };

    test("Parser.UPDATE") = [] {
        {
            ASTContext ctx;
            auto diags = parse_sql("update student set score = 100;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql(
                "update student set physics = 100, chemistry = 95, biology = 97 where id = 1;",
                &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql("update student set name = \"alice\" where score >= 90;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        // -------- syntax errors --------

        {
            // missing SET
            auto diags = parse_sql("update student score = 100;");
            expect(!diags.empty());
            diags[0].display();
        }

        {
            // missing '='
            auto diags = parse_sql("update student set score 100;");
            expect(!diags.empty());
        }

        {
            // missing field name
            auto diags = parse_sql("update student set = 100;");
            expect(!diags.empty());
        }

        {
            // missing value
            auto diags = parse_sql("update student set score =;");
            expect(!diags.empty());
        }

        {
            // where without condition
            auto diags = parse_sql("update student set score = 100 where;");
            expect(!diags.empty());
        }

        {
            // missing semicolon
            auto diags = parse_sql("update student set score = 100");
            expect(diags.size() == 1);
            diags[0].display();
        }

        // -------- recover test --------

        {
            ASTContext ctx;
            auto diags = parse_sql(
                R"( update student set score = 100; update student set score =; update student set name = "bob";)",
                &ctx);

            expect(diags.size() == 1);
            expect(ctx.get_stmts().size() == 2);
            diags[0].display();
        }

        // -------- AST shape check --------

        {
            ASTContext ctx;
            auto diags = parse_sql("update student set score = 88 where id = 3;", &ctx);
            expect(diags.empty());

            auto stmts = ctx.get_stmts();
            expect(stmts.size() == 1);
            expect(stmts[0]->get_kind() == gpamgr::Stmt::StmtKind::UpdateStmtKind);

            UpdateStmt *upd = (UpdateStmt *)stmts[0];
            expect(upd);
            expect(upd->tb_name->name == "student");
            expect(upd->assigns[0].field->name == "score");
            expect(upd->cond != nullptr);
        }
    };

    test("Parser.DELETE") = [] {
        {
            ASTContext ctx;
            auto diags = parse_sql("delete from student;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql("delete from student where id = 1;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        {
            ASTContext ctx;
            auto diags = parse_sql("delete from student where score < 60;", &ctx);
            expect(diags.empty());
            expect(ctx.get_stmts().size() == 1);
        }

        // -------- syntax errors --------

        {
            // missing FROM
            auto diags = parse_sql("delete student;");
            expect(!diags.empty());
            diags[0].display();
        }

        {
            // missing table name
            auto diags = parse_sql("delete from;");
            expect(!diags.empty());
        }

        {
            // where without condition
            auto diags = parse_sql("delete from student where;");
            expect(!diags.empty());
        }

        {
            // missing semicolon
            auto diags = parse_sql("delete from student");
            expect(diags.size() == 1);
            diags[0].display();
        }

        // -------- recover test --------

        {
            ASTContext ctx;
            auto diags = parse_sql(
                R"( delete from student; delete from student where; delete from teacher where id = 2; )",
                &ctx);

            expect(diags.size() == 1);
            expect(ctx.get_stmts().size() == 2);
            diags[0].display();
        }

        // -------- AST shape check --------

        {
            ASTContext ctx;
            auto diags = parse_sql("delete from student where id = 10;", &ctx);
            expect(diags.empty());

            auto stmts = ctx.get_stmts();
            expect(stmts.size() == 1);
            expect(stmts[0]->get_kind() == gpamgr::Stmt::StmtKind::DeleteStmtKind);

            DeleteStmt *del = (DeleteStmt *)stmts[0];
            expect(del);
            expect(del->tb_name->name == "student");
            expect(del->cond != nullptr);
        }
    };

};

}  // namespace ut
