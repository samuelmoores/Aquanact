#include <Engine.h>
#include <stdexcept>

GLFWwindow* Engine::Window = nullptr;
Renderer* Engine::Renderer = nullptr;
Camera* Engine::Camera = nullptr;

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

    std::cout << "Window Created\n";

    /* Make the window's context current */
    glfwMakeContextCurrent(Window);
    
    // Initialize Camera and Renderer
    Camera = new ::Camera(/* constructor arguments if needed */);
    Renderer = new ::Renderer(/* constructor arguments if needed */);
}