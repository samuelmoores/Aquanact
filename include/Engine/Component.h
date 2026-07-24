#pragma once

class Entity;

class Component {
public:
	virtual ~Component() = default;

	virtual const char* Name() const = 0;
	virtual void Update(Entity&, float) {}
	virtual void FirstFrame(Entity&) {}
};
