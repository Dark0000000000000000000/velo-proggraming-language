#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <functional>
#include <stdexcept>

struct Value;
using ValuePtr = std::shared_ptr<Value>;
using NativeFunc = std::function<ValuePtr(std::vector<ValuePtr>)>;

struct Value {
    enum class Type { Nil, Bool, Number, String, List, Function };
    
    Type type = Type::Nil;
    double number = 0;
    bool boolean = false;
    std::string str;
    std::vector<ValuePtr> list;
    NativeFunc native_func;
    bool is_native = false;

    // AST node index for user-defined functions (stored externally)
    int func_node_idx = -1;
    std::vector<std::string> func_params;

    static ValuePtr make_nil() {
        return std::make_shared<Value>();
    }
    static ValuePtr make_bool(bool b) {
        auto v = std::make_shared<Value>();
        v->type = Type::Bool; v->boolean = b; return v;
    }
    static ValuePtr make_number(double n) {
        auto v = std::make_shared<Value>();
        v->type = Type::Number; v->number = n; return v;
    }
    static ValuePtr make_string(const std::string& s) {
        auto v = std::make_shared<Value>();
        v->type = Type::String; v->str = s; return v;
    }
    static ValuePtr make_list(std::vector<ValuePtr> items = {}) {
        auto v = std::make_shared<Value>();
        v->type = Type::List; v->list = std::move(items); return v;
    }
    static ValuePtr make_native(NativeFunc f) {
        auto v = std::make_shared<Value>();
        v->type = Type::Function; v->is_native = true; v->native_func = f; return v;
    }

    bool is_truthy() const {
        if (type == Type::Nil) return false;
        if (type == Type::Bool) return boolean;
        if (type == Type::Number) return number != 0;
        if (type == Type::String) return !str.empty();
        return true;
    }

    std::string to_string() const {
        switch (type) {
            case Type::Nil: return "nil";
            case Type::Bool: return boolean ? "true" : "false";
            case Type::Number: {
                if (number == (long long)number)
                    return std::to_string((long long)number);
                return std::to_string(number);
            }
            case Type::String: return str;
            case Type::List: {
                std::string s = "[";
                for (size_t i = 0; i < list.size(); i++) {
                    if (i) s += ", ";
                    s += list[i]->to_string();
                }
                return s + "]";
            }
            case Type::Function: return "<function>";
        }
        return "nil";
    }

    bool equals(const Value& o) const {
        if (type != o.type) return false;
        switch (type) {
            case Type::Nil: return true;
            case Type::Bool: return boolean == o.boolean;
            case Type::Number: return number == o.number;
            case Type::String: return str == o.str;
            default: return false;
        }
    }
};
