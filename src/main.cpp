#include <iostream>
#include <SFML/Graphics.hpp>
#include <Axis.h>
#include <Line.h>
#include <Mesh.h>
#include <Camera.h>
#include <Renderer.h>
#include <Object3D.h>

std::vector<Vertex3D> cubeVertices = {
	// back face (z = -0.5)
	{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
	{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },
	{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, -1.0f }, {}, {} },

	// front face (z = +0.5)
	{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
	{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
	{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },
	{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f,  0.0f, 1.0f }, {}, {} },

	// left face (x = -0.5)
	{ { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
	{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },
	{ { -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { -1.0f,  0.0f, 0.0f }, {}, {} },

	// right face (x = +0.5)
	{ { 0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
	{ { 0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
	{ { 0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },
	{ { 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }, { 1.0f,  0.0f, 0.0f }, {}, {} },

	// bottom face (y = -0.5)
	{ {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
	{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
	{ { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },
	{ {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, {}, {} },

	// top face (y = +0.5)
	{ {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	{ { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	{ { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
	{ {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f }, { 0.0f,  1.0f, 0.0f }, {}, {} },
};
std::vector<uint32_t> cubeFaces = {
	// back face
	0, 1, 2,
	0, 2, 3,

	// front face
	4, 5, 6,
	4, 6, 7,

	// left face
	8, 9,10,
	8,10,11,

	// right face
	12,13,14,
	12,14,15,

	// bottom face
	16,17,18,
	16,18,19,

	// top face
	20,21,22,
	20,22,23
};

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
	settings.stencilBits = 8;  // Request a 8 bits stencil buffer
	settings.antialiasingLevel = 2;  // Request 2 levels of antialiasing
	settings.majorVersion = 3;
	settings.minorVersion = 3;
	sf::Window window(sf::VideoMode{ 1200, 800 }, "Modern OpenGL", sf::Style::Resize | sf::Style::Close, settings);
	sf::Clock c;
	gladLoadGL();
	Renderer renderer;
	Axis axis(10.0f);
	Line line;
	ShaderProgram lineShader;
	Camera camera(window);
	std::vector<Object3D> sceneObjects;
	Object3D* selectedObject = nullptr;
	sf::Vector2i mouseLast = sf::Mouse::getPosition();
	
	sceneObjects.push_back(Object3D("models/Xzibit.fbx", "models/Xzibit.png"));
	sceneObjects[0].SetScale(glm::vec3(0.01f, 0.01f, 0.01f));

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

				// In main/render loop
				glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

				axis.UpdateProjection(projection);
			}

			if (ev.type == sf::Event::MouseButtonPressed)
			{
				if (ev.mouseButton.button == sf::Mouse::Left)
				{
					for (int i = 0; i < sceneObjects.size(); i++)
					{
						sceneObjects[i].updateMeshAABB();
						glm::vec3 rayDirection = CastRayFromMouse(window, camera.GetViewMatrix(), camera.GetProjectionMatrix());
						line = Line(1000.0f, camera.GetPosition(), rayDirection);

						if (sceneObjects[i].intersectsRayMesh(camera.GetPosition(), rayDirection))
						{
							selectedObject = &sceneObjects[i];
							break;
						}
						else
						{
							selectedObject = nullptr;
						}
					}

				}
			}
		}

		auto now = c.getElapsedTime();
		auto diff = now - last;
		last = now;

		std::cout << 1 / diff.asSeconds() << " FPS " << std::endl;

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
			renderer.Submit(rc);
		}
		renderer.Flush(camera);
		
		window.display();
	}

	return 0;
}


