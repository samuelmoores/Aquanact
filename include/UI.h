#pragma once

class UI {
public:
	UI();
	void Loop();
	void Import();
private:
	Object3D* selectedObj;
	bool m_drawingAxis;
};
