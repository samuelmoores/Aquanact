#include "Window.h"
#include <stdexcept>
#include <iostream>

Window::Window()
{
    /* Initialize the library */
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    /* Create a windowed mode window and its OpenGL context */
    m_glfwWindow = glfwCreateWindow(1280, 720, "Aquanact Engine", NULL, NULL);

    if (!m_glfwWindow)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(m_glfwWindow);
    glfwSwapInterval(0);
}
