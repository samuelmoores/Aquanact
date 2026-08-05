#pragma once

class GameGUICreator;
class Camera;
class GameGUIAsset;
struct GameGUIWidgetDef;

class GameGUICreatorView {
public:
	void Draw(GameGUICreator& creator, const Camera& camera);
	void DrawWidgetList(GameGUICreator& creator);
	void DrawWidgetDetails(GameGUICreator& creator);
	void DrawProgressBarWidgetDetails(GameGUICreator& creator, GameGUIAsset& asset, GameGUIWidgetDef& widget);
};
