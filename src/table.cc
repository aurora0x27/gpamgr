#include "table.h"

#include "log.h"
#include "misc.h"

#include <fstream>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <vector>

namespace gpamgr {
namespace {
constexpr const char MAGIC_BYTES[] = "GPATBL\0";
constexpr auto MAGIC_SIZE = sizeof(MAGIC_BYTES);
constexpr uint32_t VERSION = 1;

namespace fs = std::filesystem;

// Binary format:
// MAGIC_BYTES
// VERSION
// Metadata:
//   - field_count
//   - alive_count
//   - next_rowid
void write_empty_header(std::ofstream &ofs) {
    ofs.write(MAGIC_BYTES, MAGIC_SIZE);
    uint32_t version = VERSION;
    ofs.write(reinterpret_cast<char *>(&version), sizeof(version));
    uint64_t field_count = 0;
    ofs.write(reinterpret_cast<char *>(&field_count), sizeof(field_count));
    uint64_t data = 0;
    // alive_count
    ofs.write(reinterpret_cast<char *>(&data), sizeof(data));
    // next_rowid
    data = 1;
    ofs.write(reinterpret_cast<char *>(&data), sizeof(data));
}

void touch_file(fs::path file_path) {
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path());
    }
    std::ofstream ofs(file_path, std::ios::out);
    if (ofs) {
        write_empty_header(ofs);
    }
}
}  // namespace

std::expected<Table, std::string> Table::create(std::string_view tb_name, std::ifstream &ifs) {
    Table tb(tb_name);
    if (auto res = tb.parse_from_file(); !res.has_value()) {
        return std::unexpected(res.error());
    }
    return tb;
}

std::expected<Table, std::string> Table::create(std::string_view tb_name,
                                                std::string_view path_view) {
    Table tb(tb_name, path_view);
    std::ifstream ifs(path_view.data());
    if (!ifs.good()) {
        logging::warn("Cannot open cache file {}, creating...", path_view);
        touch_file(path_view);
        return tb;
    }
    if (auto res = tb.parse_from_file(); !res.has_value()) {
        return std::unexpected(res.error());
    }
    return tb;
}

std::expected<RowId, std::string> Table::insert(std::span<const Table::Value> values) {
    if (values.size() != schema.size()) {
        logging::error("Column count mismatch");
        return std::unexpected<std::string>("Column count mismatch");
    }

    // check primary_field
    if (!schema.empty() && schema[primary_field].is_primary) {
        if (primary_index.contains(values[primary_field])) {
            return std::unexpected("Primary key violation");
        }
    }

    // find insert pos and insert
    auto id = next_rowid++;
    size_t target_pos;
    Row row{
        .id = id,
        .next = 0,
        .prev = tail,
        .content = std::vector<Value>(values.begin(), values.end()),
        .expired = false,
    };
    if (!free_slots.empty()) {
        // Reuse idle slots
        target_pos = free_slots.back();
        free_slots.pop_back();
        logging::trace("Reuse physics_index `{}`", target_pos);
        rows[target_pos] = std::move(row);
    } else {
        target_pos = rows.size();
        logging::trace("New physics_index `{}`", target_pos);
        rows.push_back(std::move(row));
    }

    // link
    if (tail != 0) {
        rows[rowid_index[tail]].next = id;
    } else {
        head = id;
    }
    tail = id;

    // insert to index
    rowid_index[id] = target_pos;
    if (!schema.empty() && schema[primary_field].is_primary) {
        primary_index[rows[target_pos].content[primary_field]] = id;
    }

    // write flags
    dirty = true;
    ++alive_count;
    return id;
}

void Table::scan(std::function<void(const Table::Row &)> cb) const {
    RowId curr = head;
    while (curr) {
        const Row &r = rows.at(rowid_index.at(curr));
        cb(r);
        curr = r.next;
    }
}

void Table::scan_mut(std::function<void(Row &)> cb) {
    // index();
    RowId curr = head;
    while (curr) {
        Row &r = rows[rowid_index[curr]];
        RowId next = r.next;
        cb(r);
        curr = next;
    }
    dirty = true;
}

void Table::scan_struct(std::function<ScanAction(Row &)> cb) {
    // index();
    RowId curr = head;
    while (curr != 0) {
        auto it = rowid_index.find(curr);
        if (it == rowid_index.end()) {
            break;
        }
        Row &r = rows[it->second];
        RowId next = r.next;
        logging::debug("scan_struct visit row id={}, next={}", r.id, r.next);
        auto action = cb(r);
        if (action == ScanAction::Delete) {
            auto _ = erase_row(curr);
        }
        if (action == ScanAction::Stop) {
            break;
        }
        curr = next;
    }
    dirty = true;
}

std::expected<Table::Row *, std::string> Table::find_by_id(const RowId id) {
    // index();
    auto it = rowid_index.find(id);
    if (it == rowid_index.end()) {
        return std::unexpected<std::string>(std::format("Cannot find row with id {}", id));
    }
    Row &r = rows[it->second];
    return &r;
}

