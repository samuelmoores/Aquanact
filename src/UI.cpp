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

void UI::Import()
{
	std::cout << "Import\n";
}

void UI::Loop()
{
	// Start frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Your UI code here
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Import")) {
				Import();
			}
			if (ImGui::MenuItem("Open")) {
				// Open function
			}
			if (ImGui::MenuItem("Save")) {
				// Save function
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Quit")) {
				glfwSetWindowShouldClose(Engine::Window->GLFW(), true);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo")) {
				// Undo function
			}
			if (ImGui::MenuItem("Redo")) {
				// Redo function
			}
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	// Render
	ImGui::Render();
}
