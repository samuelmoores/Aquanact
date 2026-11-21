#pragma once

#include <string>

class BootLog
{
public:
	static void Initialize(const char* executablePath);
	static void SetStage(const char* stage);
	static void Write(const std::string& message);
	static void WriteException(const char* context, const char* message);
};
