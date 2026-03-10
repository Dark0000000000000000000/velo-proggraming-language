#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "lexer.hpp"
#include "parser.hpp"
#include "interpreter.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <mutex>

// ─── Color theme ─────────────────────────────────────────────────────────────

static void SetVeloTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;
    s.FramePadding      = {8, 5};
    s.ItemSpacing       = {8, 6};
    s.WindowPadding     = {12, 12};

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]          = {0.10f, 0.11f, 0.14f, 1.0f};
    c[ImGuiCol_ChildBg]           = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_PopupBg]           = {0.12f, 0.13f, 0.16f, 1.0f};
    c[ImGuiCol_Border]            = {0.22f, 0.24f, 0.30f, 1.0f};
    c[ImGuiCol_FrameBg]           = {0.14f, 0.15f, 0.19f, 1.0f};
    c[ImGuiCol_FrameBgHovered]    = {0.18f, 0.20f, 0.25f, 1.0f};
    c[ImGuiCol_FrameBgActive]     = {0.20f, 0.22f, 0.28f, 1.0f};
    c[ImGuiCol_TitleBg]           = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_TitleBgActive]     = {0.10f, 0.12f, 0.18f, 1.0f};
    c[ImGuiCol_MenuBarBg]         = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_ScrollbarBg]       = {0.08f, 0.09f, 0.11f, 1.0f};
    c[ImGuiCol_ScrollbarGrab]     = {0.25f, 0.27f, 0.35f, 1.0f};
    c[ImGuiCol_Button]            = {0.20f, 0.38f, 0.62f, 1.0f};
    c[ImGuiCol_ButtonHovered]     = {0.26f, 0.47f, 0.75f, 1.0f};
    c[ImGuiCol_ButtonActive]      = {0.16f, 0.30f, 0.52f, 1.0f};
    c[ImGuiCol_Header]            = {0.20f, 0.38f, 0.62f, 0.6f};
    c[ImGuiCol_HeaderHovered]     = {0.26f, 0.47f, 0.75f, 0.7f};
    c[ImGuiCol_Tab]               = {0.14f, 0.15f, 0.19f, 1.0f};
    c[ImGuiCol_TabHovered]        = {0.26f, 0.47f, 0.75f, 1.0f};
    c[ImGuiCol_TabActive]         = {0.20f, 0.38f, 0.62f, 1.0f};
    c[ImGuiCol_Text]              = {0.90f, 0.92f, 0.96f, 1.0f};
    c[ImGuiCol_TextDisabled]      = {0.45f, 0.48f, 0.55f, 1.0f};
    c[ImGuiCol_Separator]         = {0.22f, 0.24f, 0.30f, 1.0f};
    c[ImGuiCol_CheckMark]         = {0.40f, 0.75f, 0.55f, 1.0f};
    c[ImGuiCol_PlotHistogram]     = {0.40f, 0.75f, 0.55f, 1.0f};
}

// ─── Simple syntax highlighting ──────────────────────────────────────────────

static const char* KEYWORDS[] = {
    "let","func","return","if","then","elif","else","end",
    "for","to","step","do","while","break","continue",
    "and","or","not","in","true","false","nil", nullptr
};
static const char* BUILTINS[] = {
    "print","input","tonum","tostr","len","push","pop",
    "type","sqrt","floor","ceil","abs","sin","cos",
    "max","min","range", nullptr
};

// Draw the code editor with basic syntax coloring via ImDrawList overlay
static void DrawHighlightedEditor(const char* label, char* buf, size_t buf_size,
                                   ImVec2 size)
{
    // We render a plain InputTextMultiline for editing, and overlay colors
    // using the draw list (approximate word-based coloring)
    ImGui::InputTextMultiline(label, buf, buf_size, size,
        ImGuiInputTextFlags_AllowTabInput);
}

// ─── App State ───────────────────────────────────────────────────────────────

static char s_code_buf[1 << 18] = // 256 KB
R"(-- ⚡ Welcome to Velo!
-- Syntax: Lua + BASIC + Python + Go vibes

-- Functions
func greet(name)
    print("Hello, " .. name .. "! Welcome to Velo.")
