#pragma once

#include <memory>
#include <vector>

class Window;
class EngineCamera;
class RenderManager;
class OpenGLGraphicsDevice;
class Debug;
class EngineGUI;
class FileManager;
class Object3D;

extern Window gWindow;
extern EngineCamera gEngineCamera;
extern RenderManager gRenderManager;
extern OpenGLGraphicsDevice gGraphicsDevice;
extern Debug gDebug;
extern EngineGUI gEngineGUI;
extern FileManager gFileManager;
extern std::vector<std::unique_ptr<Object3D>> gSceneObjects;
