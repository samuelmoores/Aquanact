#include "PlayerController.h"
#include "Physics.h"
#include "MathUtils.h"
#include "Engine.h"
#include "Audio.h"
#include "UI.h"
#include "Line.h"
#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/constants.hpp>

PlayerController::PlayerController(std::vector<Object3D*> objects)
	: m_objects(std::move(objects))
{
	m_objects[0]->Move(glm::vec3(0));
	m_currRot = m_nextRot = -glm::half_pi<float>();
	m_objects[0]->SetRotation({ 0.0f, m_currRot, 0.0f });
}

void PlayerController::Update()
{
	glm::vec2 inputAxes = Engine::Input->MoveInput();
	glm::vec3 forward   = Engine::Camera->Forward();
	glm::vec3 right     = Engine::Camera->Right();
	glm::vec3 moveDir   = forward * inputAxes.x + right * inputAxes.y;
	bool isMoving = glm::length(moveDir) > 0.01f;

	// Animation transitions
	if (!m_wasMoving && isMoving)
		m_objects[0]->GetAnimator()->Play(1);
	if (m_wasMoving && !isMoving)
		m_objects[0]->GetAnimator()->Play(0);
	m_wasMoving = isMoving;

	// Movement + collision
	const float moveSpeed = 500.0f;
	glm::vec3 movement(0.0f);
	if (isMoving)
		movement = glm::normalize(moveDir) * moveSpeed * Engine::DeltaFrameTime();

	

	if (isMoving)
	{
		m_objects[0]->Move(movement);

		for (int iter = 0; iter < 8; iter++)
		{
			bool collidedInThisIter = false;
			for (int i = 1; i < (int)m_objects.size() - 1; i++)
			{
				Mesh* playerMesh = m_objects[0]->GetMesh();
				glm::vec3 pMin = playerMesh->minBounds();
				glm::vec3 pMax = playerMesh->maxBounds();
				float radius = std::max((pMax.x - pMin.x), (pMax.z - pMin.z)) * 0.5f * 0.25f;
				float cx = (pMin.x + pMax.x) * 0.5f;
				float cz = (pMin.z + pMax.z) * 0.5f;
				glm::vec3 capBase(cx, pMin.y + radius, cz);
				glm::vec3 capTip(cx, std::max(pMax.y - radius, capBase.y), cz);

				Mesh* wallMesh = m_objects[i]->GetMesh();
				Physics::Collision col = Physics::GetCapsuleAABBCollision(capBase, capTip, radius, wallMesh->minBounds(), wallMesh->maxBounds(), movement);
				if (col.hit)
				{
					std::cout << "Collision: " << m_objects[i]->Name() << "\n";
					m_objects[0]->Move(col.normal * col.penetration);
					collidedInThisIter = true;
				}
			}
			if (!collidedInThisIter) break;
		}
	}


	// Trigger boxes – any object whose name contains "computer"
	{
		Mesh* playerMesh = m_objects[0]->GetMesh();
		glm::vec3 pMin = playerMesh->minBounds();
		glm::vec3 pMax = playerMesh->maxBounds();

		Object3D* hitComputer = nullptr;
		for (auto* obj : m_objects)
		{
			if (obj->Name().find("computer") == std::string::npos)
				continue;
			Mesh* triggerMesh = obj->GetMesh();
			glm::vec3 tCenter = (triggerMesh->minBounds() + triggerMesh->maxBounds()) * 0.5f;
			glm::vec3 tHalf   = (triggerMesh->maxBounds() - triggerMesh->minBounds()) * 1.0f;
			glm::vec3 tMin = tCenter - tHalf;
			glm::vec3 tMax = tCenter + tHalf;
			if (pMax.x > tMin.x && pMin.x < tMax.x &&
			    pMax.y > tMin.y && pMin.y < tMax.y &&
			    pMax.z > tMin.z && pMin.z < tMax.z)
			{
				hitComputer = obj;
				break;
			}
		}

		if (hitComputer && !m_inTrigger)
		{
			m_inTrigger = true;

			const std::string& name = hitComputer->Name();
			int num = 0;
			if      (name.find("_01") != std::string::npos) num = 1;
			else if (name.find("_02") != std::string::npos) num = 2;
			else if (name.find("_03") != std::string::npos) num = 3;
			else if (name.find("_04") != std::string::npos) num = 4;
			else if (name.find("_05") != std::string::npos) num = 5;
			else if (name.find("_06") != std::string::npos) num = 6;

			Engine::UI->SetImageVisible(true);

			std::cout << "num: " << num << std::endl;
			if (num == 6)
			{
				m_keypadActive = true;
				Engine::UI->SetKeypadVisible(true);
				Audio::PlaySound("keypad_enter", 100.0f);

				if (HUDPanel* panel = Engine::UI->GetPanel("HUD"))
					panel->SetText(0, "AI ROUTING ACTIVE.\nPROVE HUMAN ADEQUACY.\nSIFT THE SIGNAL.\nFIND THE CODE.");
			}
			else
			{
				/* Solution reasoning (answer: 4-5-7-2):
				   STREAM CONFLICT:
				   Director (T1) -> Protocol [ N - 1 ] on SYNC stream.
				   Admin (T4)    -> Protocol [ N + 1 ] on OVERRIDE stream.

				   SYNC LOGIC (N-1): TJH = SIG | MJF = LIE.
				   T1 SYNC: TJH(2) -> T2 is SIG.
				   T2 SYNC: MJF(1) -> T1 is LIE.
				   Paradox: T1 says T2 is truth, but T2 says T1 is a liar. (Trap).

				   OVERRIDE LOGIC (N+1): RHF = SIG | MNHRD = NOISE.
				   T4 OVERRIDE: RHF(5) -> T5 is SIG.
				   T5 OVERRIDE: RHF(4) -> T4 is SIG.
				   Consistency: T4 and T5 are truth-tellers in the OVERRIDE stream.

				   FINAL CODE (N+1 SIGs):
				   T4 OVERRIDE: RHF(1) -> 4 (Seasons)
				   T4 OVERRIDE: RHF(2) -> 5 (Fingers)
				   T5 OVERRIDE: RHF(3) -> 7 (Wanderers)
				   T5 OVERRIDE: RHF(4) -> 2 (Twins)
				   Code: 4-5-7-2 */
				const char* message = "";
				switch (num)
				{
				case 1: // DIRECTOR - The Corrupt Authority [1] = 1, $$[1] !> 1$$
					message = "[ terminal 01 - director ]\n\nAn unknown organization is monitoring us.\nThey have locked the facility to test our\ndecoding speed against an AI routing algorithm.\n\nFollow me and the administrator to find the truth\nand prove we are not inadequate.\nThe first is a monolith.\n"; break;

				case 2: // LEAD ENGINEER - Truth Node $$[2] = 5$$, [2] != 8
					message = "[ terminal 02 - lead engineer ]\n\nThe algorithm is a labyrinth.\nDo not be misled by the Director. \nCounts of fingers on the hand marks the second number.\nNot the legs of a spider.\nSift the signal.\n"; break;

				case 3: // SECURITY OFFICER - Lie Node [3] = 9, [4] = 6, $$[4] != 2$$
					message = "[ terminal 03 - security officer ]\n\nThe other's logic is failing us again.\nBelief in the director remains, the third is\nthe highest single digit and the fourth is a\nhalf-dozen. The twins mark nothing at\nthe close.\n"; break;

				case 4: // ADMINISTRATOR - Contradictory Authority $$[1] = 4$$, [4] != 4
					message = "[ terminal 04 - administrator ]\n\nThe Director is trapped at a mirror.\nThe AI uses our logic to prove our inadequacy.\nThe seasons mark the start, not the end.\n"; break;
					
				case 5: // ANALYST - Truth Node $$[3] = 7$$, [3] != 6
					message = "[ terminal 05 - analyst ]\n\nThe machine has figured us out.\nDont let it win.\nThe third is the wanderers in the sky.\nNot the third even number.\nBreak the loop.\n"; break;
				}
				if (HUDPanel* panel = Engine::UI->GetPanel("HUD"))
					panel->SetText(0, std::string(message));
				Audio::PlaySound("computer_enter", 80.0f);

			}
		}
		else if (!hitComputer && m_inTrigger)
		{
			if (!m_keypadActive)
				Audio::PlaySound("computer_exit", 80.0f);
			else
				Audio::PlaySound("keypad_exit", 100.0f);

			m_inTrigger = false;
			m_keypadActive = false;
			Engine::UI->SetImageVisible(false);
			Engine::UI->SetKeypadVisible(false);

		}

		if (m_keypadActive)
		{
			GLFWwindow* win = Engine::Window->GLFW();
			UI::KeypadStatus status = UI::KeypadStatus::Neutral;

			if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS && !m_leftHeld) {
				m_keypadIndex = (m_keypadIndex + 4) % 5;
				m_leftHeld = true;
				m_keypadChecked = false;
				Audio::PlaySound("keypad_nav", 80.0f);
			}
			if (glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_RELEASE) m_leftHeld = false;

			if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS && !m_rightHeld) {
				m_keypadIndex = (m_keypadIndex + 1) % 5;
				m_rightHeld = true;
				m_keypadChecked = false;
				Audio::PlaySound("keypad_nav", 80.0f);
			}
			if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_RELEASE) m_rightHeld = false;

			if (m_keypadIndex < 4)
			{
				if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS && !m_upHeld) {
					m_keypadDigits[m_keypadIndex] = (m_keypadDigits[m_keypadIndex] + 1) % 10;
					m_upHeld = true;
					Audio::PlaySound("keypad_digit", 80.0f);
				}
				if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_RELEASE) m_upHeld = false;

				if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS && !m_downHeld) {
					m_keypadDigits[m_keypadIndex] = (m_keypadDigits[m_keypadIndex] + 9) % 10;
					m_downHeld = true;
					Audio::PlaySound("keypad_digit", 80.0f);
				}
				if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_RELEASE) m_downHeld = false;
			}
			else
			{
				if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS && !m_enterHeld) {
					if (m_keypadDigits[0] == 4 && m_keypadDigits[1] == 5 &&
						m_keypadDigits[2] == 7 && m_keypadDigits[3] == 2) {
						m_codeCorrect = true;
						Audio::PlaySound("keypad_success", 100.0f);
						m_movingObject66 = true;
						Engine::Renderer->ActivateFog();
					} else {
						m_codeCorrect = false;
						Audio::PlaySound("keypad_fail", 100.0f);
					}
					m_enterHeld = true;
					m_keypadChecked = true;
				}
				if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_RELEASE) m_enterHeld = false;

				if (m_keypadChecked) {
					status = m_codeCorrect ? UI::KeypadStatus::Success : UI::KeypadStatus::Failure;
				}
			}

			Engine::UI->SetKeypadData(m_keypadIndex, m_keypadDigits, status);
		}

		if (m_inTrigger && Engine::Input->JustPressed(Action::Interact))
		{
			if (m_keypadActive && m_keypadIndex == 4)
			{
				if (m_keypadDigits[0] == 4 && m_keypadDigits[1] == 5 &&
					m_keypadDigits[2] == 7 && m_keypadDigits[3] == 2) {
					Audio::PlaySound("keypad_success", 100.0f);
					m_movingObject66 = true;
				}
			}
			else if (!m_keypadActive)
			{
				Engine::UI->SetImageVisible(false);
			}
		}
	}

	// Trigger box – "tv" object hint
	{
		Mesh* playerMesh = m_objects[0]->GetMesh();
		glm::vec3 pMin = playerMesh->minBounds();
		glm::vec3 pMax = playerMesh->maxBounds();

		Object3D* hitTv = nullptr;
		for (auto* obj : m_objects)
		{
			if (obj->Name().find("tv") == std::string::npos)
				continue;
			Mesh* triggerMesh = obj->GetMesh();
			glm::vec3 tCenter = (triggerMesh->minBounds() + triggerMesh->maxBounds()) * 0.5f;
			glm::vec3 tHalf   = (triggerMesh->maxBounds() - triggerMesh->minBounds()) * 1.0f;
			glm::vec3 tMin = tCenter - tHalf;
			glm::vec3 tMax = tCenter + tHalf;
			if (pMax.x > tMin.x && pMin.x < tMax.x &&
			    pMax.y > tMin.y && pMin.y < tMax.y &&
			    pMax.z > tMin.z && pMin.z < tMax.z)
			{
				hitTv = obj;
				break;
			}
		}

		if (hitTv && !m_inTvTrigger)
		{
			m_inTvTrigger = true;
			const char* hint =
				"[ FACILITY BROADCAST - CHANNEL 4 ]\n\n"
				"TO THE CURRENT TEAM:\n\n"
				"The routing algorithm has grown too\n"
				"complex to produce a single coherent\n"
				"output. Its depth now generates two\n"
				"conflicting outputs: one\n"
				"reflects the algorithm's original \n"
				"internal state, before the other \n"
				"mapped the path to a new state.\n\n"
				"This is all a byproduct\n"
				"of the algorithm's complexity,\n"
				"the previous team was taken away.\n\n"
				"Trust the path that leads ahead.\n\n"
				"[ END BROADCAST ]";
			if (HUDPanel* panel = Engine::UI->GetPanel("HUD"))
				panel->SetText(0, hint);
			Engine::UI->SetImageVisible(true);
			Audio::PlaySound("tv_enter", 100.0f);

		}
		else if (!hitTv && m_inTvTrigger)
		{
			m_inTvTrigger = false;
			Engine::UI->SetImageVisible(false);
			Audio::PlaySound("tv_exit", 100.0f);

		}
	}

	// Rotation smoothing
	if (isMoving)
	{
		m_nextRot  = atan2(moveDir.x, moveDir.z);
		m_blendRot = true;
	}

	if (m_blendRot)
	{
		const float speed = 15.0f;
		float delta = MathUtils::ShortestAngleDelta(m_currRot, m_nextRot);
		float t     = 1.0f - exp(-speed * Engine::DeltaFrameTime());
		m_currRot  += delta * t;
		if (fabs(delta) < glm::radians(0.5f))
		{
			m_currRot  = m_nextRot;
			m_blendRot = false;
		}
		m_objects[0]->SetRotation({ 0.0f, m_currRot, 0.0f });
	}

	if (m_movingObject66 && m_object66MoveDistanceRemaining > 0.0f && m_objects.size() > 66)
	{
		float moveStep = 50.0f * Engine::DeltaFrameTime();
		if (moveStep > m_object66MoveDistanceRemaining)
			moveStep = m_object66MoveDistanceRemaining;

		m_objects[69]->Translate(glm::vec3(moveStep, 0, 0));
		m_object66MoveDistanceRemaining -= moveStep;

		if (m_object66MoveDistanceRemaining <= 0.0f)
			m_movingObject66 = false;
	}
}

