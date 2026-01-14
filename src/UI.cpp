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

    selectedObj = nullptr;
    m_drawingAxis = false;
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
        if (ImGui::BeginMenu("File")) 
        {
            if (ImGui::MenuItem("Import"))
                import = true;
    
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Axis", nullptr, &m_drawingAxis))
                Engine::Level->SetDrawAxis(m_drawingAxis);

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (import)
        ImGui::OpenPopup("Import");

    // Set the size before BeginPopupModal
    static char inputBuffer[256] = "";

    if (ImGui::BeginPopupModal("Import")) 
    {
        // Draw popup contents.
        ImGui::Text("Paste file path to model (with or without \"\")");
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

    ImGui::Begin("Objects");
    std::vector<Object3D*> objects = Engine::Level->Objects();
  
    //check if there are objects in the scene
    for (int i = 0; i < objects.size(); i++)
    {
        //for each object, put a button in the box to select it
        // Add a selectable button
        if (ImGui::Button(objects[i]->Name().data()))
        {
            // Set this object as selected when button is clicked
            selectedObj = objects[i];
        }

        if (selectedObj)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "< Selected >");
        }
    }

    ImGui::End();

	// Render
	ImGui::Render();
}
