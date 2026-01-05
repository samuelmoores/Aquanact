#include <filesystem>
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

void UI::Loop()
{
	// Start frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	
    bool import = false;

    if (ImGui::BeginMainMenuBar()) 
    {
        if (ImGui::BeginMenu("Menu")) 
        {
            if (ImGui::MenuItem("Import"))
                import = true;
    
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (import)
        ImGui::OpenPopup("Import");

    static char inputBuffer[256] = "";

    // Set the size before BeginPopupModal

    // Maybe some other stuff here.
    if (ImGui::BeginPopupModal("Import")) 
    {
        // Draw popup contents.
        ImGui::Text("Enter text:");
        ImGui::InputText("##input", inputBuffer, IM_ARRAYSIZE(inputBuffer));


        if (ImGui::Button("Import"))
        {
            Engine::Level->LoadObject(inputBuffer);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();


        ImGui::EndPopup();
    }


	// Render
	ImGui::Render();
}
