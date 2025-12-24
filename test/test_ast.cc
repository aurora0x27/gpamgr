#include "ast.h"
#include "ast_dumper.h"

#include "test/test.h"

#include <iostream>

namespace ut {
suite<"AST"> ast_test = [] {
    test("ASTDumper") = [] {
        using namespace gpamgr;
        ASTContext ctx;
        {
            // select id, score from student where score > 90;
            auto *col1 = ctx.make_expr<IdentifierExpr>("id", 0, 0);
            auto *col2 = ctx.make_expr<IdentifierExpr>("score", 0, 0);

            auto *from = ctx.make_expr<IdentifierExpr>("student", 0, 0);

            auto *cond = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Gt,
                                                   ctx.make_expr<IdentifierExpr>("score", 0, 0),
                                                   ctx.make_expr<IntegerLiteral>(90, 0, 0),
                                                   0,
                                                   0);

            auto cols = std::vector<Expr *>{col1, col2};
            auto *stmt = ctx.make_stmt<SelectStmt>(std::span<Expr *>{cols.begin(), cols.end()},
                                                   from,
                                                   cond,
                                                   std::nullopt,
                                                   0,
                                                   0);

            ASTDumper dumper(std::cout);
            dumper.visit(stmt);
        }

        {
            /// SELECT id, score, age
            /// FROM student
            /// WHERE (score > 90 AND age >= 18) OR name LIKE "A%";

            // select list
            auto *id = ctx.make_expr<IdentifierExpr>("id", 0, 0);
            auto *score = ctx.make_expr<IdentifierExpr>("score", 0, 0);
            auto *age = ctx.make_expr<IdentifierExpr>("age", 0, 0);

            // from
            auto *from = ctx.make_expr<IdentifierExpr>("student", 0, 0);

            // (score > 90)
            auto *c1 = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Gt,
                                                 ctx.make_expr<IdentifierExpr>("score", 0, 0),
                                                 ctx.make_expr<IntegerLiteral>(90, 0, 0),
                                                 0,
                                                 0);

            // (age >= 18)
            auto *c2 = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Ge,
                                                 ctx.make_expr<IdentifierExpr>("age", 0, 0),
                                                 ctx.make_expr<IntegerLiteral>(18, 0, 0),
                                                 0,
                                                 0);

            // (score > 90 AND age >= 18)
            auto *and_expr = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::And, c1, c2, 0, 0);

            // (name LIKE "A%")
            auto *like_expr = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Like,
                                                        ctx.make_expr<IdentifierExpr>("name", 0, 0),
                                                        ctx.make_expr<StringLiteral>("A%", 0, 0),
                                                        0,
                                                        0);

            // ( ... ) OR ( ... )
            auto *where =
                ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Or, and_expr, like_expr, 0, 0);

            // stmt
            std::vector<Expr *> cols{id, score, age};

            auto *stmt =
                ctx.make_stmt<SelectStmt>(std::span<Expr *>{cols}, from, where, std::nullopt, 0, 0);

            ASTDumper(std::cout).visit(stmt);
        }

        {
            /// SELECT id, score
            /// FROM student
            /// ORDER BY score DESC, id ASC

            auto *id = ctx.make_expr<IdentifierExpr>("id", 0, 0);
            auto *score = ctx.make_expr<IdentifierExpr>("score", 0, 0);
            auto *from = ctx.make_expr<IdentifierExpr>("student", 0, 0);

            OrderByClause order;
            order.keys.push_back({"score", false});  // DESC
            order.keys.push_back({"id", true});      // ASC

            std::vector<Expr *> cols{id, score};

            auto *stmt = ctx.make_stmt<SelectStmt>(std::span<Expr *>{cols},
                                                   from,
                                                   nullptr,
                                                   std::nullopt,
                                                   0,
                                                   0);

            stmt->sort = order;

            ASTDumper(std::cout).visit(stmt);
        }

        {
            /// UPDATE student
            /// SET score = score + 5
            /// WHERE score < 60

            auto *tb = ctx.make_expr<IdentifierExpr>("student", 0, 0);

            // clang-format off
            std::vector<Assignment> assigns = {
                {
                    .field = ctx.make_expr<IdentifierExpr>("physics", 0, 0),
                    .value = ctx.make_expr<IntegerLiteral>(97, 0, 0),
                },
                {
                    .field = ctx.make_expr<IdentifierExpr>("chemistry", 0, 0),
                    .value = ctx.make_expr<IntegerLiteral>(95, 0, 0),
                },

                {
                    .field = ctx.make_expr<IdentifierExpr>("biology", 0, 0),
                    .value = ctx.make_expr<IntegerLiteral>(93, 0, 0),
                },
            };
            // clang-format on

            // WHERE score < 60
            auto *cond = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Lt,
                                                   ctx.make_expr<IdentifierExpr>("score", 0, 0),
                                                   ctx.make_expr<IntegerLiteral>(60, 0, 0),
                                                   0,
                                                   0);

            auto *stmt =
                ctx.make_stmt<UpdateStmt>(tb,
                                          std::span<Assignment>{assigns.begin(), assigns.end()},
                                          cond,
                                          0,
                                          0);

            ASTDumper(std::cout).visit(stmt);
        }

        {
            /// DELETE FROM student
            /// WHERE score < 60 OR age < 18

            auto *tb = ctx.make_expr<IdentifierExpr>("student", 0, 0);

            auto *c1 = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Lt,
                                                 ctx.make_expr<IdentifierExpr>("score", 0, 0),
                                                 ctx.make_expr<IntegerLiteral>(60, 0, 0),
                                                 0,
                                                 0);

            auto *c2 = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Lt,
                                                 ctx.make_expr<IdentifierExpr>("age", 0, 0),
                                                 ctx.make_expr<IntegerLiteral>(18, 0, 0),
                                                 0,
                                                 0);

            auto *cond = ctx.make_expr<BinaryExpr>(BinaryExpr::BinaryOp::Or, c1, c2, 0, 0);

            auto *stmt = ctx.make_stmt<DeleteStmt>(tb, cond, 0, 0);

            ASTDumper(std::cout).visit(stmt);
        }
    };
};
}  // namespace ut
