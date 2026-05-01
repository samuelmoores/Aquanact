#include "Input.h"
#include "Engine.h"
#include <RmlUi_Platform_GLFW.h>

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    RmlGLFW::ProcessKeyCallback(Engine::UI->GetContext(), key, action, mods);
}

void Input::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    RmlGLFW::ProcessCharCallback(Engine::UI->GetContext(), codepoint);
}

void Input::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    RmlGLFW::ProcessCursorPosCallback(Engine::UI->GetContext(), xpos, ypos, 0);
}

void Input::CursorEnterCallback(GLFWwindow* window, int entered)
{
    RmlGLFW::ProcessCursorEnterCallback(Engine::UI->GetContext(), entered);
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Input* self = static_cast<Input*>(glfwGetWindowUserPointer(window));

    bool rmlHandled = !RmlGLFW::ProcessMouseButtonCallback(Engine::UI->GetContext(), button, action, mods);
    if (!rmlHandled)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        self->m_windowActive = true;
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        self->m_mouseLast = glm::vec2(xpos, ypos);
    }
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    int mods = 0;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        mods |= GLFW_MOD_SHIFT;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
        mods |= GLFW_MOD_CONTROL;

    bool rmlHandled = !RmlGLFW::ProcessScrollCallback(Engine::UI->GetContext(), yoffset, mods);
    if (!rmlHandled)
        Engine::Camera->CameraControl(static_cast<float>(yoffset));
}

void Input::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Engine::UI->SetViewport(width, height);
    glViewport(0, 0, width, height);
}

Input::Input()
{
    GLFWwindow* win = Engine::Window->GLFW();
    glfwSetWindowUserPointer(win, this);
    glfwSetKeyCallback(win, KeyCallback);
    glfwSetCharCallback(win, CharCallback);
    glfwSetCursorPosCallback(win, CursorPosCallback);
    glfwSetCursorEnterCallback(win, CursorEnterCallback);
    glfwSetMouseButtonCallback(win, MouseButtonCallback);
    glfwSetScrollCallback(win, ScrollCallback);
    glfwSetFramebufferSizeCallback(win, FramebufferSizeCallback);

    double xpos, ypos;
    glfwGetCursorPos(win, &xpos, &ypos);
    m_mouseLast = glm::vec2(xpos, ypos);

    m_bindings[GLFW_KEY_W]      = Action::MoveForward;
    m_bindings[GLFW_KEY_S]      = Action::MoveBack;
    m_bindings[GLFW_KEY_A]      = Action::MoveLeft;
    m_bindings[GLFW_KEY_D]      = Action::MoveRight;
    m_bindings[GLFW_KEY_ESCAPE] = Action::Escape;
}

void Input::UpdateActionStates()
{
    GLFWwindow* win = Engine::Window->GLFW();
    for (auto& [key, action] : m_bindings)
    {
        bool wasDown = m_actions[action].isDown;
        bool nowDown = glfwGetKey(win, key) == GLFW_PRESS;
        m_actions[action].isDown       = nowDown;
        m_actions[action].justPressed  = !wasDown && nowDown;
        m_actions[action].justReleased = wasDown && !nowDown;
    }
}

bool Input::IsDown(Action a) const
{
    auto it = m_actions.find(a);
    return it != m_actions.end() && it->second.isDown;
}

bool Input::JustPressed(Action a) const
{
    auto it = m_actions.find(a);
    return it != m_actions.end() && it->second.justPressed;
}

bool Input::JustReleased(Action a) const
{
    auto it = m_actions.find(a);
    return it != m_actions.end() && it->second.justReleased;
}

glm::vec2 Input::MoveInput() const
{
    float fwd = (IsDown(Action::MoveForward) ? 1.0f : 0.0f) - (IsDown(Action::MoveBack)  ? 1.0f : 0.0f);
    float rgt = (IsDown(Action::MoveRight)   ? 1.0f : 0.0f) - (IsDown(Action::MoveLeft)  ? 1.0f : 0.0f);
    return glm::vec2(fwd, rgt);
}

void Input::Loop()
{
    UpdateActionStates();

    if (IsDown(Action::Escape))
    {
        glfwSetInputMode(Engine::Window->GLFW(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_windowActive = false;
    }

    if (!m_windowActive)
        return;

    // Mouse look
    double xpos, ypos;
    glfwGetCursorPos(Engine::Window->GLFW(), &xpos, &ypos);
    m_mouseCurr = glm::vec2(xpos, ypos);
    Engine::Camera->CameraControl(m_mouseCurr - m_mouseLast);
    m_mouseLast = m_mouseCurr;
}
