#pragma once
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <vector>
#include <cmath>

class MainMenu;

// Pass HUD_CENTER for an axis to auto-center on that axis, or any pixel value to pin it.
static constexpr float HUD_CENTER = std::numeric_limits<float>::quiet_NaN();

struct HUDText {
    std::string text;
    ImVec2      pos;   // pixel offset within panel; HUD_CENTER = auto-center on that axis
    ImVec4      color;
    float       scale; // font scale multiplier, 1.0 = default

    HUDText(std::string t, ImVec2 p = {HUD_CENTER, HUD_CENTER}, ImVec4 c = {1,1,1,1}, float s = 1.f)
        : text(std::move(t)), pos(p), color(c), scale(s) {}
};

struct HUDImage {
    unsigned int textureId;
    ImVec2       size;  // {-1,-1} = fill panel
    ImVec2       pos;

    HUDImage(unsigned int tex, ImVec2 sz = {-1.f,-1.f}, ImVec2 p = {0.f,0.f})
        : textureId(tex), size(sz), pos(p) {}
};

struct HUDPanel {
    std::string           name;
    ImVec2                size;
    ImVec2                anchor; // screen fraction, e.g. {0.5,0.5} = center screen
    ImVec2                pivot;  // panel fraction placed at anchor, e.g. {0.5,0.5} = center panel
    bool                  visible = false;
    std::vector<HUDImage> images;
    std::vector<HUDText>  texts;

    HUDPanel& AddImage(unsigned int tex, ImVec2 sz = {-1.f,-1.f}, ImVec2 pos = {0.f,0.f});
    HUDPanel& AddText(std::string text, ImVec2 pos = {HUD_CENTER, HUD_CENTER}, ImVec4 color = {1,1,1,1}, float scale = 1.f);
    void SetText(int index, std::string text);
    void Show() { visible = true; }
    void Hide() { visible = false; }
};

class UI {
public:
    UI();
    ~UI();

    void         Loop();
    void         SetViewport(int width, int height);
    void         SetImageVisible(bool isVisible); // targets first panel for backwards compatibility

    HUDPanel&    AddPanel(const std::string& name, ImVec2 size,
                          ImVec2 anchor = {0.5f, 0.5f}, ImVec2 pivot = {0.5f, 0.5f});
    HUDPanel*    GetPanel(const std::string& name);
    unsigned int LoadTexture(const char* path, int* outW = nullptr, int* outH = nullptr);

    void SetMainMenu(MainMenu* mm) { m_mainMenu = mm; }

    enum class KeypadStatus { Neutral, Success, Failure };
    void SetKeypadVisible(bool visible) { m_keypadVisible = visible; }
    void SetKeypadData(int selectedIndex, const int digits[4], KeypadStatus status = KeypadStatus::Neutral) {
        m_keypadSelectedIndex = selectedIndex;
        m_keypadStatus = status;
        for (int i = 0; i < 4; i++) m_keypadDigits[i] = digits[i];
    }

private:
    void RenderPanel(HUDPanel& panel);
    void RenderKeypad();

    std::vector<HUDPanel>    m_panels;
    std::vector<unsigned int> m_textures;

    bool m_keypadVisible = false;
    int  m_keypadSelectedIndex = 0;
    int  m_keypadDigits[4] = {0,0,0,0};
    KeypadStatus m_keypadStatus = KeypadStatus::Neutral;

    MainMenu* m_mainMenu = nullptr;
};
