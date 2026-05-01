#pragma once

#include <RmlUi/Core.h>
#include <RmlUi_Platform_GLFW.h>
#include <RmlUi_Renderer_GL3.h>

class Object3D;

class UI {
public:
	UI();
	~UI();

	void SetViewport(int width, int height);
	void Loop();
	void Import();

	void SetHealth(float fraction);  // 0.0 – 1.0
	void SetScore(int score);

	Rml::Context* GetContext() { return m_context; }

private:
	Object3D* selectedObj = nullptr;
	bool m_drawingAxis = true;

	SystemInterface_GLFW m_systemInterface;
	RenderInterface_GL3  m_renderInterface;
	Rml::Context*        m_context = nullptr;
};
