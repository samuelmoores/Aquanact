#include "Game/GameManager.h"

#include "Engine/Core/Entity.h"

void GameManager::startUp(Entity&)
{
	m_overlapCount = 0;
	m_elapsedSeconds = 0.0f;
}

void GameManager::Update(Entity&, float dt)
{
	if (dt <= 0.0f)
		return;

	m_elapsedSeconds += dt;
	DispatchBindableValueChanged("ElapsedSeconds");
}

void GameManager::OnTriggerEnter(Entity&)
{
	RecordOverlap();
}

void GameManager::RecordOverlap()
{
	++m_overlapCount;
	DispatchBindableValueChanged("OverlapCount");
}
