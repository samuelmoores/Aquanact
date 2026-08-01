#pragma once

#include <chrono>
#include <string>
#include <vector>

class FrameProfiler
{
public:
	struct Sample
	{
		std::string name;
		double currentMs = 0.0;
		double averageMs = 0.0;
		double maximumMs = 0.0;
	};

	class Scope
	{
	public:
		Scope(FrameProfiler& profiler, const char* name);
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
		~Scope();

	private:
		FrameProfiler* m_profiler = nullptr;
		const char* m_name = nullptr;
		std::chrono::steady_clock::time_point m_start;
	};

	void BeginFrame();
	void EndFrame();
	void SetEnabled(bool enabled);
	bool IsEnabled() const;
	std::vector<Sample> Samples() const;
	double FrameMs() const;
	double SmoothedFps() const;

private:
	struct Accumulator
	{
		double currentMs = 0.0;
		double averageMs = 0.0;
		double maximumMs = 0.0;
	};

	void Record(const std::string& name, double milliseconds);

	std::chrono::steady_clock::time_point m_frameStart;
	std::vector<std::pair<std::string, Accumulator>> m_samples;
	std::vector<std::pair<std::string, double>> m_currentSamples;
	double m_frameMs = 0.0;
	double m_smoothedFps = 0.0;
	bool m_enabled = false;
	bool m_frameActive = false;

	friend class Scope;
};
