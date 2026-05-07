#include "MainMenu.h"
#include "Engine.h"

void MainMenu::Show()
{
    m_visible   = true;
    m_wantsPlay = false;
    m_wantsQuit = false;
}

void MainMenu::Hide()
{
    m_visible = false;
}

void MainMenu::Render()
{
    if (!m_visible) return;

    // Advance loading by one object per frame
    if (m_state == State::kLoading)
    {
        if (Engine::Level->StepLoad())
        {
            m_wantsPlay = true;
            return;
        }
    }

    constexpr float kW = 300.0f;
    constexpr float kH = 230.0f;

    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        { screen.x * 0.5f, screen.y * 0.5f },
        ImGuiCond_Always,
        { 0.5f, 0.5f }
    );
    ImGui::SetNextWindowSize({ kW, kH }, ImGuiCond_Always);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.12f, 0.92f));
    ImGui::Begin("##MainMenu", nullptr, kFlags);
    ImGui::PopStyleColor();

    // Title
    ImGui::SetWindowFontScale(2.0f);
    const char* kTitle = "AQUANACT";
    float titleW = ImGui::CalcTextSize(kTitle).x;
    ImGui::SetCursorPosX((kW - titleW) * 0.5f);
    ImGui::TextColored({ 0.3f, 0.8f, 1.0f, 1.0f }, "%s", kTitle);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    if (m_state == State::kMenu)
    {
        constexpr float kBtnW = 200.0f;
        constexpr float kBtnH = 52.0f;

        ImGui::SetCursorPosX((kW - kBtnW) * 0.5f);
        if (ImGui::Button("Play Game", { kBtnW, kBtnH }))
        {
            Engine::Level->PrepareLoad();
            m_state = State::kLoading;
            m_wantsHideCursor = true;
        }

        ImGui::Spacing();

        ImGui::SetCursorPosX((kW - kBtnW) * 0.5f);
        if (ImGui::Button("Quit Game", { kBtnW, kBtnH }))
            m_wantsQuit = true;
    }
    else // kLoading
    {
        float progress = Engine::Level->LoadProgress();

        const char* kLoadingLabel = "Loading...";
        float labelW = ImGui::CalcTextSize(kLoadingLabel).x;
        ImGui::SetCursorPosX((kW - labelW) * 0.5f);
        ImGui::TextColored({ 0.7f, 0.7f, 0.7f, 1.0f }, "%s", kLoadingLabel);

        ImGui::Spacing();
        ImGui::Spacing();

        constexpr float kBarPad = 20.0f;
        ImGui::SetCursorPosX(kBarPad);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
        ImGui::ProgressBar(progress, { kW - kBarPad * 2.0f, 18.0f }, "");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(progress * 100.0f));
        float pctW = ImGui::CalcTextSize(pctBuf).x;
        ImGui::SetCursorPosX((kW - pctW) * 0.5f);
        ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 1.0f }, "%s", pctBuf);
    }

    ImGui::End();
}
