#include <string>
#include <Engine.h>
#include <UI.h>

UI::UI()
{
    m_systemInterface.SetWindow(Engine::Window->GLFW());
    Rml::SetSystemInterface(&m_systemInterface);
    Rml::SetRenderInterface(&m_renderInterface);

    RmlGL3::Initialize();
    Rml::Initialise();

    int width, height;
    glfwGetFramebufferSize(Engine::Window->GLFW(), &width, &height);
    m_renderInterface.SetViewport(width, height);

    m_context = Rml::CreateContext("main", Rml::Vector2i(width, height));

    Rml::LoadFontFace("assets/fonts/OpenSans-VariableFont_wdth,wght.ttf");

    Rml::ElementDocument* doc = m_context->LoadDocument("assets/ui/hud.rml");
    if (doc)
        doc->Show();
}

UI::~UI()
{
    Rml::RemoveContext("main");
    Rml::Shutdown();
}

void UI::SetViewport(int width, int height)
{
    m_renderInterface.SetViewport(width, height);
    if (m_context)
        m_context->SetDimensions(Rml::Vector2i(width, height));
}

void UI::Loop()
{
    if (!m_context)
        return;

    m_renderInterface.BeginFrame();
    m_context->Update();
    m_context->Render();
    m_renderInterface.EndFrame();
}

void UI::SetHealth(float fraction)
{
    Rml::Element* fill = m_context->GetDocument(0)->GetElementById("health-bar-fill");
    if (!fill) return;
    int pct = static_cast<int>(fraction * 100.0f);
    fill->SetProperty("width", std::to_string(pct) + "%");
}

void UI::SetScore(int score)
{
    Rml::Element* el = m_context->GetDocument(0)->GetElementById("score-value");
    if (!el) return;
    el->SetInnerRML(Rml::ToString(score));
}

void UI::Import()
{
}
