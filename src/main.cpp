#include <iostream>
#include <SFML/Graphics.hpp>
#include <Axis.h>
#include <Line.h>
#include <Mesh.h>
#include <Camera.h>
#include <Renderer.h>
#include <Object3D.h>
#include <Animator.h>



glm::vec3 CastRayFromMouse(sf::Window& window, const glm::mat4& view, const glm::mat4& projection)
{
	// Step 1: Get the mouse position relative to the window
	sf::Vector2i mousePos = sf::Mouse::getPosition(window);
	sf::Vector2u windowSize = window.getSize();

	// Step 2: Convert to Normalized Device Coordinates (NDC)
	float x = (2.0f * mousePos.x) / windowSize.x - 1.0f;
	float y = 1.0f - (2.0f * mousePos.y) / windowSize.y; // Flip Y
	float z = 1.0f; // Assume forward ray (into screen)

	glm::vec3 rayNDC(x, y, z);

	// Step 3: Convert NDC to clip space
	glm::vec4 rayClip(rayNDC.x, rayNDC.y, -1.0f, 1.0f); // -1.0f for forward direction

	// Step 4: Clip space to eye (camera/view) space
	glm::vec4 rayEye = glm::inverse(projection) * rayClip;
	rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f); // Set w = 0 for direction vector

	// Step 5: Eye space to world space
	glm::vec4 rayWorld = glm::inverse(view) * rayEye;
	glm::vec3 rayDirection = glm::normalize(glm::vec3(rayWorld));

	return rayDirection;
}


int main() {
	// Initialize the window and OpenGL.
	sf::ContextSettings settings;
	settings.depthBits = 24; // Request a 24 bits depth buffer
	/*
	settings.stencilBits = 8;  // Request a 8 bits stencil buffer
	settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
	settings.majorVersion = 3;
	settings.minorVersion = 3;
	*/

	//create window AND OpenGL context
	sf::Window window(sf::VideoMode{ 1280, 720 }, "Modern OpenGL", sf::Style::Resize | sf::Style::Close, settings);

	//load OpenGL context
	gladLoadGL();

	Renderer renderer;
	Camera camera(window);
	Axis axis(10.0f);
	axis.UpdateProjection(camera.GetProjectionMatrix());

	std::vector<Object3D> sceneObjects;
	Object3D* selectedObject = nullptr;
	sf::Vector2i mouseLast = sf::Mouse::getPosition();
	
	sceneObjects.push_back(Object3D("models/TestDummy.fbx", "models/TestDummy_Diffuse.png", true));
	sceneObjects.push_back(Object3D(Object3D::cubeVertices, Object3D::cubeFaces));
	sceneObjects[1].Move(glm::vec3(-2, 0, -3));

	int index = 0;
	sceneObjects[0].GetShader()->activate();
	sceneObjects[0].GetShader()->setUniform("bone", index);

	sf::Clock c;
	auto last = c.getElapsedTime();
	glEnable(GL_DEPTH_TEST);
	while (window.isOpen()) {
		sf::Event ev;
		while (window.pollEvent(ev)) {
			if (ev.type == sf::Event::Closed) {
				window.close();
			}

			if (ev.type == sf::Event::Resized)
			{
				float aspectRatio = static_cast<float>(window.getSize().x) / window.getSize().y;
				glViewport(0, 0, window.getSize().x, window.getSize().y);
				axis.UpdateProjection(camera.GetProjectionMatrix());
			}

			if (ev.type == sf::Event::MouseButtonPressed)
			{
				if (ev.mouseButton.button == sf::Mouse::Left)
				{
					for (int i = 0; i < sceneObjects.size(); i++)
					{
						sceneObjects[i].updateMeshAABB();
						glm::vec3 rayDirection = CastRayFromMouse(window, camera.GetViewMatrix(), camera.GetProjectionMatrix());

						if (sceneObjects[i].intersectsRayMesh(camera.GetPosition(), rayDirection))
						{
							//selectedObject = &sceneObjects[i];
							break;
						}
						else
						{
							selectedObject = nullptr;
						}
					}

				}
				else if(ev.mouseButton.button == sf::Mouse::Middle)
				{
					sceneObjects[0].GetShader()->activate();
					std::cout << "bone " << index << " selected\n";
					sceneObjects[0].GetShader()->setUniform("bone", ++index % 52);
				}
			}


		}

		auto now = c.getElapsedTime();
		auto diff = now - last;
		last = now;

		//std::cout << 1 / diff.asSeconds() << " FPS " << std::endl;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		camera.CameraControl(mouseLast, diff);
		axis.draw(camera.GetViewMatrix());

		if (selectedObject != nullptr)
		{
			selectedObject->Rotate(glm::vec3(0, diff.asSeconds() / 4, 0));
		}

		for (int i = 0; i < sceneObjects.size(); i++)
		{
			RenderCommand rc;
			rc.mesh = sceneObjects[i].GetMesh();
			rc.shader = sceneObjects[i].GetShader();
			rc.modelMatrix = sceneObjects[i].BuildModelMatrix();
			rc.isSkinned = sceneObjects[i].skinned();
			renderer.Submit(rc);
		}
		renderer.Flush(camera);
		
		window.display();
	}

	return 0;
}


