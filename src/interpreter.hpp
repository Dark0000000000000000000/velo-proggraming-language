#pragma once
#include "parser.hpp"
#include "value.hpp"
#include "velo_api.hpp"
#include <unordered_map>
#include <cmath>
#include <iostream>
#include <functional>
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif

// ─── Environment (Scope) ─────────────────────────────────────────────────────

struct Env {
    std::unordered_map<std::string, ValuePtr> vars;
    std::shared_ptr<Env> parent;

    Env(std::shared_ptr<Env> p = nullptr) : parent(p) {}

    ValuePtr get(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (parent) return parent->get(name);
        throw std::runtime_error("Undefined variable: '" + name + "'");
    }

    void set(const std::string& name, ValuePtr val) {
        // Search up the chain, set where it exists
        auto* e = this;
        while (e) {
            auto it = e->vars.find(name);
            if (it != e->vars.end()) { it->second = val; return; }
            e = e->parent.get();
        }
        // Not found → create in current scope
        vars[name] = val;
    }

    void define(const std::string& name, ValuePtr val) {
        vars[name] = val;
    }
};

// ─── Control flow signals ────────────────────────────────────────────────────

struct ReturnSignal { ValuePtr val; };
struct BreakSignal {};
struct ContinueSignal {};

// ─── Interpreter ─────────────────────────────────────────────────────────────

class Interpreter {
public:
    std::shared_ptr<Env> global;
    std::function<void(const std::string&)> output_fn;
    std::function<std::string()> input_fn;

    // Loaded extension handles (kept alive so DLLs stay loaded)
    std::vector<void*> ext_handles;
    // Extension search path (same dir as exe by default)
    std::string ext_dir = ".";

    Interpreter() {
        global = std::make_shared<Env>();
        output_fn = [](const std::string& s){ std::cout << s << "\n"; };
        input_fn  = [](){ std::string s; std::getline(std::cin, s); return s; };
        register_builtins();
    }

    ~Interpreter() {
        // Unload DLLs
        for (auto h : ext_handles) {
#ifdef _WIN32
            FreeLibrary((HMODULE)h);
#else
            dlclose(h);
#endif
        }
    }

    // ── Load a .dll extension by name ──────────────────────────────────────
    void load_extension(const std::string& name) {
        // Build path:  ext_dir/name.dll  (or .so on Linux)
#ifdef _WIN32
        std::string path = ext_dir + "/" + name + ".dll";
        HMODULE handle = LoadLibraryA(path.c_str());
        if (!handle) {
            // Also try current directory
            handle = LoadLibraryA((name + ".dll").c_str());
        }
        if (!handle)
            throw std::runtime_error("Cannot load extension '" + name +
                "': " + path + " not found");
        auto init_fn = (VeloInitFn)GetProcAddress(handle, "velo_init");
        if (!init_fn) {
            FreeLibrary(handle);
            throw std::runtime_error("Extension '" + name +
                "' has no velo_init() function");
        }
        ext_handles.push_back((void*)handle);
#else
        std::string path = ext_dir + "/lib" + name + ".so";
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle)
            throw std::runtime_error("Cannot load extension '" + name + "': " + dlerror());
        auto init_fn = (VeloInitFn)dlsym(handle, "velo_init");
        if (!init_fn) {
            dlclose(handle);
            throw std::runtime_error("Extension '" + name + "' has no velo_init()");
        }
        ext_handles.push_back(handle);
#endif
        // Build namespace table (a Velo "list" used as namespace object)
        auto ns = Value::make_list();
        // We'll store functions as named slots — use a map piggy-backed via
        // a special Value that wraps a string→function map.
        // Simpler: register as  name.func  in global env via a proxy object.

        // We give the extension a VeloAPI and let it register functions.
        // Functions get stored in global as   name + "." + funcname
        // so  vegame.draw(...)  works via FieldAccess + Call.

        std::string ns_name = name;
        VeloAPI api;