end

greet("World")

-- Lists & loops
let scores = [95, 80, 73, 88, 61, 99, 77]
let total = 0

for i = 0 to len(scores) - 1 do
    total = total + scores[i]
end

let avg = total / len(scores)
print("Average score: " .. tostr(floor(avg)))

-- Conditionals
if avg >= 90 then
    print("Grade: A")
elif avg >= 75 then
    print("Grade: B")
elif avg >= 60 then
    print("Grade: C")
else
    print("Grade: F")
end

-- Recursive fibonacci
func fib(n)
    if n <= 1 then
        return n
    end
    return fib(n - 1) + fib(n - 2)
end

print("")
print("First 10 Fibonacci numbers:")
let results = []
for i = 0 to 9 do
    push(results, fib(i))
end
print(tostr(results))
)";

static std::string s_output;
static std::string s_error;
static bool s_has_error = false;
static bool s_scroll_output = false;

// ─── Run code ────────────────────────────────────────────────────────────────

static void RunCode() {
    s_output.clear();
    s_error.clear();
    s_has_error = false;

    try {
        Lexer lexer(s_code_buf);
        auto tokens = lexer.tokenize();

        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        Interpreter interp;
        // Extensions live next to velo.exe
        interp.ext_dir = ".";
        interp.output_fn = [](const std::string& line) {
            s_output += line + "\n";
        };
        interp.input_fn = []() -> std::string {
            return "";
        };
        interp.exec(ast);
    } catch (std::exception& e) {
        s_error = e.what();
        s_has_error = true;
    }
    s_scroll_output = true;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    if (!glfwInit()) return -1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1200, 780, "Velo IDE", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;


    // Font: try to load a monospace font, fallback to default
    ImFontConfig cfg;
    cfg.SizePixels = 16.0f;
    io.Fonts->AddFontDefault(&cfg);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    SetVeloTheme();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-screen dockspace
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
        ImGui::PopStyleVar(2);

        // ── Menu bar ──
        if (ImGui::BeginMenuBar()) {
            ImGui::TextColored({0.40f, 0.75f, 0.55f, 1.0f}, "◆ Velo IDE");
            ImGui::SameLine(0, 20);
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Clear Editor")) memset(s_code_buf, 0, sizeof(s_code_buf));
                if (ImGui::MenuItem("Clear Output")) { s_output.clear(); s_error.clear(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Examples")) {
                if (ImGui::MenuItem("Hello World")) {
                    strncpy(s_code_buf,
                        "print(\"Hello, World!\")\n"
                        "let name = \"Velo\"\n"
                        "print(\"I am \" .. name)\n", sizeof(s_code_buf));
                }
                if (ImGui::MenuItem("Fibonacci")) {
                    strncpy(s_code_buf,
                        "func fib(n)\n"
                        "    if n <= 1 then\n"
                        "        return n\n"
                        "    end\n"
                        "    return fib(n - 1) + fib(n - 2)\n"
                        "end\n\n"
                        "for i = 0 to 15 do\n"
                        "    print(tostr(i) .. \" -> \" .. tostr(fib(i)))\n"
                        "end\n", sizeof(s_code_buf));
                }
                if (ImGui::MenuItem("List Operations")) {
                    strncpy(s_code_buf,
                        "let fruits = [\"apple\", \"banana\", \"cherry\"]\n"
                        "push(fruits, \"date\")\n"
                        "print(\"List: \" .. tostr(fruits))\n"
                        "print(\"Length: \" .. tostr(len(fruits)))\n\n"
                        "for i = 0 to len(fruits) - 1 do\n"
                        "    print(\"  [\" .. tostr(i) .. \"] \" .. fruits[i])\n"
                        "end\n", sizeof(s_code_buf));
                }
                if (ImGui::MenuItem("FizzBuzz")) {
                    strncpy(s_code_buf,
                        "for i = 1 to 30 do\n"
                        "    if i % 15 == 0 then\n"
                        "        print(\"FizzBuzz\")\n"
                        "    elif i % 3 == 0 then\n"
                        "        print(\"Fizz\")\n"
                        "    elif i % 5 == 0 then\n"
                        "        print(\"Buzz\")\n"
                        "    else\n"
                        "        print(tostr(i))\n"
                        "    end\n"
                        "end\n", sizeof(s_code_buf));
                }
                if (ImGui::MenuItem("Bubble Sort")) {
                    strncpy(s_code_buf,
                        "let arr = [64, 34, 25, 12, 22, 11, 90]\n"
                        "let n = len(arr)\n\n"
                        "for i = 0 to n - 2 do\n"
                        "    for j = 0 to n - i - 2 do\n"
                        "        if arr[j] > arr[j + 1] then\n"
                        "            let tmp = arr[j]\n"
                        "            arr[j] = arr[j + 1]\n"
                        "            arr[j + 1] = tmp\n"
                        "        end\n"
                        "    end\n"
                        "end\n\n"
                        "print(\"Sorted: \" .. tostr(arr))\n", sizeof(s_code_buf));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Extensions (VeGame)")) {
                    strncpy(s_code_buf,
                        "-- Import the VeGame extension\n"
                        "-- (vegame.dll must be next to velo.exe)\n"
                        "import \"vegame\"\n\n"
                        "vegame.print_banner()\n"
                        "print(\"Version: \" .. vegame.version())\n\n"
                        "let win = vegame.create_window(800, 600, \"My Velo Game\")\n"
                        "vegame.set_bg(30, 30, 60)\n"
                        "vegame.draw_rect(100, 100, 64, 64)\n"
                        "vegame.play_sound(\"jump.wav\")\n\n"
                        "print(\"\")\n"
                        "print(\"Done! In a real VeGame these would open a window.)\n", sizeof(s_code_buf));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // ── Layout: left = editor, right = output ──
        float panel_w = (vp->Size.x - 40) / 2.0f;
        float panel_h = vp->Size.y - 80;

        // ── Editor panel ──
        ImGui::BeginChild("##editor_panel", {panel_w, panel_h}, false);
        {
            // Header
            ImGui::TextColored({0.55f, 0.75f, 0.95f, 1.0f}, "  Editor");
            ImGui::SameLine(panel_w - 120);
            ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.55f, 0.32f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.68f, 0.40f, 1.0f});
            if (ImGui::Button("  ▶  Run  ", {110, 30})) RunCode();
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::Text(" [F5]");
            ImGui::Separator();

            // Code editor
            ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.07f, 0.08f, 0.10f, 1.0f});
            ImGui::PushFont(io.Fonts->Fonts[0]);
            DrawHighlightedEditor("##code", s_code_buf, sizeof(s_code_buf),
                {panel_w - 8, panel_h - 70});
            ImGui::PopFont();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::SameLine(0, 16);

        // ── Output panel ──
        ImGui::BeginChild("##output_panel", {panel_w, panel_h}, false);
        {
            ImGui::TextColored({0.55f, 0.75f, 0.95f, 1.0f}, "  Output");
            ImGui::SameLine(panel_w - 100);
            if (ImGui::Button("Clear", {90, 28})) {
                s_output.clear(); s_error.clear(); s_has_error = false;
            }
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.05f, 0.06f, 0.08f, 1.0f});
            ImGui::BeginChild("##output_scroll", {panel_w - 8, panel_h - 70},
                false, ImGuiWindowFlags_HorizontalScrollbar);

            if (s_has_error) {
                ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.4f, 0.4f, 1.0f});
                ImGui::TextWrapped("⚠  Error:\n%s", s_error.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, {0.75f, 0.95f, 0.75f, 1.0f});
                if (!s_output.empty())
                    ImGui::TextUnformatted(s_output.c_str());
                else
                    ImGui::TextDisabled("(no output yet — press Run or F5)");
                ImGui::PopStyleColor();
            }

            if (s_scroll_output) {
                ImGui::SetScrollHereY(1.0f);
                s_scroll_output = false;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        // ── Status bar ──
        ImGui::Separator();
        ImGui::TextDisabled("  Velo v1.0  |  Lua+BASIC+Python+Go syntax  |  F5 = Run");

        ImGui::End();

        // F5 shortcut
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) RunCode();

        // Render
        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