std::expected<Table::Row *, std::string> Table::find_by_pk(const Value &value) {
    // index();
    if (value != schema[primary_field].type) {
        return std::unexpected<std::string>("Type mismatch");
    }
    auto it = primary_index.find(value);
    if (it == primary_index.end()) {
        return std::unexpected<std::string>(std::format("Cannot find row"));
    }
    auto idx = rowid_index.find(it->second);
    if (idx == rowid_index.end()) {
        return std::unexpected<std::string>(std::format("Index ruined"));
    }
    Row &r = rows[idx->second];
    return &r;
}

std::expected<void, std::string> Table::erase_row(RowId id) {
    // index();
    auto it = rowid_index.find(id);
    if (it == rowid_index.end()) {
        return std::unexpected("Row not found");
    }

    auto physics_index = it->second;
    Row &r = rows[physics_index];
    if (r.expired) {
        return {};
    }

    // 1. unlink
    if (r.prev) {
        rows[rowid_index[r.prev]].next = r.next;
    } else {
        head = r.next;
    }

    if (r.next) {
        rows[rowid_index[r.next]].prev = r.prev;
    } else {
        tail = r.prev;
    }

    // 2. index cleanup
    if (!schema.empty() && schema[primary_field].is_primary) {
        primary_index.erase(r.content[primary_field]);
    }
    rowid_index.erase(id);

    // 3. add to free slots
    free_slots.push_back(physics_index);
    logging::trace("Add to free slot: `{}`", physics_index);

    // 4. mark expired
    r.expired = true;
    dirty = true;
    --alive_count;
    return {};
}

std::expected<void, std::string> Table::parse_from_file() {
    std::ifstream ifs(file_on_disk, std::ios::binary);
    if (!ifs) {
        logging::critical("Failed to open file");
    }

    return parse_from_file(ifs);
}

std::expected<void, std::string> Table::parse_from_file(std::ifstream &ifs) {
    // 1. Magic
    char magic[MAGIC_SIZE];
    ifs.read(magic, sizeof(magic));
    if (memcmp(MAGIC_BYTES, magic, MAGIC_SIZE) != 0) {
        return std::unexpected("Invalid table file");
    }

    // 2. Version
    uint32_t version;
    ifs.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (version != VERSION) {
        return std::unexpected("Unsupported version");
    }

    auto set_flags = [&]() {
        dirty = false;
    };

    // 3. Metadata
    uint64_t field_count;
    ifs.read(reinterpret_cast<char *>(&field_count), sizeof(field_count));
    logging::trace("Got `field_count` {}", field_count);
    ifs.read(reinterpret_cast<char *>(&alive_count), sizeof(alive_count));
    logging::trace("Got `alive_count` {}", alive_count);
    ifs.read(reinterpret_cast<char *>(&next_rowid), sizeof(next_rowid));
    logging::trace("Got `next_rowid` {}", next_rowid);
    if (field_count == 0) {
        logging::warn("Loading a table with no schema...");
        set_flags();
        return {};
    }

    // 4. Schema
    schema.clear();
    schema.reserve(field_count);
    for (uint64_t i = 0; i < field_count; ++i) {
        Field f;

        uint32_t name_len;
        ifs.read(reinterpret_cast<char *>(&name_len), sizeof(name_len));

        f.name.resize(name_len);
        ifs.read(f.name.data(), name_len);

        int32_t ty;
        ifs.read(reinterpret_cast<char *>(&ty), sizeof(ty));
        f.type = static_cast<FieldType>(ty);

        uint8_t is_pk;
        ifs.read(reinterpret_cast<char *>(&is_pk), sizeof(is_pk));
        f.is_primary = is_pk != 0;
        if (f.is_primary) {
            primary_field = i;
        }

        schema.push_back(std::move(f));
    }

    // 5. Rows
    rows.clear();
    free_slots.clear();
    rows.reserve(alive_count);
    for (uint64_t i = 0; i < alive_count; ++i) {
        Row r{};

        ifs.read(reinterpret_cast<char *>(&r.id), sizeof(r.id));
        r.prev = r.next = 0;
        r.expired = false;

        r.content.reserve(schema.size());
        for (auto &f: schema) {
            r.content.push_back(Value::from_binary(f.type, ifs));
        }

        rows.push_back(std::move(r));
    }

    set_flags();
    index();
    rebuild_links();

    return {};
}

