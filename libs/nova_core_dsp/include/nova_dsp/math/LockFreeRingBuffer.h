#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <algorithm>

namespace nova::dsp {

/**
 * @brief Lock-free, allocation-free Single-Producer Single-Consumer (SPSC) ring buffer.
 * Used to pass audio samples from the realtime audio thread to the 60-FPS GPU visualizer.
 */
template <typename T, std::size_t Capacity>
class LockFreeRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    LockFreeRingBuffer() = default;

    void clear() noexcept {
        writeIndex_.store(0, std::memory_order_relaxed);
        readIndex_.store(0, std::memory_order_relaxed);
    }

    bool push(const T& item) noexcept {
        const auto write = writeIndex_.load(std::memory_order_relaxed);
        const auto nextWrite = (write + 1) & kMask;
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }
        buffer_[write] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);
        return true;
    }

    void pushOverwrite(const T& item) noexcept {
        const auto write = writeIndex_.load(std::memory_order_relaxed);
        const auto nextWrite = (write + 1) & kMask;
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            readIndex_.store((readIndex_.load(std::memory_order_relaxed) + 1) & kMask,
                             std::memory_order_release);
        }
        buffer_[write] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);
    }

    bool pop(T& outItem) noexcept {
        const auto read = readIndex_.load(std::memory_order_relaxed);
        if (read == writeIndex_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }
        outItem = buffer_[read];
        readIndex_.store((read + 1) & kMask, std::memory_order_release);
        return true;
    }

    std::size_t popAvailable(T* dest, std::size_t maxCount) noexcept {
        std::size_t count = 0;
        while (count < maxCount && pop(dest[count])) {
            ++count;
        }
        return count;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;
    std::array<T, Capacity> buffer_{};
    alignas(64) std::atomic<std::size_t> writeIndex_{0};
    alignas(64) std::atomic<std::size_t> readIndex_{0};
};

} // namespace nova::dsp