        api.reg = [this, ns_name](const std::string& fn_name, VeloExtFunc fn) {
            // Register as a native Value::Function in a namespace table
            // stored as global variable `ns_name` which is a pseudo-object.
            // We use a flat naming scheme: store each function directly
            // on the namespace Value's list as named entries.
            // Actually: store in global env as "ns.fn" key — then FieldAccess
            // in the interpreter will look up "ns" and then ".fn".
            // Easiest: make the namespace a special map-like Value.
            // We'll store ns as a Value::List where list[0] is a sentinel,
            // and named methods are stored in the Env as ns_name + "\x00" + fn_name.

            auto fn_val = Value::make_native([fn, this](std::vector<ValuePtr> args) -> ValuePtr {
                // Build a fresh api pointer for each call
                VeloAPI call_api = build_api();
                return fn(args, &call_api);
            });
            // Store under mangled name in global env
            global->define(ns_name + "." + fn_name, fn_val);
        };

        // Fill in the rest of the api
        VeloAPI base = build_api();
        api.nil     = base.nil;
        api.num     = base.num;
        api.boolean = base.boolean;
        api.str     = base.str;
        api.list    = base.list;
        api.print   = base.print;
        api.to_num  = base.to_num;
        api.to_str  = base.to_str;
        api.to_bool = base.to_bool;
        api.to_list = base.to_list;
        api.is_nil  = base.is_nil;
        api.is_num  = base.is_num;
        api.is_str  = base.is_str;
        api.is_bool = base.is_bool;
        api.is_list = base.is_list;
        api.is_func = base.is_func;

        init_fn(&api);

        // Create a namespace proxy object in the global env so that
        //   vegame.draw  →  FieldAccess("vegame", "draw")
        // resolves to the mangled global "vegame.draw"
        // We store a special marker value under the ns name.
        auto marker = std::make_shared<Value>();
        marker->type  = Value::Type::String;
        marker->str   = "__ns__" + ns_name; // marker
        global->define(ns_name, marker);
    }

    void exec(NodePtr node) {
        try { eval(node, global); }
        catch (ReturnSignal&) {}
    }

