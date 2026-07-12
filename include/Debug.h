#pragma once

class Camera;

class Debug {
public:
	Debug() = default;
	void startUp();
	void shutDown();
	void draw(const Camera& camera);

private:
	class Axis* m_axis = nullptr;
};
