#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

class UI {
public:
	UI();
	void Loop();
	void Import();
private:
	static ImGuiIO* imgui_io;
	Object3D* selectedObj;
	bool m_drawingAxis;

};
