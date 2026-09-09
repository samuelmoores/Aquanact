#include "Game/Health.h"

#include "Engine/Core/Entity.h"

#include <algorithm>
#include <cmath>

void Health::startUp(Entity&)
{
	m_currentHealth = m_maxHealth;
}

void Health::SetMaxHealth(float value)
{
	if (!std::isfinite(value))
		return;
	m_maxHealth = std::max(1.0f, value);
	m_currentHealth = std::min(m_currentHealth, m_maxHealth);
}

bool Health::ReceiveDamage(Entity&, float amount)
{
	if (!std::isfinite(amount) || amount <= 0.0f || IsDead())
		return false;

	const bool wasAlive = !IsDead();
	m_currentHealth = std::max(0.0f, m_currentHealth - amount);
	DispatchBindableValueChanged("CurrentHealth");
	DispatchBindableValueChanged("IsDead");
	DispatchBindableEvent("DamageTaken");
	if (wasAlive && IsDead())
		DispatchBindableEvent("Died");
	return true;
}
