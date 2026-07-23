#pragma once

class Object3D;

class Component {
public:
	virtual ~Component() = default;

	virtual const char* Name() const = 0;
	virtual void Update(Object3D&, float) {}
};