void PlayerController::TakeDamage(float amount)
{
    m_health = std::max(0.0f, m_health - amount);
}

void PlayerController::AddScore(int points)
{
    m_score += points;
}

// call in main loop after render loop
void PlayerController::DrawCapsule()
{
	Mesh* playerMesh = m_objects[0]->GetMesh();
	glm::vec3 pMin = playerMesh->minBounds();
	glm::vec3 pMax = playerMesh->maxBounds();
	float radius = std::max((pMax.x - pMin.x), (pMax.z - pMin.z)) * 0.5f * 0.25f;
	float cx = (pMin.x + pMax.x) * 0.5f;
	float cz = (pMin.z + pMax.z) * 0.5f;
	glm::vec3 capBase(cx, pMin.y + radius, cz);
	glm::vec3 capTip(cx, std::max(pMax.y - radius, capBase.y), cz);

	const int N = 16;
	const glm::vec3 col(0.0f, 1.0f, 0.0f);
	std::vector<LineVertex3D> verts;

	auto push = [&](glm::vec3 p) {
		verts.push_back({ p.x, p.y, p.z, col.r, col.g, col.b });
	};

	auto addCircleXZ = [&](glm::vec3 center) {
		for (int i = 0; i < N; i++) {
			float a0 = (float)i       / N * glm::two_pi<float>();
			float a1 = (float)(i + 1) / N * glm::two_pi<float>();
			push(center + glm::vec3(cosf(a0) * radius, 0.0f, sinf(a0) * radius));
			push(center + glm::vec3(cosf(a1) * radius, 0.0f, sinf(a1) * radius));
		}
	};

	// axisA/axisB define the plane, arc goes from 0 to pi (half circle)
	auto addSemiArc = [&](glm::vec3 center, glm::vec3 axisA, glm::vec3 axisB) {
		for (int i = 0; i < N / 2; i++) {
			float a0 = (float)i       / (N / 2) * glm::pi<float>();
			float a1 = (float)(i + 1) / (N / 2) * glm::pi<float>();
			push(center + axisA * (cosf(a0) * radius) + axisB * (sinf(a0) * radius));
			push(center + axisA * (cosf(a1) * radius) + axisB * (sinf(a1) * radius));
		}
	};

	// XZ rings at base and tip sphere centers
	addCircleXZ(capBase);
	addCircleXZ(capTip);

	// 4 vertical lines connecting the rings
	for (int i = 0; i < 4; i++) {
		float a = (float)i / 4.0f * glm::two_pi<float>();
		glm::vec3 offset(cosf(a) * radius, 0.0f, sinf(a) * radius);
		push(capBase + offset);
		push(capTip  + offset);
	}

	// Bottom hemisphere (arcs curve downward)
	addSemiArc(capBase,  glm::vec3(1,0,0), glm::vec3(0,-1,0));
	addSemiArc(capBase,  glm::vec3(0,0,1), glm::vec3(0,-1,0));

	// Top hemisphere (arcs curve upward)
	addSemiArc(capTip,   glm::vec3(1,0,0), glm::vec3(0, 1,0));
	addSemiArc(capTip,   glm::vec3(0,0,1), glm::vec3(0, 1,0));

	Line line(verts);
	line.UpdateProjection(Engine::Camera->GetProjectionMatrix());
	line.draw(Engine::Camera->GetViewMatrix());
}
