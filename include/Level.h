#pragma once
#include <vector>
#include "Object3D.h"
#include "Axis.h"

class Level {
public:
	Level();
	std::vector<Object3D*> Objects();
	void Load();
	void DrawAxis();
	void LoadObject(char filepath[]);
	void SetDrawAxis(bool drawAxis);

private:
	std::vector<Object3D*> objects;
	Axis m_axis;
	bool m_drawAxis;
};
