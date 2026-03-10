// ─────────────────────────────────────────────────────────────────────────────
//  vegame.cpp  —  VeGame extension for Velo  (SFML 3.0)
// ─────────────────────────────────────────────────────────────────────────────

#include "../../src/velo_api.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <array>

// ─── Global state ─────────────────────────────────────────────────────────────

struct VGWindow {
    sf::RenderWindow window;
    sf::Color bg_color = sf::Color(30, 30, 60);
    VGWindow(unsigned w, unsigned h, const std::string& title)
        : window(sf::VideoMode({w, h}), title) {
        window.setFramerateLimit(60);
    }
};

struct VGSprite {
    sf::Texture texture;
    sf::Sprite  sprite;
    VGSprite() : sprite(texture) {}
};

struct VGSound {
    sf::SoundBuffer buffer;
    sf::Sound       sound;
    VGSound() : sound(buffer) {}
};

struct VGFont { sf::Font font; };

static std::unordered_map<int, std::unique_ptr<VGWindow>> g_windows;
static std::unordered_map<int, std::unique_ptr<VGSprite>> g_sprites;
static std::unordered_map<int, std::unique_ptr<VGSound>>  g_sounds;
static std::unordered_map<int, std::unique_ptr<VGFont>>   g_fonts;
static int g_next_id = 1;

static VGWindow* get_win(ValuePtr v, VeloAPI* a) {
    int id = (int)a->to_num(v);
    auto it = g_windows.find(id);
    if (it == g_windows.end())
        throw std::runtime_error("vegame: invalid window handle " + std::to_string(id));
    return it->second.get();
}

// ─────────────────────────────────────────────────────────────────────────────

