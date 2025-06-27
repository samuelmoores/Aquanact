#include "Renderer.h"

void Renderer::Submit(const RenderCommand& command)
{
	commands.push_back(command);
}

void Renderer::Flush(Camera camera)
{
	for (int i = 0; i < commands.size(); i++)
	{
		commands[i].shader->activate();
		commands[i].shader->setUniform("model", commands[i].modelMatrix);
		commands[i].shader->setUniform("view", camera.GetViewMatrix());
		commands[i].shader->setUniform("projection", camera.GetProjectionMatrix());
		commands[i].shader->setUniform("viewPos", camera.GetPosition());
		commands[i].mesh->Bind();

		glDrawElements(GL_TRIANGLES, commands[i].mesh->FacesSize(), GL_UNSIGNED_INT, 0);
		commands[i].mesh->UnBind();
	}

	commands.clear();
}
