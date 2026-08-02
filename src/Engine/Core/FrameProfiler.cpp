#include "Engine/Core/FrameProfiler.h"

#include <algorithm>

FrameProfiler::Scope::Scope(FrameProfiler& profiler, const char* name)
	: m_profiler(profiler.IsEnabled() ? &profiler : nullptr)
	, m_name(name ? name : "Unnamed")
	, m_start(std::chrono::steady_clock::now())
{
}

FrameProfiler::Scope::~Scope()
{
	if (!m_profiler)
	{
		return;
	}

	const auto elapsed = std::chrono::steady_clock::now() - m_start;
	m_profiler->Record(m_name, std::chrono::duration<double, std::milli>(elapsed).count());
}

void FrameProfiler::BeginFrame()
{
	m_frameStart = std::chrono::steady_clock::now();
	if (m_enabled)
	{
		m_currentSamples.clear();
		for (auto& entry : m_samples)
		{
			entry.second.currentMs = 0.0;
		}
	}
	m_frameActive = true;
}

void FrameProfiler::EndFrame()
{
	if (!m_frameActive)
	{
		return;
	}

	const auto elapsed = std::chrono::steady_clock::now() - m_frameStart;
	m_frameMs = std::chrono::duration<double, std::milli>(elapsed).count();
	if (m_enabled)
	{
		Record("Frame", m_frameMs);
		for (const auto& current : m_currentSamples)
		{
			auto sample = std::find_if(m_samples.begin(), m_samples.end(), [&current](const auto& entry)
			{
				return entry.first == current.first;
			});
			if (sample == m_samples.end())
			{
				continue;
			}

			sample->second.averageMs = sample->second.averageMs <= 0.0
				? current.second
				: (sample->second.averageMs * 0.9) + (current.second * 0.1);
			sample->second.maximumMs = std::max(sample->second.maximumMs, current.second);
		}
	}
	const double instantFps = m_frameMs > 0.0 ? 1000.0 / m_frameMs : 0.0;
	m_smoothedFps = m_smoothedFps <= 0.0
		? instantFps
		: (m_smoothedFps * 0.9) + (instantFps * 0.1);
	m_frameActive = false;
}

void FrameProfiler::SetEnabled(bool enabled)
{
	m_enabled = enabled;
	if (!enabled)
	{
		m_currentSamples.clear();
	}
}

bool FrameProfiler::IsEnabled() const
{
	return m_enabled;
}

void FrameProfiler::Record(const std::string& name, double milliseconds)
{
	if (!m_enabled || !m_frameActive)
	{
		return;
	}

	auto current = std::find_if(m_currentSamples.begin(), m_currentSamples.end(), [&name](const auto& sample)
	{
		return sample.first == name;
	});
	if (current == m_currentSamples.end())
	{
		m_currentSamples.emplace_back(name, milliseconds);
	}
	else
	{
		current->second += milliseconds;
	}

	auto sample = std::find_if(m_samples.begin(), m_samples.end(), [&name](const auto& entry)
	{
		return entry.first == name;
	});
	if (sample == m_samples.end())
	{
		m_samples.emplace_back(name, Accumulator{});
		sample = std::prev(m_samples.end());
	}
	sample->second.currentMs += milliseconds;
}

std::vector<FrameProfiler::Sample> FrameProfiler::Samples() const
{
	std::vector<Sample> samples;
	samples.reserve(m_samples.size());
	for (const auto& entry : m_samples)
	{
		Sample sample;
		sample.name = entry.first;
		sample.currentMs = entry.second.currentMs;
		sample.averageMs = entry.second.averageMs;
		sample.maximumMs = entry.second.maximumMs;
		samples.push_back(std::move(sample));
	}
	return samples;
}

double FrameProfiler::FrameMs() const
{
	return m_frameMs;
}

double FrameProfiler::SmoothedFps() const
{
	return m_smoothedFps;
}
