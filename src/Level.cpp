#include "Level.h"
#include "Engine.h"
#include "Audio.h"
#include <filesystem>
#include <algorithm>
#include <iostream>

Level::Level()
{
    m_drawAxis = false;
}

std::vector<Object3D*> Level::Objects()
{
	return objects;
}

void Level::Load()
{
	m_axis = Axis(1000.0f, 100.0f);

    PointLight pl;
    pl.position  = glm::vec3(-400.0f, 550.0f, -300.0f);
    pl.color     = glm::vec3(1.0f, 0.9f, 0.8f);
    pl.constant  = 1.0f;
    pl.linear    = 0.0004f;
    pl.quadratic = 0.000002f;
    Engine::Renderer->AddPointLight(pl);

    PointLight pl2;
    pl2.position = glm::vec3(500.0f, 650.0f, 400.0f);
    pl2.color = glm::vec3(1.0f, 0.9f, 0.8f);
    pl2.constant = 1.0f;
    pl2.linear = 0.0004f;
    pl2.quadratic = 0.000002f;
    Engine::Renderer->AddPointLight(pl2);

    //Player
    std::string filepathString = "assets/Tom";
    char* filepath = filepathString.data();
    LoadObject(filepath);
    Mesh* mesh = objects[0]->GetMesh();
    for (int i = 0; i < mesh->NumBuffers(); i++)
        mesh->SetAmbientColor(i, mesh->GetMaterial(i).ambientColor * 0.2f);
    Engine::Camera->Focus(mesh->minBounds(), mesh->maxBounds());

    // =============================================================

    filepathString = "assets/Office";
    filepath = filepathString.data();
    LoadObject(filepath);
    
    // ================================================================

    //ground
    filepathString = "assets/Floor";
    filepath = filepathString.data();
    LoadObject(filepath);

    Audio::PlayMusic("assets/sounds/music/TheMole.mp3", true, 90.0f);
    Audio::LoadSound("footstep", "assets/sounds/sfx/Footstep_01.wav");
    Audio::LoadSound("keypad_digit",   "assets/sounds/ui/Click_Standard_00.mp3");
    Audio::LoadSound("keypad_nav",     "assets/sounds/ui/Click_Standard_01.mp3");
    Audio::LoadSound("keypad_success", "assets/sounds/ui/Click_Sharp_00.mp3");
    Audio::LoadSound("keypad_fail",    "assets/sounds/ui/UISounds_018.wav");
    Audio::LoadSound("computer_enter", "assets/sounds/ui/Click_Standard_04.mp3");
    Audio::LoadSound("computer_exit",  "assets/sounds/ui/Click_Standard_05.mp3");

    Audio::LoadSound("keypad_enter", "assets/sounds/ui/Click_Electronic_02.mp3");
    Audio::LoadSound("keypad_exit", "assets/sounds/ui/Click_Electronic_03.mp3");
    
    Audio::LoadSound("tv_enter", "assets/sounds/ui/Click_Electronic_00.mp3");
    Audio::LoadSound("tv_exit", "assets/sounds/ui/Click_Electronic_01.mp3");

    Animator* playerAnimator = objects[0]->GetAnimator();
    playerAnimator->AddEvent(1, 5.0f, [] { Audio::PlaySound("footstep", 50.0f); });
    playerAnimator->AddEvent(1, 15.0f, [] { Audio::PlaySound("footstep", 50.0f); });

}

void Level::DrawAxis()
{
    if (m_drawAxis)
    {
	    m_axis.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	    m_axis.draw(Engine::Camera->GetViewMatrix());
    }
}

void Level::LoadObject(char filepath[])
{
    namespace fs = std::filesystem;
    static const std::vector<std::string> modelExts = { ".fbx", ".obj", ".gltf", ".glb", ".dae" };

    if (fs::is_directory(filepath))
    {
        for (const auto& entry : fs::directory_iterator(filepath))
        {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (std::find(modelExts.begin(), modelExts.end(), ext) == modelExts.end()) continue;

            std::string modelPath = entry.path().string();
            Object3D* obj = new Object3D(modelPath.data());
            
            std::string name = obj->Name();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find("wall") == std::string::npos)
            {
                obj->SetIgnoreCameraCollision(true);
            }
            else
            {
                std::cout << "[ignore cam col false: " << (objects.size() - 1) << "] " << obj->Name() << std::endl;

            }

            objects.push_back(obj);
        }
    }
    else
    {
        Object3D* obj = new Object3D(filepath);

        std::string name = obj->Name();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find("wall") == std::string::npos)
        {
            obj->SetIgnoreCameraCollision(true);
        }

        objects.push_back(obj);
        std::cout << "[" << (objects.size() - 1) << "] " << obj->Name() << std::endl;
    }
}

void Level::SetDrawAxis(bool drawAxis)
{
    m_drawAxis = drawAxis;
}
