#include "Engine/Core/Window.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>

void Window::startUp()
{
    /* Initialize the library */
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_SAMPLES, 4);

    /* Create a windowed mode window and its OpenGL context */
    m_glfwWindow = glfwCreateWindow(1280, 720, "Aquanact Engine", NULL, NULL);

    if (!m_glfwWindow)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(m_glfwWindow);
    // Frame pacing is controlled by Root rather than the display compositor.
    // This keeps Present from blocking at an unexpected refresh interval.
    glfwSwapInterval(0);
}

void Window::shutDown()
{
    if (m_glfwWindow) {
        glfwDestroyWindow(m_glfwWindow);
        m_glfwWindow = nullptr;
    }
    glfwTerminate();
}

void Window::GetFramebufferSize(int& width, int& height) const
{
    width = 0;
    height = 0;
    if (!m_glfwWindow)
    {
        return;
    }

    glfwGetFramebufferSize(m_glfwWindow, &width, &height);
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_glfwWindow);
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_glfwWindow);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

void Window::Focus()
{
	if (m_glfwWindow)
	{
		glfwFocusWindow(m_glfwWindow);
	}
}

double Window::RefreshRate() const
{
	if (!m_glfwWindow)
	{
		return 60.0;
	}

	int monitorCount = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
	int windowX = 0, windowY = 0, windowWidth = 0, windowHeight = 0;
	glfwGetWindowPos(m_glfwWindow, &windowX, &windowY);
	glfwGetWindowSize(m_glfwWindow, &windowWidth, &windowHeight);

	GLFWmonitor* activeMonitor = glfwGetPrimaryMonitor();
	int largestOverlap = -1;
	for (int index = 0; monitors && index < monitorCount; ++index)
	{
		int monitorX = 0, monitorY = 0;
		glfwGetMonitorPos(monitors[index], &monitorX, &monitorY);
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[index]);
		if (!mode) continue;
		const int overlapWidth = std::max(0, std::min(windowX + windowWidth, monitorX + mode->width) - std::max(windowX, monitorX));
		const int overlapHeight = std::max(0, std::min(windowY + windowHeight, monitorY + mode->height) - std::max(windowY, monitorY));
		const int overlap = overlapWidth * overlapHeight;
		if (overlap > largestOverlap) { largestOverlap = overlap; activeMonitor = monitors[index]; }
	}

	const GLFWvidmode* mode = activeMonitor ? glfwGetVideoMode(activeMonitor) : nullptr;
	return mode && mode->refreshRate > 0 ? static_cast<double>(mode->refreshRate) : 60.0;
}

Window::~Window()
{
    shutDown();
}

