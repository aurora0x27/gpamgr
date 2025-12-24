#include "driver.h"

#include "test/test.h"

namespace ut {
namespace {
using namespace gpamgr;

static Table::SchemaDesc make_schema() {
    Table::SchemaDesc desc;
    desc.fields = {
        {"id",    Table::FieldType::INT,    true },
        {"name",  Table::FieldType::STRING, false},
        {"score", Table::FieldType::FLOAT,  false},
    };
    return desc;
}
}  // namespace

suite<"ScriptDriver"> driver_test = [] {
    test("curr_table: empty at start") = [] {
        ScriptDriver drv;
        auto r = drv.curr_table();
        expect(!r.has_value());
    };

    test("create_table: success") = [] {
        ScriptDriver drv;

        auto t = drv.create_table("t1", make_schema());
        expect(t.has_value());
        expect(t.value() != nullptr);

        auto curr = drv.curr_table();
        expect(curr.has_value());
        expect(curr.value() == t.value());
    };

    test("create_table: duplicate name fails") = [] {
        ScriptDriver drv;

        expect(drv.create_table("t1", make_schema()).has_value());
        auto r = drv.create_table("t1", make_schema());

        expect(!r.has_value());
    };

    test("load_table: schema missing") = [] {
        ScriptDriver drv;
        auto tbl = drv.create_table("tbl_empty", {});
        expect(tbl.has_value());
        tbl.value()->flush();
        auto r = drv.load_table("tbl_empty.gpa");
        expect(r.has_value());
    };

    test("load_table: same table loaded twice") = [] {
        ScriptDriver drv;
        auto _ = drv.create_table("tbl", make_schema());
        auto t1 = drv.load_table("tbl.gpa");
        auto t2 = drv.load_table("tbl.gpa");
        if (!t1.has_value()) {
            std::cout << t1.error() << '\n';
        }
        expect(t1.has_value());
        expect(t1.value() == t2.value());
    };

    test("set_table: success") = [] {
        ScriptDriver drv;

        auto t1 = drv.create_table("a", make_schema()).value();
        auto t2 = drv.create_table("b", make_schema()).value();

        auto r = drv.set_table("a");
        expect(r.has_value());
        expect(r.value() == t1);

        auto curr = drv.curr_table();
        expect(curr.value() == t1);

        auto _ = drv.set_table("b");
        expect(drv.curr_table().value() == t2);
    };

    test("set_table: non-exist fails") = [] {
        ScriptDriver drv;

        auto r = drv.set_table("nope");
        expect(!r.has_value());
    };

    test("curr_table_mut: writable") = [] {
        ScriptDriver drv;
        auto t = drv.create_table("t", make_schema()).value();

        auto mut = drv.curr_table_mut();
        expect(mut.has_value());

        Table::Value v1{int64_t(1)};
        Table::Value v2{std::string("Foo")};
        Table::Value v3{3.14};
        auto vec = std::vector<Table::Value>{v1, v2, v3};
        auto id = mut.value()->insert(vec);
        expect(id.has_value());
    };

    test("erase_table: success") = [] {
        ScriptDriver drv;

        auto _ = drv.create_table("t1", make_schema());
        auto err = drv.erase_table("t1");

        expect(!err.has_value());

        auto r = drv.set_table("t1");
        expect(!r.has_value());
    };

    test("erase_table: erase current clears curr") = [] {
        ScriptDriver drv;

        auto _ = drv.create_table("t1", make_schema());
        expect(drv.curr_table().has_value());

        drv.erase_table("t1");

        expect(!drv.curr_table().has_value());
    };

    test("erase_table: non-exist") = [] {
        ScriptDriver drv;
        auto err = drv.erase_table("ghost");
        expect(err.has_value());
    };

    test("create schema and read") = [] {
        {
            ScriptDriver drv;
            auto t = drv.create_table("rw", make_schema()).value();
            auto mut = drv.curr_table_mut();
            {
                Table::Value v1{int64_t(1)};
                Table::Value v2{std::string("Foo")};
                Table::Value v3{3.14};
                auto vec = std::vector<Table::Value>{v1, v2, v3};
                auto id = mut.value()->insert(vec);
            }

            {
                Table::Value v1{int64_t(2)};
                Table::Value v2{std::string("Bar")};
                Table::Value v3{1.14};
                auto vec = std::vector<Table::Value>{v1, v2, v3};
                auto id = mut.value()->insert(vec);
            }

            {
                Table::Value v1{int64_t(3)};
                Table::Value v2{std::string("Baz")};
                Table::Value v3{5.14};
                auto vec = std::vector<Table::Value>{v1, v2, v3};
                auto id = mut.value()->insert(vec);
            }
        }
        {
            ScriptDriver drv;
            auto t = drv.load_table("rw.gpa");
            expect(t.has_value());
            auto tb = t.value();
            drv.debug_dump();
            {
                auto row = tb->find_by_pk(Table::Value{int64_t(1)});
                expect(row.has_value());
                if (row.has_value()) {
                    expect(row.value()->content.size() == 3);
                    std::cout << "id    is: ";
                    row.value()->content[0].display(std::cout);
                    std::cout << '\n';
                    std::cout << "score is: ";
                    row.value()->content[1].display(std::cout);
                    std::cout << '\n';
                }
            }
            {
                auto row = tb->find_by_pk(Table::Value{int64_t(2)});
                expect(row.has_value());
                if (row.has_value()) {
                    expect(row.value()->content.size() == 3);
                    std::cout << "id    is: ";
                    row.value()->content[0].display(std::cout);
                    std::cout << '\n';
                    std::cout << "score is: ";
                    row.value()->content[1].display(std::cout);
                    std::cout << '\n';
                }
            }
            {
                auto row = tb->find_by_pk(Table::Value{int64_t(3)});
                expect(row.has_value());
                if (row.has_value()) {
                    expect(row.value()->content.size() == 3);
                    std::cout << "id    is: ";
                    row.value()->content[0].display(std::cout);
                    std::cout << '\n';
                    std::cout << "score is: ";
                    row.value()->content[1].display(std::cout);
                    std::cout << '\n';
                }
            }
        }
    };
};
}  // namespace ut
