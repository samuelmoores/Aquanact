#pragma once
#include <imgui.h>

class MainMenu {
public:
    bool IsVisible()        const { return m_visible; }
    bool WantsPlay()        const { return m_wantsPlay; }
    bool WantsQuit()        const { return m_wantsQuit; }
    bool WantsHideCursor()  const { return m_wantsHideCursor; }
    void ConsumeHideCursor()      { m_wantsHideCursor = false; }

    void Show();
    void Hide();
    void Render();

private:
    enum class State { kMenu, kLoading };
    State m_state    = State::kMenu;
    bool m_visible   = true;
    bool m_wantsPlay        = false;
    bool m_wantsQuit        = false;
    bool m_wantsHideCursor  = false;
};
