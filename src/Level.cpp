#include "Level.h"
#include "Engine.h"
#include "Audio.h"
#include <filesystem>
#include <algorithm>

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
    pl.position = glm::vec3(0.0f, 350.0f, 0.0f);
    pl.color = glm::vec3(1.0f, 0.9f, 0.8f);
    Engine::Renderer->AddPointLight(pl);

    //Player
    std::string filepathString = "assets/Tom";
    char* filepath = filepathString.data();
    LoadObject(filepath);
    Mesh* mesh = objects[0]->GetMesh();
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

    Audio::PlayMusic("assets/sounds/music/TheMole.mp3");
    Audio::LoadSound("footstep", "assets/sounds/sfx/Footstep_01.wav");

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
            objects.push_back(new Object3D(modelPath.data()));
        }
    }
    else
    {
        objects.push_back(new Object3D(filepath));
    }
}

void Level::SetDrawAxis(bool drawAxis)
{
    m_drawAxis = drawAxis;
}