extern "C" VELO_EXPORT void velo_init(VeloAPI* api) {

    // ── Window ────────────────────────────────────────────────────────────────

    api->reg("create_window", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        unsigned w    = args.size() > 0 ? (unsigned)a->to_num(args[0]) : 800;
        unsigned h    = args.size() > 1 ? (unsigned)a->to_num(args[1]) : 600;
        std::string t = args.size() > 2 ? a->to_str(args[2])           : "Velo Game";
        int id = g_next_id++;
        g_windows[id] = std::make_unique<VGWindow>(w, h, t);
        return a->num(id);
    });

    api->reg("is_open", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->boolean(false);
        return a->boolean(get_win(args[0], a)->window.isOpen());
    });

    api->reg("close", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (!args.empty()) get_win(args[0], a)->window.close();
        return a->nil();
    });

    // Process OS events — call every frame!
    api->reg("poll_events", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->nil();
        auto* vgw = get_win(args[0], a);
        while (auto event = vgw->window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                vgw->window.close();
        }
        return a->nil();
    });

    api->reg("set_bg", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 4) throw std::runtime_error("set_bg(win, r, g, b)");
        get_win(args[0], a)->bg_color = sf::Color(
            (uint8_t)a->to_num(args[1]),
            (uint8_t)a->to_num(args[2]),
            (uint8_t)a->to_num(args[3]));
        return a->nil();
    });

    api->reg("clear", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->nil();
        auto* vgw = get_win(args[0], a);
        vgw->window.clear(vgw->bg_color);
        return a->nil();
    });

    api->reg("display", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (!args.empty()) get_win(args[0], a)->window.display();
        return a->nil();
    });

    // ── Drawing ───────────────────────────────────────────────────────────────

    // draw_rect(win, x, y, w, h, r, g, b, alpha=255)
    api->reg("draw_rect", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 8) throw std::runtime_error("draw_rect(win,x,y,w,h,r,g,b)");
        auto* vgw = get_win(args[0], a);
        sf::RectangleShape rect({(float)a->to_num(args[3]), (float)a->to_num(args[4])});
        rect.setPosition({(float)a->to_num(args[1]), (float)a->to_num(args[2])});
        uint8_t alpha = args.size() > 8 ? (uint8_t)a->to_num(args[8]) : 255;
        rect.setFillColor(sf::Color(
            (uint8_t)a->to_num(args[5]),
            (uint8_t)a->to_num(args[6]),
            (uint8_t)a->to_num(args[7]), alpha));
        vgw->window.draw(rect);
        return a->nil();
    });

    // draw_circle(win, x, y, radius, r, g, b, alpha=255)
    api->reg("draw_circle", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 7) throw std::runtime_error("draw_circle(win,x,y,radius,r,g,b)");
        auto* vgw = get_win(args[0], a);
        float radius = (float)a->to_num(args[3]);
        sf::CircleShape circle(radius);
        circle.setPosition({(float)a->to_num(args[1]) - radius,
                            (float)a->to_num(args[2]) - radius});
        uint8_t alpha = args.size() > 7 ? (uint8_t)a->to_num(args[7]) : 255;
        circle.setFillColor(sf::Color(
            (uint8_t)a->to_num(args[4]),
            (uint8_t)a->to_num(args[5]),
            (uint8_t)a->to_num(args[6]), alpha));
        vgw->window.draw(circle);
        return a->nil();
    });

    // draw_line(win, x1, y1, x2, y2, r, g, b)
    api->reg("draw_line", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 8) throw std::runtime_error("draw_line(win,x1,y1,x2,y2,r,g,b)");
        auto* vgw = get_win(args[0], a);
        sf::Color col((uint8_t)a->to_num(args[5]),
                      (uint8_t)a->to_num(args[6]),
                      (uint8_t)a->to_num(args[7]));
        std::array<sf::Vertex, 2> line = {
            sf::Vertex{{(float)a->to_num(args[1]), (float)a->to_num(args[2])}, col},
            sf::Vertex{{(float)a->to_num(args[3]), (float)a->to_num(args[4])}, col}
        };
        vgw->window.draw(line.data(), 2, sf::PrimitiveType::Lines);
        return a->nil();
    });

    // ── Sprites ───────────────────────────────────────────────────────────────

    // load_sprite(path)  →  handle
    api->reg("load_sprite", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) throw std::runtime_error("load_sprite(path)");
        auto spr = std::make_unique<VGSprite>();
        if (!spr->texture.loadFromFile(a->to_str(args[0])))
            throw std::runtime_error("vegame: cannot load image: " + a->to_str(args[0]));
        spr->sprite.setTexture(spr->texture);
        int id = g_next_id++;
        g_sprites[id] = std::move(spr);
        return a->num(id);
    });

    // draw_sprite(win, sprite, x, y, scale_x=1, scale_y=1)
    api->reg("draw_sprite", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 4) throw std::runtime_error("draw_sprite(win,sprite,x,y)");
        auto* vgw = get_win(args[0], a);
        int sid = (int)a->to_num(args[1]);
        auto it = g_sprites.find(sid);
        if (it == g_sprites.end()) throw std::runtime_error("vegame: invalid sprite");
        auto& spr = it->second->sprite;
        spr.setPosition({(float)a->to_num(args[2]), (float)a->to_num(args[3])});
        float sx = args.size() > 4 ? (float)a->to_num(args[4]) : 1.f;
        float sy = args.size() > 5 ? (float)a->to_num(args[5]) : sx;
        spr.setScale({sx, sy});
        vgw->window.draw(spr);
        return a->nil();
    });

    // ── Text ──────────────────────────────────────────────────────────────────

    // load_font(path)  →  handle
    api->reg("load_font", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) throw std::runtime_error("load_font(path)");
        auto fnt = std::make_unique<VGFont>();
        if (!fnt->font.openFromFile(a->to_str(args[0])))
            throw std::runtime_error("vegame: cannot load font: " + a->to_str(args[0]));
        int id = g_next_id++;
        g_fonts[id] = std::move(fnt);
        return a->num(id);
    });

    // draw_text(win, font, text, x, y, size=24, r=255, g=255, b=255)
    api->reg("draw_text", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.size() < 5) throw std::runtime_error("draw_text(win,font,text,x,y)");
        auto* vgw = get_win(args[0], a);
        int fid = (int)a->to_num(args[1]);
        auto fit = g_fonts.find(fid);
        if (fit == g_fonts.end()) throw std::runtime_error("vegame: invalid font");
        sf::Text text(fit->second->font);
        text.setString(a->to_str(args[2]));
        text.setPosition({(float)a->to_num(args[3]), (float)a->to_num(args[4])});
        text.setCharacterSize(args.size() > 5 ? (unsigned)a->to_num(args[5]) : 24);
        uint8_t r  = args.size() > 6 ? (uint8_t)a->to_num(args[6]) : 255;
        uint8_t g2 = args.size() > 7 ? (uint8_t)a->to_num(args[7]) : 255;
        uint8_t b  = args.size() > 8 ? (uint8_t)a->to_num(args[8]) : 255;
        text.setFillColor(sf::Color(r, g2, b));
        vgw->window.draw(text);
        return a->nil();
    });

    // ── Input ─────────────────────────────────────────────────────────────────

    // key_pressed("left"/"right"/"up"/"down"/"space"/"escape"/"w"/"a"/"s"/"d"/...)
    api->reg("key_pressed", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->boolean(false);
        std::string k = a->to_str(args[0]);
        using Key = sf::Keyboard::Key;
        Key key = Key::Unknown;
        if      (k=="left")   key=Key::Left;
        else if (k=="right")  key=Key::Right;
        else if (k=="up")     key=Key::Up;
        else if (k=="down")   key=Key::Down;
        else if (k=="space")  key=Key::Space;
        else if (k=="escape") key=Key::Escape;
        else if (k=="enter")  key=Key::Enter;
        else if (k=="w")      key=Key::W;
        else if (k=="a")      key=Key::A;
        else if (k=="s")      key=Key::S;
        else if (k=="d")      key=Key::D;
        else if (k=="r")      key=Key::R;
        else if (k=="f")      key=Key::F;
        else if (k=="q")      key=Key::Q;
        else if (k=="e")      key=Key::E;
        return a->boolean(sf::Keyboard::isKeyPressed(key));
    });

    // mouse_x(win) / mouse_y(win)  →  number
    api->reg("mouse_x", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->num(0);
        return a->num(sf::Mouse::getPosition(get_win(args[0],a)->window).x);
    });
    api->reg("mouse_y", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->num(0);
        return a->num(sf::Mouse::getPosition(get_win(args[0],a)->window).y);
    });

    // mouse_button("left"/"right"/"middle")  →  bool
    api->reg("mouse_button", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        std::string btn = args.empty() ? "left" : a->to_str(args[0]);
        sf::Mouse::Button b = sf::Mouse::Button::Left;
        if (btn=="right")  b=sf::Mouse::Button::Right;
        if (btn=="middle") b=sf::Mouse::Button::Middle;
        return a->boolean(sf::Mouse::isButtonPressed(b));
    });

    // ── Audio ─────────────────────────────────────────────────────────────────

    // load_sound(path)  →  handle
    api->reg("load_sound", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) throw std::runtime_error("load_sound(path)");
        auto snd = std::make_unique<VGSound>();
        if (!snd->buffer.loadFromFile(a->to_str(args[0])))
            throw std::runtime_error("vegame: cannot load sound: " + a->to_str(args[0]));
        snd->sound.setBuffer(snd->buffer);
        int id = g_next_id++;
        g_sounds[id] = std::move(snd);
        return a->num(id);
    });

    api->reg("play_sound", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->nil();
        auto it = g_sounds.find((int)a->to_num(args[0]));
        if (it != g_sounds.end()) it->second->sound.play();
        return a->nil();
    });

    api->reg("stop_sound", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        if (args.empty()) return a->nil();
        auto it = g_sounds.find((int)a->to_num(args[0]));
        if (it != g_sounds.end()) it->second->sound.stop();
        return a->nil();
    });

    // ── Utils ─────────────────────────────────────────────────────────────────

    api->reg("version", [](VeloArgs, VeloAPI* a) -> ValuePtr {
        return a->str("VeGame 0.2 (SFML 3.0)");
    });

    // sleep(ms)
    api->reg("sleep", [](VeloArgs args, VeloAPI* a) -> ValuePtr {
        int ms = args.empty() ? 0 : (int)a->to_num(args[0]);
        sf::sleep(sf::milliseconds(ms));
        return a->nil();
    });

    // time()  →  seconds since first call (float)
    api->reg("time", [](VeloArgs, VeloAPI* a) -> ValuePtr {
        static sf::Clock clk;
        return a->num(clk.getElapsedTime().asSeconds());
    });
}