private:
    // ── Build a VeloAPI struct filled with lambdas ──────────────────────────
    VeloAPI build_api() {
        VeloAPI a;
        a.nil     = []() { return Value::make_nil(); };
        a.num     = [](double n) { return Value::make_number(n); };
        a.boolean = [](bool b)   { return Value::make_bool(b); };
        a.str     = [](const std::string& s) { return Value::make_string(s); };
        a.list    = [](std::vector<ValuePtr> v) { return Value::make_list(v); };
        a.print   = [this](const std::string& s) { output_fn(s); };
        a.to_num  = [](ValuePtr v) { return v->number; };
        a.to_str  = [](ValuePtr v) { return v->to_string(); };
        a.to_bool = [](ValuePtr v) { return v->is_truthy(); };
        a.to_list = [](ValuePtr v) -> std::vector<ValuePtr>& { return v->list; };
        a.is_nil  = [](ValuePtr v) { return v->type == Value::Type::Nil; };
        a.is_num  = [](ValuePtr v) { return v->type == Value::Type::Number; };
        a.is_str  = [](ValuePtr v) { return v->type == Value::Type::String; };
        a.is_bool = [](ValuePtr v) { return v->type == Value::Type::Bool; };
        a.is_list = [](ValuePtr v) { return v->type == Value::Type::List; };
        a.is_func = [](ValuePtr v) { return v->type == Value::Type::Function; };
        return a;
    }

    void register_builtins() {
        // print
        global->define("print", Value::make_native([this](std::vector<ValuePtr> args) -> ValuePtr {
            std::string out;
            for (size_t i = 0; i < args.size(); i++) {
                if (i) out += "\t";
                out += args[i]->to_string();
            }
            output_fn(out);
            return Value::make_nil();
        }));

        // input
        global->define("input", Value::make_native([this](std::vector<ValuePtr> args) -> ValuePtr {
            if (!args.empty()) output_fn(args[0]->to_string());
            return Value::make_string(input_fn());
        }));

        // tonum
        global->define("tonum", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.empty()) return Value::make_nil();
            try { return Value::make_number(std::stod(args[0]->str)); }
            catch (...) { return Value::make_nil(); }
        }));

        // tostr
        global->define("tostr", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.empty()) return Value::make_string("");
            return Value::make_string(args[0]->to_string());
        }));

        // len
        global->define("len", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.empty()) return Value::make_number(0);
            auto& v = args[0];
            if (v->type == Value::Type::String) return Value::make_number(v->str.size());
            if (v->type == Value::Type::List) return Value::make_number(v->list.size());
            throw std::runtime_error("len: unsupported type");
        }));

        // push
        global->define("push", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.size() < 2 || args[0]->type != Value::Type::List)
                throw std::runtime_error("push(list, value)");
            args[0]->list.push_back(args[1]);
            return args[0];
        }));

        // pop
        global->define("pop", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.empty() || args[0]->type != Value::Type::List)
                throw std::runtime_error("pop(list)");
            if (args[0]->list.empty()) return Value::make_nil();
            auto v = args[0]->list.back();
            args[0]->list.pop_back();
            return v;
        }));

        // type
        global->define("type", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.empty()) return Value::make_string("nil");
            switch (args[0]->type) {
                case Value::Type::Nil:      return Value::make_string("nil");
                case Value::Type::Bool:     return Value::make_string("bool");
                case Value::Type::Number:   return Value::make_string("number");
                case Value::Type::String:   return Value::make_string("string");
                case Value::Type::List:     return Value::make_string("list");
                case Value::Type::Function: return Value::make_string("function");
            }
            return Value::make_string("nil");
        }));

        // math functions
        auto math_fn1 = [&](const std::string& name, double(*fn)(double)) {
            global->define(name, Value::make_native([fn](std::vector<ValuePtr> args) -> ValuePtr {
                if (args.empty() || args[0]->type != Value::Type::Number)
                    throw std::runtime_error("Expected number");
                return Value::make_number(fn(args[0]->number));
            }));
        };
        math_fn1("sqrt",  std::sqrt);
        math_fn1("floor", std::floor);
        math_fn1("ceil",  std::ceil);
        math_fn1("abs",   std::abs);
        math_fn1("sin",   std::sin);
        math_fn1("cos",   std::cos);

        global->define("max", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.size() < 2) throw std::runtime_error("max(a, b)");
            return Value::make_number(std::max(args[0]->number, args[1]->number));
        }));
        global->define("min", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            if (args.size() < 2) throw std::runtime_error("min(a, b)");
            return Value::make_number(std::min(args[0]->number, args[1]->number));
        }));

        // range
        global->define("range", Value::make_native([](std::vector<ValuePtr> args) -> ValuePtr {
            double start = 0, end_val = 0, step = 1;
            if (args.size() == 1) { end_val = args[0]->number; }
            else if (args.size() >= 2) { start = args[0]->number; end_val = args[1]->number; }
            if (args.size() >= 3) step = args[2]->number;
            auto list = Value::make_list();
            for (double i = start; i < end_val; i += step)
                list->list.push_back(Value::make_number(i));
            return list;
        }));
    }

    // ── Main eval ──
    ValuePtr eval(NodePtr node, std::shared_ptr<Env> env) {
        using K = ASTNode::Kind;

        switch (node->kind) {
            case K::NumberLit: return Value::make_number(node->num_val);
            case K::StringLit: return Value::make_string(node->str_val);
            case K::BoolLit:   return Value::make_bool(node->bool_val);
            case K::NilLit:    return Value::make_nil();

            case K::ListLit: {
                auto list = Value::make_list();
                for (auto& c : node->children)
                    list->list.push_back(eval(c, env));
                return list;
            }

            case K::Ident: return env->get(node->str_val);

            case K::Block: {
                ValuePtr last = Value::make_nil();
                for (auto& c : node->children)
                    last = eval(c, env);
                return last;
            }

            case K::LetDecl: {
                auto val = eval(node->children[0], env);
                env->define(node->str_val, val);
                return val;
            }

            case K::Assign: {
                auto& target = node->children[0];
                auto val = eval(node->children[1], env);
                if (target->kind == K::Ident) {
                    env->set(target->str_val, val);
                } else if (target->kind == K::Index) {
                    auto list_val = eval(target->children[0], env);
                    auto idx_val  = eval(target->children[1], env);
                    if (list_val->type != Value::Type::List)
                        throw std::runtime_error("Line " + std::to_string(target->line) + ": Not a list");
                    int idx = (int)idx_val->number;
                    if (idx < 0 || idx >= (int)list_val->list.size())
                        throw std::runtime_error("Index out of bounds");
                    list_val->list[idx] = val;
                } else {
                    throw std::runtime_error("Invalid assignment target");
                }
                return val;
            }

            case K::FuncDecl: {
                auto fn = std::make_shared<Value>();
                fn->type = Value::Type::Function;
                fn->func_params = node->params;
                fn->func_node_idx = -2; // marks as user func
                // store body as child
                fn->native_func = nullptr;
                fn->is_native = false;
                // We'll store body reference via a closure
                auto body = node->children[0];
                auto captured_env = env;
                fn->native_func = [this, body, node, captured_env](std::vector<ValuePtr> args) -> ValuePtr {
                    auto fn_env = std::make_shared<Env>(captured_env);
                    for (size_t i = 0; i < node->params.size(); i++) {
                        fn_env->define(node->params[i],
                            i < args.size() ? args[i] : Value::make_nil());
                    }
                    try {
                        eval(body, fn_env);
                    } catch (ReturnSignal& r) {
                        return r.val;
                    }
                    return Value::make_nil();
                };
                fn->is_native = true;
                env->define(node->str_val, fn);
                return fn;
            }

            case K::Call: {
                auto callee = eval(node->children[0], env);
                if (callee->type != Value::Type::Function)
                    throw std::runtime_error("Line " + std::to_string(node->line) + ": Not a function");
                std::vector<ValuePtr> args;
                for (size_t i = 1; i < node->children.size(); i++)
                    args.push_back(eval(node->children[i], env));
                return callee->native_func(args);
            }

            case K::Index: {
                auto container = eval(node->children[0], env);
                auto idx_val   = eval(node->children[1], env);
                if (container->type == Value::Type::List) {
                    int idx = (int)idx_val->number;
                    if (idx < 0) idx = (int)container->list.size() + idx;
                    if (idx < 0 || idx >= (int)container->list.size())
                        throw std::runtime_error("Index out of bounds: " + std::to_string(idx));
                    return container->list[idx];
                }
                if (container->type == Value::Type::String) {
                    int idx = (int)idx_val->number;
                    if (idx < 0) idx = (int)container->str.size() + idx;
                    if (idx < 0 || idx >= (int)container->str.size())
                        throw std::runtime_error("String index out of bounds");
                    return Value::make_string(std::string(1, container->str[idx]));
                }
                throw std::runtime_error("Cannot index this type");
            }

            case K::BinOp: return eval_binop(node, env);
            case K::UnaryOp: return eval_unary(node, env);
            case K::Concat: {
                auto l = eval(node->children[0], env);
                auto r = eval(node->children[1], env);
                return Value::make_string(l->to_string() + r->to_string());
            }

            // ns.func  →  look up "ns.func" in global env
            case K::FieldAccess: {
                auto obj = eval(node->children[0], env);
                // If it's a namespace marker, look up "nsname.fieldname" globally
                if (obj->type == Value::Type::String &&
                    obj->str.substr(0, 6) == "__ns__") {
                    std::string ns_name = obj->str.substr(6);
                    std::string key = ns_name + "." + node->str_val;
                    try { return global->get(key); }
                    catch (...) {
                        throw std::runtime_error(
                            "Extension '" + ns_name + "' has no member '" +
                            node->str_val + "'");
                    }
                }
                throw std::runtime_error(
                    "Line " + std::to_string(node->line) +
                    ": '.' only works on imported extensions");
            }

            // import "extname"
            case K::Import: {
                load_extension(node->str_val);
                return Value::make_nil();
            }

            case K::If: {
                // children: cond, body, [elif_cond, elif_body, ...], [else_body]
                size_t i = 0;
                while (i + 1 < node->children.size()) {
                    auto cond = eval(node->children[i], env);
                    if (cond->is_truthy()) {
                        auto scope = std::make_shared<Env>(env);
                        return eval(node->children[i+1], scope);
                    }
                    i += 2;
                }
                // else
                if (i < node->children.size()) {
                    auto scope = std::make_shared<Env>(env);
                    return eval(node->children[i], scope);
                }
                return Value::make_nil();
            }

            case K::For: {
                auto start_v = eval(node->children[0], env);
                auto end_v   = eval(node->children[1], env);
                double step  = 1.0;
                int body_idx = 2;
                if (node->has_step) {
                    step = eval(node->children[2], env)->number;
                    body_idx = 3;
                }
                double i_val = start_v->number;
                double end_val = end_v->number;
                while ((step > 0 ? i_val <= end_val : i_val >= end_val)) {
                    auto scope = std::make_shared<Env>(env);
                    scope->define(node->loop_var, Value::make_number(i_val));
                    try {
                        eval(node->children[body_idx], scope);
                    } catch (BreakSignal&) { break; }
                      catch (ContinueSignal&) {}
                    i_val += step;
                }
                return Value::make_nil();
            }

            case K::While: {
                while (true) {
                    auto cond = eval(node->children[0], env);
                    if (!cond->is_truthy()) break;
                    auto scope = std::make_shared<Env>(env);
                    try {
                        eval(node->children[1], scope);
                    } catch (BreakSignal&) { break; }
                      catch (ContinueSignal&) {}
                }
                return Value::make_nil();
            }

            case K::Return: {
                ValuePtr v = Value::make_nil();
                if (!node->children.empty()) v = eval(node->children[0], env);
                throw ReturnSignal{v};
            }
            case K::Break:    throw BreakSignal{};
            case K::Continue: throw ContinueSignal{};

            default:
                throw std::runtime_error("Unknown AST node");
        }
    }

    ValuePtr eval_binop(NodePtr node, std::shared_ptr<Env> env) {
        const std::string& op = node->op;

        // Short circuit
        if (op == "and") {
            auto l = eval(node->children[0], env);
            if (!l->is_truthy()) return l;
            return eval(node->children[1], env);
        }
        if (op == "or") {
            auto l = eval(node->children[0], env);
            if (l->is_truthy()) return l;
            return eval(node->children[1], env);
        }

        auto l = eval(node->children[0], env);
        auto r = eval(node->children[1], env);

        if (op == "==") return Value::make_bool(l->equals(*r));
        if (op == "!=") return Value::make_bool(!l->equals(*r));

        // Numeric ops
        if (l->type == Value::Type::Number && r->type == Value::Type::Number) {
            double a = l->number, b = r->number;
            if (op == "+") return Value::make_number(a + b);
            if (op == "-") return Value::make_number(a - b);
            if (op == "*") return Value::make_number(a * b);
            if (op == "/") {
                if (b == 0) throw std::runtime_error("Division by zero");
                return Value::make_number(a / b);
            }
            if (op == "%") return Value::make_number(std::fmod(a, b));
            if (op == "^") return Value::make_number(std::pow(a, b));
            if (op == "<") return Value::make_bool(a < b);
            if (op == "<=") return Value::make_bool(a <= b);
            if (op == ">") return Value::make_bool(a > b);
            if (op == ">=") return Value::make_bool(a >= b);
        }

        // String + (concatenation shorthand)
        if (op == "+" && l->type == Value::Type::String && r->type == Value::Type::String)
            return Value::make_string(l->str + r->str);

        // String comparison
        if ((op == "<" || op == ">" || op == "<=" || op == ">=") &&
            l->type == Value::Type::String && r->type == Value::Type::String) {
            if (op == "<")  return Value::make_bool(l->str < r->str);
            if (op == "<=") return Value::make_bool(l->str <= r->str);
            if (op == ">")  return Value::make_bool(l->str > r->str);
            if (op == ">=") return Value::make_bool(l->str >= r->str);
        }

        throw std::runtime_error("Line " + std::to_string(node->line) +
            ": Invalid operation '" + op + "' on " +
            l->to_string() + " and " + r->to_string());
    }

    ValuePtr eval_unary(NodePtr node, std::shared_ptr<Env> env) {
        auto v = eval(node->children[0], env);
        if (node->op == "-") {
            if (v->type != Value::Type::Number)
                throw std::runtime_error("Unary minus on non-number");
            return Value::make_number(-v->number);
        }
        if (node->op == "not") return Value::make_bool(!v->is_truthy());
        throw std::runtime_error("Unknown unary op");
    }
};
