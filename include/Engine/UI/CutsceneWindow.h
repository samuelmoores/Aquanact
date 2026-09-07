#pragma once

class ProjectManager;
class SceneManager;

class CutsceneWindow
{
public:
	void Draw(SceneManager& scenes, ProjectManager& projects, bool& open);

private:
	float m_time = 0.0f;
	bool m_playing = false;
	bool m_engineCamera = true;
	int m_selectedTrack = -1;
	int m_selectedCameraPoint = -1;
};