void Table::write_back_binary(std::ofstream &ofs) {
    if (!ofs.good()) {
        logging::critical("Unknown error, cannot open file `{}`", file_on_disk);
    }
    if (schema.empty()) {
        write_empty_header(ofs);
        logging::debug("Empty schema, writing an empty file");
        return;
    }

    ofs.write(MAGIC_BYTES, MAGIC_SIZE);

    uint32_t version = VERSION;
    ofs.write(reinterpret_cast<char *>(&version), sizeof(version));
    uint64_t field_count = schema.size();
    ofs.write(reinterpret_cast<char *>(&field_count), sizeof(field_count));
    ofs.write(reinterpret_cast<char *>(&alive_count), sizeof(alive_count));
    ofs.write(reinterpret_cast<char *>(&next_rowid), sizeof(next_rowid));

    for (auto &f: schema) {
        uint32_t len = f.name.size();
        ofs.write(reinterpret_cast<char *>(&len), sizeof(len));
        ofs.write(f.name.data(), len);

        int32_t ty = static_cast<int32_t>(f.type);
        ofs.write(reinterpret_cast<char *>(&ty), sizeof(ty));

        uint8_t pk = f.is_primary;
        ofs.write(reinterpret_cast<char *>(&pk), sizeof(pk));
    }

    auto cb = [&](const Row &r) {
        ofs.write(reinterpret_cast<const char *>(&r.id), sizeof(r.id));
        for (size_t i = 0; i < schema.size(); ++i) {
            r.content[i].dump_binary(ofs);
        }
    };
    scan(cb);
}

void Table::write_back_binary() {
    if (file_on_disk.empty()) {
        logging::warn("This is table in memory, you should assign store path.");
        return;
    }
    logging::trace("Began to write back to file`{}`", file_on_disk);
    if (!std::filesystem::exists(file_on_disk)) {
        logging::warn("Cannot open data file`{}`, creating...", file_on_disk);
        touch_file(file_on_disk);
    }
    std::ofstream os(file_on_disk, std::ios::binary);
    write_back_binary(os);
}

void Table::rebuild_links() {
    head = tail = 0;
    RowId prev = 0;

    for (auto &r: rows) {
        if (r.expired)
            continue;

        r.prev = prev;
        r.next = 0;

        if (prev) {
            rows[rowid_index[prev]].next = r.id;
        } else {
            head = r.id;
        }

        prev = r.id;
    }

    tail = prev;
}

void Table::index() const {
    primary_index.clear();
    rowid_index.clear();
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row &r = rows[i];

        if (r.expired) {
            continue;
        }

        rowid_index[r.id] = i;

        if (!schema.empty() && schema[primary_field].is_primary) {
            primary_index[r.content[primary_field]] = r.id;
        }
    }
}

void Table::dump_schema() const {
    std::cout << utils::StyledText::format("Table from file `{}`", file_on_disk).green().bold()
              << '\n'
              << utils::StyledText("Schema:").magenta().bold() << '\n';
    for (auto &field: schema) {
        std::cout << utils::StyledText::format("- {}: ", field.name).cyan().bold()
                  << utils::StyledText(field_ty_as_string(field.type)).bold();
        if (field.is_primary) {
            std::cout << utils::StyledText(" PRIMARY").magenta().italic().bold();
        }
        std::cout << '\n';
    }
}

void Table::dump_schema(std::ostream &os) const {
    for (auto &field: schema) {
        os << utils::StyledText::format("{}:", field.name).cyan().bold()
           << utils::StyledText(field_ty_as_string(field.type)).bold();
        if (field.is_primary) {
            os << utils::StyledText("*").magenta().italic().bold();
        }
        os << '|';
    }
}

void Table::dump_row(RowId id) const {
    std::cout << id << '|';
    if (auto it = rowid_index.find(id); it != rowid_index.end()) {
        auto &row = rows[it->second];
        for (auto &v: row.content) {
            v.display(std::cout);
            std::cout << '|';
        }
        std::cout << '\n';
    } else {
        std::cout << '\n';
        logging::error("Cannot find row with id {}", id);
    }
}

void Table::dump_row(std::ostream &os, RowId id) const {
    os << id << '|';
    if (auto it = rowid_index.find(id); it != rowid_index.end()) {
        auto &row = rows[it->second];
        for (auto &v: row.content) {
            v.display(os);
            os << '|';
        }
        os << '\n';
    } else {
        os << '\n';
        logging::error("Cannot find row with id {}", id);
    }
}

void Table::dump_row(std::stringstream &ss, RowId id) const {
    ss << id << '|';
    if (auto it = rowid_index.find(id); it != rowid_index.end()) {
        auto &row = rows[it->second];
        for (auto &v: row.content) {
            v.display(ss);
            ss << '|';
        }
        ss << '\n';
    } else {
        ss << '\n';
        logging::error("Cannot find row with id {}", id);
    }
}

std::expected<void, std::string> Table::validate_row(std::span<const Value> values) const {
    const auto num_of_fields = field_count();
    if (values.size() != num_of_fields) {
        return std::unexpected(std::format("Field count mismatch: given `{}`, expected `{}`",
                                           values.size(),
                                           num_of_fields));
    }
    for (size_t i = 0; i < num_of_fields; ++i) {
        if (!values[i].is(schema[i].type)) {
            return std::unexpected(
                std::format("Field at index `{}` type mismatch: given `{}`, expected `{}`",
                            i,
                            field_ty_as_string(values[i].type),
                            field_ty_as_string(schema[i].type)));
        }
    }
    return {};
}
}  // namespace gpamgr
