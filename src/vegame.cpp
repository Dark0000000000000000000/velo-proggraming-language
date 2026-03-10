// ─────────────────────────────────────────────────────────────────────────────
//  vegame.cpp  —  VeGame extension for Velo
//
//  Build (MSYS2 MinGW64):
//    g++ -shared -fPIC -O2 -std=c++17 -o vegame.dll vegame.cpp
//
//  Usage in Velo:
//    import "vegame"
//    vegame.print_banner()
//    let w = vegame.create_window(800, 600, "My Game")
// ─────────────────────────────────────────────────────────────────────────────

#include "../src/velo_api.hpp"
#include <string>
#include <cstdio>

// In a real VeGame you'd use SFML/SDL here.
// This is a demo extension that shows the plugin system works.

extern "C" VELO_EXPORT void velo_init(VeloAPI* api) {

    // vegame.version()  →  "VeGame 0.1"
    api->reg("version", [](VeloArgs, VeloAPI* a) -> ValuePtr {
        return a->str("VeGame 0.1 - Velo Game Library");
    });

    // vegame.print_banner()
    api->reg("print_banner", [](VeloArgs, VeloAPI* a) -> ValuePtr {
        a->print("╔══════════════════════════╗");
        a->print("║  ⚡ VeGame  v0.1         ║");
        a->print("║  Velo Game Extension     ║");
        a->print("╚══════════════════════════╝");
        return a->nil();
    });

    // vegame.add(a, b)  →  number  (simple math demo)
    api->reg("add", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 2 || !a->is_num(args[0]) || !a->is_num(args[1]))
            throw std::runtime_error("vegame.add(num, num)");
        return a->num(a->to_num(args[0]) + a->to_num(args[1]));
    });

    // vegame.create_window(w, h, title)  →  1  (stub, returns handle id)
    api->reg("create_window", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        int w = args.size() > 0 ? (int)a->to_num(args[0]) : 800;
        int h = args.size() > 1 ? (int)a->to_num(args[1]) : 600;
        std::string title = args.size() > 2 ? a->to_str(args[2]) : "Velo Game";
        a->print("[VeGame] Window created: " + std::to_string(w) +
                 "x" + std::to_string(h) + " \"" + title + "\"");
        return a->num(1); // window handle = 1 (stub)
    });

    // vegame.set_bg(r, g, b)
    api->reg("set_bg", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        int r = args.size() > 0 ? (int)a->to_num(args[0]) : 0;
        int g = args.size() > 1 ? (int)a->to_num(args[1]) : 0;
        int b = args.size() > 2 ? (int)a->to_num(args[2]) : 0;
        a->print("[VeGame] Background set to rgb(" +
                 std::to_string(r) + "," +
                 std::to_string(g) + "," +
                 std::to_string(b) + ")");
        return a->nil();
    });

    // vegame.draw_rect(x, y, w, h)
    api->reg("draw_rect", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 4)
            throw std::runtime_error("vegame.draw_rect(x, y, w, h)");
        a->print("[VeGame] Rect drawn at (" +
                 std::to_string((int)a->to_num(args[0])) + "," +
                 std::to_string((int)a->to_num(args[1])) + ") size " +
                 std::to_string((int)a->to_num(args[2])) + "x" +
                 std::to_string((int)a->to_num(args[3])));
        return a->nil();
    });

    // vegame.play_sound(name)
    api->reg("play_sound", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        std::string name = args.size() > 0 ? a->to_str(args[0]) : "?";
        a->print("[VeGame] Playing sound: " + name);
        return a->nil();
    });
}
