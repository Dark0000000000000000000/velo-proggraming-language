#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  velo_api.hpp  —  Public API for Velo extensions (.dll / .so)
//
//  Extension authors include ONLY this header.
//  Example extension (e.g. vegame.cpp):
//
//    #include "velo_api.hpp"
//    extern "C" VELO_EXPORT void velo_init(VeloAPI* api) {
//        api->reg("hello", [](VeloArgs args, VeloAPI* a) {
//            a->print("Hello from extension!");
//            return a->nil();
//        });
//    }
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <vector>
#include <functional>
#include <memory>

#ifdef _WIN32
  #define VELO_EXPORT __declspec(dllexport)
#else
  #define VELO_EXPORT __attribute__((visibility("default")))
#endif

// Forward declare Value so api.hpp is self-contained
struct Value;
using ValuePtr      = std::shared_ptr<Value>;
using VeloArgs      = std::vector<ValuePtr>;
using VeloExtFunc   = std::function<ValuePtr(VeloArgs, struct VeloAPI*)>;

// ─── VeloAPI — the object passed to velo_init ────────────────────────────────

struct VeloAPI {
    // ── Register a top-level function in the extension namespace
    //    e.g. api->reg("draw", fn)  →  vegame.draw(...)
    std::function<void(const std::string&, VeloExtFunc)> reg;

    // ── Convenience value constructors ───────────────────────
    std::function<ValuePtr()>                          nil;
    std::function<ValuePtr(double)>                    num;
    std::function<ValuePtr(bool)>                      boolean;
    std::function<ValuePtr(const std::string&)>        str;
    std::function<ValuePtr(std::vector<ValuePtr>)>     list;

    // ── Helpers ───────────────────────────────────────────────
    std::function<void(const std::string&)>            print;   // write to Velo output
    std::function<double(ValuePtr)>                    to_num;
    std::function<std::string(ValuePtr)>               to_str;
    std::function<bool(ValuePtr)>                      to_bool;
    std::function<std::vector<ValuePtr>&(ValuePtr)>    to_list;

    // ── Type checks ───────────────────────────────────────────
    std::function<bool(ValuePtr)> is_nil;
    std::function<bool(ValuePtr)> is_num;
    std::function<bool(ValuePtr)> is_str;
    std::function<bool(ValuePtr)> is_bool;
    std::function<bool(ValuePtr)> is_list;
    std::function<bool(ValuePtr)> is_func;
};

// ─── Signature every extension DLL must export ───────────────────────────────
//
//   extern "C" VELO_EXPORT void velo_init(VeloAPI* api);
//
using VeloInitFn = void(*)(VeloAPI*);
