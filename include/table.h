#pragma once

#include "misc.h"
#include "log.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <variant>
#include <cstdint>
#include <expected>
#include <ostream>
#include <istream>
#include <unordered_map>
#include <string_view>
#include <functional>

namespace gpamgr {
using RowId = uint64_t;

class Table {
    friend class ScriptDriver;

public:
    struct Value;
    struct Field;
    struct Row;

    Table(std::string_view name) : tb_name(name) {}

    Table(std::string_view name, std::string_view file) : tb_name(name), file_on_disk(file) {}

    ~Table() {
        if (!file_on_disk.empty()) {
            flush();
        }
    }

    struct SchemaDesc {
        std::vector<Field> fields;
    };

    enum class FieldType : int {
        INT,
        STRING,
        FLOAT,
    };

    static auto field_ty_as_string(FieldType ty) {
        switch (ty) {
            case FieldType::STRING: return "STRING";
            case FieldType::INT: return "INT";
            case FieldType::FLOAT: return "FLOAT";
            default: std::abort();
        }
    }

    struct Field {
        std::string name;
        FieldType type;
        // size_t col;
        bool is_primary = false;
    };

    struct Value {
        std::variant<int64_t, double, std::string> inner;
        FieldType type;
        Value() = default;

        explicit Value(int64_t v) : inner(v), type(FieldType::INT) {}

        explicit Value(double v) : inner(v), type(FieldType::FLOAT) {}

        explicit Value(std::string v) : inner(std::move(v)), type(FieldType::STRING) {}

        explicit Value(const char *v) : inner(std::string(v)), type(FieldType::STRING) {}

        const int64_t *as_int() const {
            return std::get_if<int64_t>(&inner);
        }

        const double *as_double() const {
            return std::get_if<double>(&inner);
        }

        const std::string *as_string() const {
            return std::get_if<std::string>(&inner);
        }

        template <typename T>
        std::optional<std::reference_wrapper<T>> get() {
            if (auto *p = std::get_if<T>(&inner)) {
                return std::ref(*p);
            }
            return std::nullopt;
        }

        template <typename T>
        std::optional<std::reference_wrapper<const T>> sget() const {
            if (auto *p = std::get_if<T>(&inner)) {
                return std::cref(*p);
            }
            return std::nullopt;
        }

        bool is(FieldType ty) const {
            return type == ty;
            // switch (ty) {
            //     case FieldType::FLOAT: return nullptr != std::get_if<double>(&inner);
            //     case FieldType::INT: return nullptr != std::get_if<int64_t>(&inner);
            //     case FieldType::STRING: return nullptr != std::get_if<std::string>(&inner);
            //     default: std::abort();
            // }
        }

        bool operator== (FieldType ty) const {
            return is(ty);
        }

        bool operator== (const Value &other) const {
            return inner == other.inner;
        }

        static Value from_binary(FieldType type, std::istream &is) {
            Value v;
            switch (type) {
                case FieldType::INT: {
                    int64_t x;
                    is.read(reinterpret_cast<char *>(&x), sizeof(x));
                    logging::trace("Read INT value `{}`", x);
                    v.inner = x;
                    v.type = FieldType::INT;
                    break;
                }
                case FieldType::FLOAT: {
                    double d;
                    is.read(reinterpret_cast<char *>(&d), sizeof(d));
                    logging::trace("Read FLOAT value `{}`", d);
                    v.inner = d;
                    v.type = FieldType::FLOAT;
                    break;
                }
                case FieldType::STRING: {
                    uint32_t len;
                    is.read(reinterpret_cast<char *>(&len), sizeof(len));
                    std::string s(len, '\0');
                    is.read(s.data(), len);
                    logging::trace("Read STRING value `{}`", s);
                    v.inner = std::move(s);
                    v.type = FieldType::STRING;
                    break;
                }
            }
            return v;
        }

        void dump_binary(std::ostream &os) const {
            std::visit(
                [&](auto &&v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        uint32_t len = v.size();
                        os.write(reinterpret_cast<const char *>(&len), sizeof(len));
                        os.write(v.data(), len);
                    } else {
                        os.write(reinterpret_cast<const char *>(&v), sizeof(v));
                    }
                },
                inner);
        }

        static std::expected<Value, std::string> from_text(FieldType type, std::string_view sv) {
            Value v;
            auto ss = std::stringstream(std::string(sv));

            switch (type) {
                case FieldType::INT: {
                    int64_t x;
                    if (!(ss >> x)) {
                        return std::unexpected("Invalid Int");
                    }
                    v.inner = x;
                    v.type = FieldType::INT;
                    break;
                }
                case FieldType::FLOAT: {
                    uint64_t bits;
                    if (!(ss >> bits)) {
                        return std::unexpected("Invalid Float");
                    }
                    v.inner = std::bit_cast<double>(bits);
                    v.type = FieldType::FLOAT;
                    break;
                }
                case FieldType::STRING: {
                    std::string s;
                    if (!(ss >> std::quoted(s))) {
                        return std::unexpected("Invalid String");
                    }
                    v.inner = std::move(s);
                    v.type = FieldType::STRING;
                    break;
                }
            }
            return v;
        }

