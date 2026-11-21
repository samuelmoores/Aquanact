#include "Engine/Core/BootLog.h"

#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#endif
#endif

namespace
{
	std::filesystem::path g_logPath;
	std::mutex g_logMutex;
	int g_logFd = -1;
	constexpr std::array<const char*, 19> g_stages = {
		"process entry", "Root construction", "owned systems initialized", "window startup",
		"audio startup", "RenderManager startup", "FrontEndManager startup", "EngineGUI startup",
		"GameGUICreator startup", "GameGUI startup", "input startup", "debug startup",
		"file manager startup", "component registration", "scene startup", "gameplay startup",
		"application running", "application shutdown", "shutdown complete"
	};
	volatile std::sig_atomic_t g_stageIndex = 0;

	void AppendLine(const char* line) noexcept
	{
		if (g_logPath.empty() || !line)
		{
			return;
		}
		try
		{
			std::lock_guard<std::mutex> lock(g_logMutex);
			std::FILE* file = std::fopen(g_logPath.string().c_str(), "ab");
			if (file)
			{
				std::fputs(line, file);
				std::fputc('\n', file);
				std::fflush(file);
				std::fclose(file);
			}
		}
		catch (...)
		{
		}
	}

#ifdef _WIN32
	LONG WINAPI CrashExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
	{
		char line[256] = {};
		const DWORD code = exceptionInfo && exceptionInfo->ExceptionRecord
			? exceptionInfo->ExceptionRecord->ExceptionCode : 0;
		std::snprintf(line, sizeof(line), "FATAL: unhandled Windows exception 0x%08lx at boot stage '%s'",
			static_cast<unsigned long>(code), g_stages[static_cast<std::size_t>(g_stageIndex)]);
		AppendLine(line);

		void* frames[64] = {};
		const USHORT frameCount = CaptureStackBackTrace(0, 64, frames, nullptr);
		for (USHORT index = 0; index < frameCount; ++index)
		{
			std::snprintf(line, sizeof(line), "  frame %u: %p", static_cast<unsigned>(index), frames[index]);
			AppendLine(line);
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}
#else
	void CrashSignalHandler(int signalNumber) noexcept
	{
		const char* signalName = "unknown signal";
		switch (signalNumber)
		{
		case SIGSEGV: signalName = "SIGSEGV (segmentation fault)"; break;
		case SIGABRT: signalName = "SIGABRT (abort)"; break;
		case SIGBUS: signalName = "SIGBUS (bus error)"; break;
		case SIGILL: signalName = "SIGILL (illegal instruction)"; break;
		case SIGFPE: signalName = "SIGFPE (arithmetic fault)"; break;
		}

		char line[256] = {};
		std::snprintf(line, sizeof(line), "FATAL: %s (%d) at boot stage '%s'",
			signalName, signalNumber, g_stages[static_cast<std::size_t>(g_stageIndex)]);

#if defined(__APPLE__) || defined(__linux__)
		if (g_logFd >= 0)
		{
			write(g_logFd, line, std::strlen(line));
			constexpr char stackHeader[] = "\nNative stack trace:\n";
			write(g_logFd, stackHeader, sizeof(stackHeader) - 1);
		}
		void* frames[64] = {};
		const int frameCount = backtrace(frames, 64);
		if (g_logFd >= 0)
		{
			backtrace_symbols_fd(frames, frameCount, g_logFd);
			fsync(g_logFd);
		}
#endif
		std::signal(signalNumber, SIG_DFL);
		std::raise(signalNumber);
		std::_Exit(128 + signalNumber);
	}
#endif
}

void BootLog::Initialize(const char* executablePath)
{
	(void)executablePath;
	std::filesystem::path executableDirectory = std::filesystem::current_path();
#ifdef _WIN32
	wchar_t pathBuffer[32768] = {};
	const DWORD pathLength = GetModuleFileNameW(nullptr, pathBuffer, static_cast<DWORD>(std::size(pathBuffer)));
	if (pathLength > 0 && pathLength < std::size(pathBuffer))
		executableDirectory = std::filesystem::path(pathBuffer).parent_path();
#elif defined(__APPLE__)
	std::vector<char> pathBuffer(1024);
	for (;;)
	{
		std::uint32_t pathSize = static_cast<std::uint32_t>(pathBuffer.size());
		if (_NSGetExecutablePath(pathBuffer.data(), &pathSize) == 0)
		{
			executableDirectory = std::filesystem::path(pathBuffer.data()).parent_path();
			break;
		}
		if (pathSize <= pathBuffer.size())
			break;
		pathBuffer.resize(pathSize);
	}
#else
	if (executablePath && *executablePath)
	{
		std::error_code error;
		const auto absolutePath = std::filesystem::absolute(executablePath, error);
		if (!error)
			executableDirectory = absolutePath.parent_path();
	}
#endif
	{
		std::error_code error;
		const auto absoluteDirectory = std::filesystem::absolute(executableDirectory, error);
		if (!error)
			executableDirectory = absoluteDirectory;
	}

	try
	{
		const auto logDirectory = executableDirectory / "logs";
		std::filesystem::create_directories(logDirectory);
		g_logPath = logDirectory / "aquanact-boot.txt";
#ifndef _WIN32
		g_logFd = open(g_logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
#endif
		AppendLine("Aquanact boot log");
		AppendLine((std::string("Executable directory: ") + executableDirectory.string()).c_str());
		AppendLine((std::string("Working directory: ") + std::filesystem::current_path().string()).c_str());
	}
	catch (...)
	{
		g_logPath.clear();
	}

#ifdef _WIN32
	SetUnhandledExceptionFilter(CrashExceptionHandler);
#else
	struct sigaction action {};
	action.sa_handler = CrashSignalHandler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESETHAND;
	for (const int signalNumber : { SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE })
		sigaction(signalNumber, &action, nullptr);
#endif
	SetStage("process entry");
}

void BootLog::SetStage(const char* stage)
{
	std::size_t index = 0;
	for (; index < g_stages.size(); ++index)
	{
		if (stage && std::strcmp(stage, g_stages[index]) == 0)
			break;
	}
	if (index >= g_stages.size())
		index = 0;
	g_stageIndex = static_cast<std::sig_atomic_t>(index);
	char line[192] = {};
	std::snprintf(line, sizeof(line), "BOOT: %s", g_stages[index]);
	AppendLine(line);
}

void BootLog::Write(const std::string& message)
{
	AppendLine(message.c_str());
}

void BootLog::WriteException(const char* context, const char* message)
{
	char line[1024] = {};
	std::snprintf(line, sizeof(line), "ERROR: %s: %s (boot stage '%s')",
		context ? context : "unhandled exception",
		message ? message : "no exception detail",
		g_stages[static_cast<std::size_t>(g_stageIndex)]);
	AppendLine(line);
}
