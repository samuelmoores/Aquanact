#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

class FrameAllocator {
public:
	FrameAllocator() = default;
	explicit FrameAllocator(std::size_t capacityBytes)
	{
		ResetCapacity(capacityBytes);
	}

	~FrameAllocator()
	{
		delete[] m_buffer;
	}

	FrameAllocator(const FrameAllocator&) = delete;
	FrameAllocator& operator=(const FrameAllocator&) = delete;

	FrameAllocator(FrameAllocator&& other) noexcept
	{
		*this = std::move(other);
	}

	FrameAllocator& operator=(FrameAllocator&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		delete[] m_buffer;
		m_buffer = other.m_buffer;
		m_capacityBytes = other.m_capacityBytes;
		m_usedBytes = other.m_usedBytes;
		m_peakBytes = other.m_peakBytes;

		other.m_buffer = nullptr;
		other.m_capacityBytes = 0;
		other.m_usedBytes = 0;
		other.m_peakBytes = 0;
		return *this;
	}

	void Reset()
	{
		m_usedBytes = 0;
	}

	void ResetCapacity(std::size_t capacityBytes)
	{
		delete[] m_buffer;
		m_buffer = nullptr;
		m_capacityBytes = 0;
		m_usedBytes = 0;
		m_peakBytes = 0;

		if (capacityBytes == 0)
		{
			return;
		}

		m_buffer = new std::byte[capacityBytes];
		m_capacityBytes = capacityBytes;
	}

	void Reserve(std::size_t capacityBytes)
	{
		if (capacityBytes <= m_capacityBytes)
		{
			return;
		}

		std::byte* newBuffer = new std::byte[capacityBytes];
		if (m_buffer && m_usedBytes > 0)
		{
			for (std::size_t i = 0; i < m_usedBytes; ++i)
			{
				newBuffer[i] = m_buffer[i];
			}
		}

		delete[] m_buffer;
		m_buffer = newBuffer;
		m_capacityBytes = capacityBytes;
	}

	void* Allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		if (bytes == 0)
		{
			return nullptr;
		}

		if (!m_buffer || alignment == 0)
		{
			return nullptr;
		}

		const std::size_t alignedOffset = AlignUp(m_usedBytes, alignment);
		if (alignedOffset > m_capacityBytes || bytes > m_capacityBytes - alignedOffset)
		{
			return nullptr;
		}

		void* result = m_buffer + alignedOffset;
		m_usedBytes = alignedOffset + bytes;
		if (m_usedBytes > m_peakBytes)
		{
			m_peakBytes = m_usedBytes;
		}
		return result;
	}

	template <typename T, typename... Args>
	T* Create(Args&&... args)
	{
		void* memory = Allocate(sizeof(T), alignof(T));
		if (!memory)
		{
			return nullptr;
		}

		return new (memory) T(std::forward<Args>(args)...);
	}

	template <typename T>
	T* AllocateArray(std::size_t count)
	{
		if (count == 0)
		{
			return nullptr;
		}

		void* memory = Allocate(sizeof(T) * count, alignof(T));
		return static_cast<T*>(memory);
	}

	std::size_t CapacityBytes() const
	{
		return m_capacityBytes;
	}

	std::size_t UsedBytes() const
	{
		return m_usedBytes;
	}

	std::size_t PeakBytes() const
	{
		return m_peakBytes;
	}

	bool HasCapacity(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) const
	{
		const std::size_t alignedOffset = AlignUp(m_usedBytes, alignment);
		return alignedOffset <= m_capacityBytes && bytes <= m_capacityBytes - alignedOffset;
	}

private:
	static std::size_t AlignUp(std::size_t value, std::size_t alignment)
	{
		const std::size_t mask = alignment - 1;
		return (value + mask) & ~mask;
	}

	std::byte* m_buffer = nullptr;
	std::size_t m_capacityBytes = 0;
	std::size_t m_usedBytes = 0;
	std::size_t m_peakBytes = 0;
};

