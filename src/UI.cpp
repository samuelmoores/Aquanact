#include <UI.h>
#include <Engine.h>
#include <StbImage.h>
#include <GLHeaders.h>
#include "MainMenu.h"

HUDPanel& HUDPanel::AddImage(unsigned int tex, ImVec2 sz, ImVec2 pos)
{
    if (sz.x < 0.f) sz = size;
    images.push_back({tex, sz, pos});
    return *this;
}

HUDPanel& HUDPanel::AddText(std::string text, ImVec2 pos, ImVec4 color, float scale)
{
    texts.push_back({std::move(text), pos, color, scale});
    return *this;
}

void HUDPanel::SetText(int index, std::string text)
{
    if (index >= 0 && index < (int)texts.size())
        texts[index].text = std::move(text);
}

UI::UI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(Engine::Window->GLFW(), false);
    ImGui_ImplOpenGL3_Init("#version 330");

    int w, h;
    unsigned int panelTex = LoadTexture("assets/ui/PanelWindow.tga", &w, &h);
    AddPanel("HUD", {(float)w * 1.25f, (float)h * 1.25f})
        .AddImage(panelTex)
        .AddText("AI ROUTING ACTIVE.\nPROVE HUMAN ADEQUACY.\nSIFT THE SIGNAL.\nFIND THE CODE.", { HUD_CENTER, 50.f }, { 1,1,1,1 }, 1.5f);
}

UI::~UI()
{
    for (unsigned int tex : m_textures)
        glDeleteTextures(1, &tex);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::SetViewport(int width, int height) {}

unsigned int UI::LoadTexture(const char* path, int* outW, int* outH)
{
    StbImage img;
    img.loadFromFile(path);

    if (outW) *outW = img.getWidth();
    if (outH) *outH = img.getHeight();

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 img.getWidth(), img.getHeight(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, img.getData());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textures.push_back(tex);
    return tex;
}

HUDPanel& UI::AddPanel(const std::string& name, ImVec2 size, ImVec2 anchor, ImVec2 pivot)
{
    m_panels.push_back({name, size, anchor, pivot});
    return m_panels.back();
}

HUDPanel* UI::GetPanel(const std::string& name)
{
    for (auto& panel : m_panels)
        if (panel.name == name)
            return &panel;
    return nullptr;
}

void UI::Loop()
{
    bool anyVisible = false;
    if (m_mainMenu && m_mainMenu->IsVisible()) anyVisible = true;
    if (m_keypadVisible) anyVisible = true;
    for (auto& panel : m_panels)
        if (panel.visible) { anyVisible = true; break; }
    if (!anyVisible) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (m_mainMenu && m_mainMenu->IsVisible())
        m_mainMenu->Render();

    for (auto& panel : m_panels)
        if (panel.visible) RenderPanel(panel);

    if (m_keypadVisible)
        RenderKeypad();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UI::RenderKeypad()
{
    ImGui::SetNextWindowPos({ ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::Begin("KEYPAD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));
    for (int i = 0; i < 4; i++)
    {
        if (i > 0) ImGui::SameLine();
        
        bool isSelected = (m_keypadSelectedIndex == i);
        if (isSelected) 
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.8f, 0.8f, 0.1f, 1.0f)); // Yellow
        else
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));

        char buf[2] = { (char)('0' + m_keypadDigits[i]), '\0' };
        ImGui::SetWindowFontScale(3.0f);
        ImGui::PushItemWidth(50.0f);
        ImGui::InputText(("##digit" + std::to_string(i)).c_str(), buf, 2, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    bool executeSelected = (m_keypadSelectedIndex == 4);
    if (executeSelected)
    {
        if (m_keypadStatus == KeypadStatus::Success)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.8f, 0.1f, 1.0f)); // Green
        else if (m_keypadStatus == KeypadStatus::Failure)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f)); // Red
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.1f, 1.0f)); // Yellow
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    }

    ImGui::SetWindowFontScale(1.5f);
    ImGui::Button("EXECUTE", ImVec2(120, 50));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    
    ImGui::PopStyleVar();
    ImGui::End();
}

void UI::RenderPanel(HUDPanel& panel)
{
    constexpr ImGuiWindowFlags kHUDFlags =
        ImGuiWindowFlags_NoDecoration     |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove           |
        ImGuiWindowFlags_NoInputs         |
        ImGuiWindowFlags_NoNav            |
        ImGuiWindowFlags_NoBackground;

    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImVec2 pos(
        screen.x * panel.anchor.x - panel.size.x * panel.pivot.x,
        screen.y * panel.anchor.y - panel.size.y * panel.pivot.y
    );

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::Begin(panel.name.c_str(), nullptr, kHUDFlags);

    for (auto& img : panel.images) {
        ImGui::SetCursorPos(img.pos);
        ImGui::Image((ImTextureID)(intptr_t)img.textureId, img.size);
    }

    for (auto& txt : panel.texts) {
        ImGui::SetWindowFontScale(txt.scale);
        float w  = ImGui::CalcTextSize(txt.text.c_str()).x;
        float h  = ImGui::GetTextLineHeight();
        float cx = std::isnan(txt.pos.x) ? (panel.size.x - w) * 0.5f : txt.pos.x;
        float cy = std::isnan(txt.pos.y) ? (panel.size.y - h) * 0.5f : txt.pos.y;
        ImGui::SetCursorPos({cx, cy});
        ImGui::TextColored(txt.color, "%s", txt.text.c_str());
        ImGui::SetWindowFontScale(1.f);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void UI::SetImageVisible(bool isVisible)
{
    if (!m_panels.empty())
        m_panels[0].visible = isVisible;
}
