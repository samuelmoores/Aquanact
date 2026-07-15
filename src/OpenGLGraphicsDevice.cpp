#include "OpenGLGraphicsDevice.h"

#include "OpenGLGraphics.h"
#include "Window.h"
#include "RenderCommand.h"
#include "Camera.h"
#include "Mesh.h"
#include "ShaderProgram.h"
#include "GLHeaders.h"

#include <memory>
#include <vector>

namespace {
	glm::mat4 AiToGlm(const aiMatrix4x4& aiMat)
	{
		glm::mat4 result;
		result[0][0] = aiMat.a1; result[1][0] = aiMat.a2; result[2][0] = aiMat.a3; result[3][0] = aiMat.a4;
		result[0][1] = aiMat.b1; result[1][1] = aiMat.b2; result[2][1] = aiMat.b3; result[3][1] = aiMat.b4;
		result[0][2] = aiMat.c1; result[1][2] = aiMat.c2; result[2][2] = aiMat.c3; result[3][2] = aiMat.c4;
		result[0][3] = aiMat.d1; result[1][3] = aiMat.d2; result[2][3] = aiMat.d3; result[3][3] = aiMat.d4;
		return result;
	}
}

void OpenGLGraphicsDevice::startUp(Window& window)
{
	if (m_initialized) {
		return;
	}

	m_window = &window;
	startUp();
}

void OpenGLGraphicsDevice::startUp()
{
	if (m_initialized || !m_window) {
		return;
	}

	if (!m_platform)
	{
		m_platform = std::make_unique<OpenGLGraphics>();
	}
	m_platform->startUp(*m_window);
	m_initialized = true;
}

void OpenGLGraphicsDevice::shutDown()
{
	if (m_platform)
	{
		m_platform->shutDown();
		m_platform.reset();
	}

	m_window = nullptr;
	m_initialized = false;
}

OpenGLGraphicsDevice::~OpenGLGraphicsDevice()
{
	shutDown();
}

void OpenGLGraphicsDevice::BeginFrame()
{
	if (m_platform)
	{
		m_platform->MakeCurrent();
		m_platform->UpdateViewport();
	}
}

void OpenGLGraphicsDevice::Clear(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLGraphicsDevice::EndFrame()
{
	if (m_platform && m_platform->IsInitialized())
	{
		m_platform->SwapBuffers();
	}
}

void OpenGLGraphicsDevice::Draw(const RenderCommand& command, const Camera& camera)
{
	command.shader->activate();
	command.shader->setUniform("model", command.modelMatrix);
	command.shader->setUniform("view", camera.GetViewMatrix());
	command.shader->setUniform("projection", camera.GetProjectionMatrix());
	command.shader->setUniform("skinned", command.isSkinned);
	command.shader->setUniform("viewPos", camera.GetPosition());

	if (command.isSkinned) {
		const auto& assimpTransforms = command.mesh->GetSkeleton().finalTransformations;
		std::vector<glm::mat4> glmTransforms;
		glmTransforms.reserve(assimpTransforms.size());

		for (const aiMatrix4x4& aiMat : assimpTransforms) {
			glmTransforms.push_back(AiToGlm(aiMat));
		}

		command.shader->setUniform("finalBones", glmTransforms);
	}

	int numBuffs = command.mesh->NumBuffers();
	for (int j = 0; j < numBuffs; j++) {
		const SubMeshMaterial& mat = command.mesh->GetMaterial(j);
		command.shader->setUniform("material", mat.phong);
		command.shader->setUniform("ambientColor", mat.ambientColor);
		command.mesh->Bind(j);
		glDrawElements(GL_TRIANGLES, command.mesh->FacesSize(j), GL_UNSIGNED_INT, reinterpret_cast<void*>(static_cast<uintptr_t>(command.mesh->FacesOffset(j) * sizeof(uint32_t))));
		command.mesh->UnBind();
	}
}
