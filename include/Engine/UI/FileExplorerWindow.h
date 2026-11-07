#pragma once

class FileManager;

class FileExplorerWindow
{
public:
	void Draw(FileManager& fileManager, bool& open);

private:
	bool m_wasOpen = false;
};
