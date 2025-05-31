/**
 * @file timer.cpp
 * @brief AeroJS タイマーユーティリティ実装
 * @version 0.1.0
 * @license MIT
 */

#include "timer.h"

namespace aerojs {
namespace utils {

Timer::Timer() : isRunning_(false) {
}

Timer::~Timer() = default;

void Timer::start() {
    startTime_ = std::chrono::high_resolution_clock::now();
    isRunning_ = true;
}

void Timer::stop() {
    if (isRunning_) {
        endTime_ = std::chrono::high_resolution_clock::now();
        isRunning_ = false;
    }
}

void Timer::reset() {
    isRunning_ = false;
    startTime_ = std::chrono::high_resolution_clock::time_point{};
    endTime_ = std::chrono::high_resolution_clock::time_point{};
}

uint64_t Timer::getElapsedNanoseconds() const {
    auto end = isRunning_ ? std::chrono::high_resolution_clock::now() : endTime_;
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startTime_);
    return static_cast<uint64_t>(duration.count());
}

uint64_t Timer::getElapsedMicroseconds() const {
    return getElapsedNanoseconds() / 1000;
}

uint64_t Timer::getElapsedMilliseconds() const {
    return getElapsedNanoseconds() / 1000000;
}

double Timer::getElapsedSeconds() const {
    return static_cast<double>(getElapsedNanoseconds()) / 1000000000.0;
}

uint64_t Timer::getCurrentTimeNanos() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

uint64_t Timer::getCurrentTimeMicros() {
    return getCurrentTimeNanos() / 1000;
}

uint64_t Timer::getCurrentTimeMillis() {
    return getCurrentTimeNanos() / 1000000;
}

} // namespace utils
} // namespace aerojs 