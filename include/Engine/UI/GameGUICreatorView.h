#pragma once

class GameGUICreator;
class Camera;

class GameGUICreatorView {
public:
	void Draw(GameGUICreator& creator, const Camera& camera);
};
