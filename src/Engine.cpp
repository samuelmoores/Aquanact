#include <Engine.h>
#include <stdexcept>

GLFWwindow* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;
ImGuiIO* Engine::imgui_io = nullptr;


Engine::Engine()
{
    /* Initialize the library */
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    /* Create a windowed mode window and its OpenGL context */
    Window = glfwCreateWindow(1280, 720, "Aquanact Engine", NULL, NULL);

    if (!Window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(Window);

    // 1. Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    Engine::imgui_io = &io;

    // 2. Setup style (optional)
    ImGui::StyleColorsDark();

    // 3. Initialize backends
    ImGui_ImplGlfw_InitForOpenGL(Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");  // or your GLSL version
    
    // Initialize Camera and Renderer
    Camera = new ::Camera(/* constructor arguments if needed */);
    Renderer = new ::Renderer(/* constructor arguments if needed */);
}