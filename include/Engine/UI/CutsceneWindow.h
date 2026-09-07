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
	int m_selectedTrack = -1;
};
