#pragma once
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class UI {
public:
    UI();
    ~UI();

    void SetViewport(int width, int height);
    void Loop();
    void Import();
    void SetImageVisible(bool isVisible);

private:
    bool         m_visible      = false;
    unsigned int m_panelTexture = 0;
};
