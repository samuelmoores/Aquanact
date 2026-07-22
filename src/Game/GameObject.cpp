#include "Game/GameObject.h"

GameObject::GameObject(std::string name)
	: m_name(std::move(name))
{
}
