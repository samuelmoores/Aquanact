#include <UI.h>
#include <Engine.h>
#include <StbImage.h>
#include <GLHeaders.h>

UI::UI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(Engine::Window->GLFW(), false);
#ifdef __EMSCRIPTEN__
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    StbImage img;
    img.loadFromFile("assets/ui/PanelWindow.tga");

    glGenTextures(1, &m_panelTexture);
    glBindTexture(GL_TEXTURE_2D, m_panelTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.getWidth(), img.getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.getData());
    glBindTexture(GL_TEXTURE_2D, 0);
}

UI::~UI()
{
    glDeleteTextures(1, &m_panelTexture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::SetViewport(int width, int height) {}

void UI::Loop()
{
    if (!m_visible)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    constexpr ImGuiWindowFlags kHUDFlags =
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_NoInputs        |
        ImGuiWindowFlags_NoNav           |
        ImGuiWindowFlags_NoBackground;

    ImVec2 panelSize(160, 80);
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImVec2 center(screen.x * 0.5f - panelSize.x * 0.5f, screen.y * 0.5f - panelSize.y * 0.5f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::SetNextWindowPos(center, ImGuiCond_Always);
    ImGui::Begin("HUD", nullptr, kHUDFlags);

    ImGui::Image((ImTextureID)(intptr_t)m_panelTexture, panelSize);

    const char* label = "[E]";
    float labelW = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorPos(ImVec2((panelSize.x - labelW) * 0.5f, (panelSize.y - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::Text("%s", label);

    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UI::Import() {}

void UI::SetImageVisible(bool isVisible)
{
    m_visible = isVisible;
}
