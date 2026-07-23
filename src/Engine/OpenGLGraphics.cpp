#include "Engine/OpenGLGraphics.h"

#include "Engine/Window.h"
#include "Engine/GLHeaders.h"

#include <stdexcept>

void OpenGLGraphics::startUp(Window& window)
{
	if (m_initialized)
	{
		return;
	}

	m_window = &window;
	// GLAD must be loaded before any OpenGL calls. Earlier startup crashes would
	// show up here if the context was not current yet.
	MakeCurrent();
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		throw std::runtime_error("Failed to initialize GLAD");
	}

	// The scene renderer starts from a known baseline so the 3D pass is consistent.
	ConfigureDefaultState();
	SetVSync(true);
	UpdateViewport();
	m_initialized = true;
}

void OpenGLGraphics::shutDown()
{
	m_initialized = false;
	m_window = nullptr;
}

OpenGLGraphics::~OpenGLGraphics()
{
	shutDown();
}

void OpenGLGraphics::MakeCurrent()
{
	if (!m_window)
	{
		return;
	}

	glfwMakeContextCurrent(m_window->GLFW());
}

void OpenGLGraphics::SwapBuffers()
{
	if (!m_window)
	{
		return;
	}

	glfwSwapBuffers(m_window->GLFW());
}

void OpenGLGraphics::SetVSync(bool enabled)
{
	m_vsyncEnabled = enabled;
	glfwSwapInterval(enabled ? 1 : 0);
}

void OpenGLGraphics::UpdateViewport()
{
	if (!m_window)
	{
		return;
	}

	int width = 0;
	int height = 0;
	m_window->GetFramebufferSize(width, height);
	if (width <= 0 || height <= 0)
	{
		return;
	}

	if (width == m_lastViewportWidth && height == m_lastViewportHeight)
	{
		return;
	}

	m_lastViewportWidth = width;
	m_lastViewportHeight = height;
	glViewport(0, 0, width, height);
}

void OpenGLGraphics::ConfigureDefaultState()
{
	// 3D rendering wants depth testing enabled and multisampling active.
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void OpenGLGraphics::ConfigureGuiState()
{
	// The GUI pass previously failed because it inherited scene state.
	// This state block gives MyGUI a simple overlay environment instead.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(0);
	glBindVertexArray(0);
}


