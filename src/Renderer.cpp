#include "Engine.h"

void Renderer::Init()
{
	m_shadowMap = std::make_unique<ShadowMap>(2048);
}

void Renderer::Submit(const RenderCommand& command)
{
	commands.push_back(command);
}

void Renderer::AddPointLight(const PointLight& light)
{
	m_pointLights.push_back(light);
}

void Renderer::ClearPointLights()
{
	m_pointLights.clear();
}

glm::mat4 AiToGlm(const aiMatrix4x4& aiMat)
{
	glm::mat4 result;
	result[0][0] = aiMat.a1; result[1][0] = aiMat.a2; result[2][0] = aiMat.a3; result[3][0] = aiMat.a4;
	result[0][1] = aiMat.b1; result[1][1] = aiMat.b2; result[2][1] = aiMat.b3; result[3][1] = aiMat.b4;
	result[0][2] = aiMat.c1; result[1][2] = aiMat.c2; result[2][2] = aiMat.c3; result[3][2] = aiMat.c4;
	result[0][3] = aiMat.d1; result[1][3] = aiMat.d2; result[2][3] = aiMat.d3; result[3][3] = aiMat.d4;
	return result;
}

void printMatrixRender(const aiMatrix4x4& m) {
	// Set precision and fixed notation
	std::cout << std::fixed << std::setprecision(3);

	// Print each row with proper alignment
	// Width of 9 accommodates sign, digits, decimal point, and precision
	std::cout << "[ "
		<< std::setw(9) << m.a1 << " "
		<< std::setw(9) << m.a2 << " "
		<< std::setw(9) << m.a3 << " "
		<< std::setw(9) << m.a4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.b1 << " "
		<< std::setw(9) << m.b2 << " "
		<< std::setw(9) << m.b3 << " "
		<< std::setw(9) << m.b4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.c1 << " "
		<< std::setw(9) << m.c2 << " "
		<< std::setw(9) << m.c3 << " "
		<< std::setw(9) << m.c4 << " ]\n";
	std::cout << "[ "
		<< std::setw(9) << m.d1 << " "
		<< std::setw(9) << m.d2 << " "
		<< std::setw(9) << m.d3 << " "
		<< std::setw(9) << m.d4 << " ]\n";
	std::cout << "----------------------------------------------\n";
}

void Renderer::Flush(Camera* camera)
{
	for (int i = 0; i < commands.size(); i++)
	{
		commands[i].shader->activate();

		commands[i].shader->setUniform("model", commands[i].modelMatrix);
		commands[i].shader->setUniform("view", camera->GetViewMatrix());
		commands[i].shader->setUniform("projection", camera->GetProjectionMatrix());
		commands[i].shader->setUniform("skinned", commands[i].isSkinned);
		commands[i].shader->setUniform("viewPos", camera->GetPosition());

		//apply transforms
		if (commands[i].isSkinned)
		{
			//send transforms to GPU
			const auto& assimpTransforms = commands[i].mesh->GetSkeleton().finalTransformations;
			std::vector<glm::mat4> glmTransforms;
			glmTransforms.reserve(assimpTransforms.size());

			for (const aiMatrix4x4& aiMat : assimpTransforms)
			{
				glmTransforms.push_back(AiToGlm(aiMat));
			}

			commands[i].shader->setUniform("finalBones", glmTransforms);
		}

		if (m_shadowMap && !m_pointLights.empty())
		{
			m_shadowMap->BindTexture(2);
			commands[i].shader->setUniform("shadowMap", (int32_t)2);
			commands[i].shader->setUniform("shadowFarPlane", m_shadowMap->FarPlane());
		}

		commands[i].shader->setUniform("numPointLights", (int32_t)m_pointLights.size());
		for (int k = 0; k < (int)m_pointLights.size(); k++)
		{
			std::string base = "pointLights[" + std::to_string(k) + "].";
			commands[i].shader->setUniform(base + "position", m_pointLights[k].position);
			commands[i].shader->setUniform(base + "color", m_pointLights[k].color);
			commands[i].shader->setUniform(base + "constant", m_pointLights[k].constant);
			commands[i].shader->setUniform(base + "linear", m_pointLights[k].linear);
			commands[i].shader->setUniform(base + "quadratic", m_pointLights[k].quadratic);
		}

		int numBuffs = commands[i].mesh->NumBuffers();
		for (int j = 0; j < numBuffs; j++)
		{
			const SubMeshMaterial& mat = commands[i].mesh->GetMaterial(j);

			commands[i].shader->setUniform("material", mat.phong);
			commands[i].shader->setUniform("ambientColor", mat.ambientColor);

			commands[i].mesh->Bind();
			glDrawElements(GL_TRIANGLES, commands[i].mesh->FacesSize(), GL_UNSIGNED_INT, 0);
			commands[i].mesh->UnBind();
		}
		commands[i].mesh->ClearBufferIndex();

		//commands[i].mesh->DrawBoundingBox();
	}

	commands.clear();
}

void Renderer::Loop()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	Engine::UI->Loop();
	std::vector<Object3D*> objects = Engine::Level->Objects();
	Engine::Level->DrawAxis();

	//only loops through the objects of one default level
	for (int i = 0; i < objects.size(); i++)
	{
		if (objects[i]->GetAnimator())
			objects[i]->GetAnimator()->Update(Engine::DeltaFrameTime());

		RenderCommand rc;
		rc.mesh = objects[i]->GetMesh();
		rc.shader = objects[i]->GetShader();
		rc.modelMatrix = objects[i]->BuildModelMatrix();
		rc.isSkinned = objects[i]->skinned();
		Submit(rc);
	}
	if (m_shadowMap && !m_pointLights.empty())
		m_shadowMap->ShadowPass(commands, m_pointLights[0]);

	Flush(Engine::Camera);
	glfwSwapBuffers(Engine::Window->GLFW());
	glfwPollEvents();
}
