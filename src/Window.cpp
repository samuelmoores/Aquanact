#include "Window.h"
#include <stdexcept>

GLFWwindow* Window::Engine = nullptr;

Window::Window()
{
    /* Initialize the library */
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    /* Create a windowed mode window and its OpenGL context */
    Engine = glfwCreateWindow(1280, 720, "Aquanact Engine", NULL, NULL);
    if (!Engine)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(Engine);
}
