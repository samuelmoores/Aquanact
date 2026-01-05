#include <Engine.h>
#include <UI.h>

UI::UI()
{
    // 1. Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // 2. Setup style (optional)
    ImGui::StyleColorsDark();

    // 3. Initialize backends
    ImGui_ImplGlfw_InitForOpenGL(Engine::Window->GLFW(), true);
    ImGui_ImplOpenGL3_Init("#version 330");  // or your GLSL version
}