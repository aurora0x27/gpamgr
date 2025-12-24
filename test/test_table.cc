#include "table.h"

#include "test/test.h"

#include <cstdint>

namespace ut {
namespace {
using namespace gpamgr;
using Value = gpamgr::Table::Value;

static Table make_basic_table() {
    using FT = Table::FieldType;
    return Table::create_in_memory({
        {
         {"id", FT::INT, true},
         {"score", FT::FLOAT, false},
         }
    });
}

}  // namespace

suite<"Table"> table = [] {
    using namespace gpamgr;

    constexpr double EPS = 1e-6;
#define abs(n) ((n) > 0 ? (n) : -(n))
#define float_eq(a, b) (abs(a - b) < EPS)
    test("Values") = [&] {
        using FT = gpamgr::Table::FieldType;
        {
            auto v1 = Table::Value::from_text(FT::FLOAT, "4637758623307630903");
            expect(v1.has_value());
            auto tmp = v1.value().get<double>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(float_eq(tmp->get(), 114.514));
            }
        }

        {
            auto v1 = Table::Value::from_text(FT::FLOAT, "4614256655080292474");
            expect(v1.has_value());
            auto tmp = v1.value().get<double>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(float_eq(tmp->get(), 3.141592));
            }
        }

        {
            auto v1 = Table::Value::from_text(FT::INT, "114");
            expect(v1.has_value());
            auto tmp = v1.value().get<int64_t>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(tmp->get() == 114);
            }
        }

        {
            auto v1 = Table::Value::from_text(FT::STRING, R"STR("hello world")STR");
            expect(v1.has_value());
            auto tmp = v1.value().get<std::string>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(tmp->get() == "hello world");
            }
        }

        {
            // FLOAT: 114.514
            std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

            Table::Value v;
            v.inner = 114.514;

            v.dump_binary(ss);

            ss.seekg(0);

            auto v2 = Table::Value::from_binary(FT::FLOAT, ss);
            auto tmp = v2.get<double>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(float_eq(tmp->get(), 114.514));
            }
        }

        {
            // FLOAT: pi
            std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

            Table::Value v;
            v.inner = 3.141592;

            v.dump_binary(ss);

            ss.seekg(0);

            auto v2 = Table::Value::from_binary(FT::FLOAT, ss);

            auto tmp = v2.get<double>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(float_eq(tmp->get(), 3.141592));
            }
        }

        {
            // INT
            std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

            Table::Value v;
            v.inner = int64_t{114};

            v.dump_binary(ss);

            ss.seekg(0);

            auto v2 = Table::Value::from_binary(FT::INT, ss);
            auto tmp = v2.get<int64_t>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(tmp->get() == 114);
            }
        }

        {
            // STRING
            std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

            Table::Value v;
            v.inner = std::string("hello world");

            v.dump_binary(ss);

            ss.seekg(0);

            auto v2 = Table::Value::from_binary(FT::STRING, ss);
            auto tmp = v2.get<std::string>();
            expect(tmp.has_value());
            if (tmp.has_value()) {
                expect(tmp->get() == "hello world");
            }
        }
    };

    test("SchemaDrivenRow") = [&] {
        Table::Field f1{"id", Table::FieldType::INT, true};
        Table::Field f2{"score", Table::FieldType::FLOAT, false};

        std::vector<Table::Field> schema = {f1, f2};

        std::vector<Table::Value> row;
        row.push_back(Table::Value{int64_t(1)});
        row.push_back(Table::Value{3.14});

        expect(row[0].is(Table::FieldType::INT));
        expect(row[1].is(Table::FieldType::FLOAT));
    };

    test("EmptyTable") = [&] {
        auto t = make_basic_table();

        bool called = false;
        t.scan([&](const Table::Row &) { called = true; });

        expect(!called);

        auto r = t.find_by_id(1);
        expect(!r.has_value());
    };

    test("ManyInserts") = [&] {
        auto t = make_basic_table();
        for (int64_t i = 1; i <= 1000; ++i) {
            std::vector<Value> data = {Value{i}, Value{0.0}};
            expect(t.insert(data).has_value());
        }
        int count = 0;
        t.scan([&count](const Table::Row &) { ++count; });
        expect(count == 1000);
    };

    test("InsertAndScanOrder") = [&] {
        auto t = make_basic_table();

        std::vector<Value> l1{
            Table::Value{int64_t(1)},
            Table::Value{1.0},
        };

        auto r1 = t.insert(l1);

        std::vector<Value> l2{
            Table::Value{int64_t(2)},
            Table::Value{2.0},
        };
        auto r2 = t.insert(l2);

        expect(r1.has_value());
        expect(r2.has_value());
        expect(r1.value() == 1);
        expect(r2.value() == 2);

        std::vector<RowId> seen;
        t.scan([&](const Table::Row &r) { seen.push_back(r.id); });

        expect(seen.size() == 2);
        expect(seen[0] == 1);
        expect(seen[1] == 2);
    };

    test("FreeSlotReuse") = [&] {
        auto t = make_basic_table();

        // 1. Prepare and insert initial data
        std::vector<Value> v1 = {Value{int64_t(1)}, Value{10.0}};
        auto r1 = t.insert(v1);

        std::vector<Value> v2 = {Value{int64_t(2)}, Value{20.0}};
        auto r2 = t.insert(v2);

        std::vector<Value> v3 = {Value{int64_t(3)}, Value{30.0}};
        auto r3 = t.insert(v3);

        auto count = t.rows_physical_size();
        std::cout << t.rows_physical_size() << std::endl;
        expect(r1.has_value() && r2.has_value() && r3.has_value());
        // 2. Erase the middle row (ID 2) using captured RowId
        auto res = t.erase_row(r2.value());
        expect(res.has_value());

        // 3. Reuse the slot with a new row
        std::vector<Value> v4 = {Value{int64_t(4)}, Value{40.0}};
        auto r4 = t.insert(v4);
        expect(r4.has_value());

        // 4. Verify physical storage reuse
        std::cout << t.rows_physical_size() << std::endl;
        expect(t.rows_physical_size() == count);

        auto found = t.find_by_pk(Value{int64_t(4)});
        expect(found.has_value());
    };

    test("DeleteAndScanConsistency") = [&] {
        auto t = make_basic_table();
        std::vector<RowId> ids;

        for (int64_t i = 1; i <= 5; ++i) {
            std::vector<Value> row_data = {Value{i}, Value{double(i * 2.0)}};
            auto res = t.insert(row_data);
            ids.push_back(res.value());
        }

        // Delete Head, Middle, and Tail to test link list pointer integrity
        expect(t.erase_row(ids[0]).has_value());  // Row 1
        expect(t.erase_row(ids[2]).has_value());  // Row 3
        expect(t.erase_row(ids[4]).has_value());  // Row 5

        std::vector<uint64_t> seen;
        t.scan([&](const Table::Row &r) {
            auto val = r.content[0].sget<int64_t>();
            if (val.has_value()) {
                seen.push_back(*val);
            }
        });

        // Verify remaining rows and order (2 and 4)
        expect(seen.size() == 2);
        expect(seen[0] == 2);
        expect(seen[1] == 4);
    };

    test("PrimaryKeyViolation") = [&] {
        auto t = make_basic_table();

        std::vector<Value> data1 = {Value{int64_t(100)}, Value{1.0}};
        auto r1 = t.insert(data1);
        expect(r1.has_value());

        // Attempting to insert duplicate PK
        std::vector<Value> data2 = {Value{int64_t(100)}, Value{2.0}};
        auto r2 = t.insert(data2);
        expect(!r2.has_value());

        // Clean up and re-insert
        expect(t.erase_row(r1.value()).has_value());

        std::vector<Value> data3 = {Value{int64_t(100)}, Value{3.0}};
        auto r3 = t.insert(data3);
        expect(r3.has_value());
    };

    test("ScanStructAction") = [&] {
        auto t = make_basic_table();
        for (int64_t i = 1; i <= 10; ++i) {
            std::vector<Value> data = {Value{i}, Value{0.0}};
            expect(t.insert(data).has_value());
        }

        // Use scan_struct to remove even IDs
        t.scan_struct([](Table::Row &r) {
            auto id_val = r.content[0].get<int64_t>();
            if (id_val && id_val->get() % 2 == 0) {
                return Table::ScanAction::Delete;
            }
            return Table::ScanAction::Keep;
        });

        size_t count = 0;
        t.scan([&](const Table::Row &) { count++; });
        expect(count == 5);
    };
};
}  // namespace ut
