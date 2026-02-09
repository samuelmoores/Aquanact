#include "Engine.h"

void Renderer::Submit(const RenderCommand& command)
{
	commands.push_back(command);
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

		//check if submeshes?
		//	then get curr texture and facesize
		int numBuffs = commands[i].mesh->NumBuffers();
		for (int j = 0; j < numBuffs; j++)
		{
			commands[i].mesh->Bind();
			glDrawElements(GL_TRIANGLES, commands[i].mesh->FacesSize(), GL_UNSIGNED_INT, 0);
			commands[i].mesh->UnBind();
		}
		commands[i].mesh->ClearBufferIndex();

		//commands[i].mesh->DrawBoundingBox();
	}

	commands.clear();
}


float blendFactor = 0.0f;

void Renderer::Loop()
{
	std::vector<Object3D*> objects = Engine::Level->Objects();
	Engine::Level->DrawAxis();

	//only loops through the objects of one default level
	for (int i = 0; i < objects.size(); i++)
	{
		if (objects[i]->skinned())
		{
			//std::cout << objects[i]->BlendFactor() << std::endl;
			if (objects[i]->BlendFactor() < 1.0f)
			{
				int nextAnim = objects[i]->GetMesh()->GetNextAnim();
				objects[i]->IncBlendFactor(Engine::DeltaFrameTime() * 3.0f);
				objects[i]->GetMesh()->BlendAnimation(nextAnim, Engine::TimeElapsed(), objects[i]->BlendFactor());

				if (objects[i]->BlendFactor() >= 1.0f)
				{
					objects[i]->GetMesh()->SetCurrentAnim(nextAnim);
				}
			}
			else
			{
				objects[i]->GetMesh()->RunAnimation(Engine::TimeElapsed());
			}
		}

		RenderCommand rc;
		rc.mesh = objects[i]->GetMesh();
		rc.shader = objects[i]->GetShader();
		rc.modelMatrix = objects[i]->BuildModelMatrix();
		rc.isSkinned = objects[i]->skinned();
		Submit(rc);
	}
	Flush(Engine::Camera);
}
