#pragma once

#include "misc.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>

namespace utils {

class Option;

bool parse_commandline_options(const int argc, const char **const argv);

void print_help(std::string_view prog_name, std::string_view overview);

void dump_args();

void dump_args(std::ostream &os);

class OptionRegistry {
    friend class Option;
    friend bool parse_commandline_options(const int, const char **const);
    friend void print_help(std::string_view, std::string_view);
    friend void dump_args();
    friend void dump_args(std::ostream &os);
    std::unordered_map<std::string_view, Option *> options;
};

OptionRegistry &arg_registry();

const std::vector<std::string_view> &position_args();

const bool help_triggered();

class Option {
    Option() = delete;

public:
    virtual ~Option() = default;

    const std::string &get_name() const {
        return name;
    }

    const std::string &get_help() const {
        return help;
    }

    virtual bool parse(std::string_view arg) = 0;

    virtual bool is_flag() const = 0;
    virtual void print() const = 0;

    virtual void print(std::ostream &os) const = 0;

protected:
    Option(std::string name, std::string help) : name(std::move(name)), help(std::move(help)) {
        arg_registry().options.emplace(this->name, this);
    }

    std::string name;
    std::string help;
};

template <typename DataType>
class opt final : public Option {
    DataType value;

    bool parse_impl(std::string_view arg);

public:
    opt(std::string name, std::string help = "", DataType default_val = {}) :
        Option(std::move(name), std::move(help)), value(default_val) {}

    operator const DataType &() const {
        return value;
    }

    const DataType &operator* () const {
        return value;
    }

    const DataType get() {
        return value;
    }

    bool is_flag() const override {
        return false;
    }

    bool parse(std::string_view arg) override {
        return parse_impl(arg);
    }

    void print() const override {
        std::cout << StyledText::format("-{}:", name).cyan().bold()
                  << StyledText::format(" {}", value).green() << '\n';
    }

    void print(std::ostream &os) const override {
        os << StyledText::format("-{}:", name).cyan().bold()
           << StyledText::format(" {}", value).green() << '\n';
    }
};

template <>
class opt<bool> final : public Option {
    bool value;

public:
    opt(std::string name, std::string help = "", bool default_val = false) :
        Option(std::move(name), std::move(help)), value(default_val) {}

    bool parse(std::string_view arg) override {
        if (arg.empty()) {
            value = true;
            return true;
        }
        if (arg == "true" || arg == "TRUE" || arg == "True" || arg == "1") {
            value = true;
            return true;
        } else if (arg == "false" || arg == "FALSE" || arg == "false" || arg == "0") {
            value = false;
            return true;
        }
        std::cerr << "Option `-" << name << "` expect bool\n";
        return false;
    }

    bool is_flag() const override {
        return true;
    }

    bool get() const {
        return value;
    }

    operator bool() const {
        return value;
    }

    void print() const override {
        std::cout << StyledText::format("-{}", name).cyan().bold() << StyledText(" (flag)").green()
                  << '\n';
    }

    void print(std::ostream &os) const override {
        os << StyledText::format("-{}", name).cyan().bold() << StyledText(" (flag)").green()
           << '\n';
    }
};

}  // namespace utils