        void dump_text(std::ostream &os) const {
            switch (type) {
                case FieldType::INT: os << std::get<int64_t>(inner); break;
                case FieldType::FLOAT: {
                    uint64_t bits = std::bit_cast<uint64_t>(std::get<double>(inner));
                    os << bits;
                    break;
                }
                case FieldType::STRING: os << std::quoted(std::get<std::string>(inner)); break;
            }
        }

        void display(std::ostream &os) const {
            std::visit(
                [&](auto &&v) {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        os << utils::StyledText(v).green();
                    } else {
                        os << utils::StyledText::format("{}", v).green();
                    }
                },
                inner);
        }
    };

    struct Row {
        RowId id;
        RowId next;
        RowId prev;
        std::vector<Value> content;
        bool expired;
    };

    static Table create_in_memory(SchemaDesc schema) {
        Table t(":memory:");
        t.schema = std::move(schema.fields);

        for (size_t i = 0; i < t.schema.size(); ++i) {
            if (t.schema[i].is_primary) {
                t.primary_field = i;
                break;
            }
        }

        return t;
    }

    static std::expected<Table, std::string> create(std::string_view tb_name, std::ifstream &ifs);

    static std::expected<Table, std::string> create(std::string_view tb_name,
                                                    std::string_view file);

    // Logic const, ensure scan actions
    void index() const;

    // void text_dump();
    // void text_dump(std::ostream &os);
    // bool text_load(std::string_view s);

    bool is_empty() {
        return schema.empty();
    }

    // Apis
    std::expected<Row *, std::string> find_by_id(const RowId id);
    std::expected<Row *, std::string> find_by_pk(const Value &);
    void scan(std::function<void(const Table::Row &)> cb) const;
    void scan_mut(std::function<void(Row &)> cb);
    enum class ScanAction {
        Keep,
        Delete,
        Stop,
    };
    void scan_struct(std::function<ScanAction(Row &)> cb);
    std::expected<RowId, std::string> insert(std::span<const Value> values);
    std::expected<void, std::string> erase_row(RowId id);

    std::string_view get_file_path() {
        return file_on_disk;
    }

    std::string_view get_name() const {
        return tb_name;
    }

    const auto rows_physical_size() const {
        return rows.size();
    }

    bool is_dirty() const {
        return dirty;
    }

    void flush() {
        logging::trace("Flushing `{}`", file_on_disk);
        if (!dirty) {
            return;
        }
        write_back_binary();
        dirty = false;
    }

    void dump_schema() const;
    void dump_schema(std::ostream &os) const;
    void dump_row(RowId id) const;
    void dump_row(std::ostream &os, RowId id) const;
    void dump_row(std::stringstream &os, RowId id) const;

    std::optional<Field> find_field(std::string_view name) const {
        for (auto &f: schema) {
            if (name == f.name) {
                return f;
            }
        }
        return std::nullopt;
    }

    std::optional<Field> find_field(size_t idx) const {
        if (idx >= schema.size()) {
            return std::nullopt;
        }
        return schema[idx];
    }

    const std::vector<Field> &get_schema() const {
        return schema;
    }

    size_t field_count() const {
        return schema.size();
    }

    size_t primary_key_col() const {
        return primary_field;
    }

    std::optional<size_t> field_index(std::string_view name) const {
        for (size_t i = 0; i < schema.size(); ++i) {
            if (schema[i].name == name) {
                return i;
            }
        }
        return std::nullopt;
    }

    size_t alive_rows() const {
        return alive_count;
    }

    const Value &get_value(const Row &row, size_t col) const {
        return row.content[col];
    }

    std::expected<void, std::string> validate_row(std::span<const Value> values) const;

private:
    std::vector<Row> rows;
    std::vector<size_t> free_slots;
    std::vector<Field> schema;

    uint64_t primary_field;

    RowId head = 0;
    RowId tail = 0;
    uint64_t alive_count = 0;
    RowId next_rowid = 1;

    struct ValueHash {
        size_t operator() (const Table::Value &v) const noexcept {
            return std::visit(
                [](const auto &x) { return std::hash<std::decay_t<decltype(x)>>{}(x); },
                v.inner);
        }
    };

    // Primary key -> rowid
    mutable std::unordered_map<Value, RowId, ValueHash> primary_index;

    // rowid -> real index in rows
    mutable std::unordered_map<RowId, size_t> rowid_index;

    std::string file_on_disk;

    std::string tb_name;

    // enum Filetype { BIN, TXT } ft;

    bool dirty = false;
    void write_back_binary();
    void write_back_binary(std::ofstream &ofs);

    std::expected<void, std::string> parse_from_file();
    std::expected<void, std::string> parse_from_file(std::ifstream &ifs);

    void rebuild_links();
};

using TableView = std::unordered_map<std::string_view, Table *>;

}  // namespace gpamgr
