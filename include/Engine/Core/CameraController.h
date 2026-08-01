#pragma once

class Input;

class CameraController {
public:
	virtual ~CameraController() = default;
	virtual void Update(const Input& input) = 0;
};

