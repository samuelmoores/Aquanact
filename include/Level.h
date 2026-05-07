#pragma once
#include <vector>
#include <string>
#include "Object3D.h"
#include "Axis.h"

class Level {
public:
	Level();
	std::vector<Object3D*> Objects();
	void Load();
	void PrepareLoad();
	bool StepLoad();   // load one file; returns true when all files are loaded
	float LoadProgress() const;
	void DrawAxis();
	void LoadObject(char filepath[]);
	void SetDrawAxis(bool drawAxis);

private:
	void ScanPath(const std::string& path);
	void LoadSingleFile(const std::string& path);
	void FinishLoad();

	std::vector<Object3D*> objects;
	Axis m_axis;
	bool m_drawAxis;

	struct FileTask { std::string path; };
	std::vector<FileTask> m_fileQueue;
	int  m_queueIndex    = 0;
	int  m_harryEndIdx   = 0;  // objects[m_harryEndIdx-1] is the harry object
	bool m_loadComplete  = false;
};
