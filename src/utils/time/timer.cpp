/**
 * @file timer.cpp
 * @brief AeroJS タイマー実装
 * @version 0.1.0
 * @license MIT
 */

#include "timer.h"

namespace aerojs {
namespace utils {

Timer::Timer() : isRunning_(false) {
    reset();
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
    startTime_ = std::chrono::high_resolution_clock::now();
    endTime_ = startTime_;
    isRunning_ = false;
}

uint64_t Timer::getElapsedNanoseconds() const {
    auto end = isRunning_ ? std::chrono::high_resolution_clock::now() : endTime_;
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startTime_);
    return duration.count();
}

uint64_t Timer::getElapsedMicroseconds() const {
    auto end = isRunning_ ? std::chrono::high_resolution_clock::now() : endTime_;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - startTime_);
    return duration.count();
}

uint64_t Timer::getElapsedMilliseconds() const {
    auto end = isRunning_ ? std::chrono::high_resolution_clock::now() : endTime_;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime_);
    return duration.count();
}

double Timer::getElapsedSeconds() const {
    auto end = isRunning_ ? std::chrono::high_resolution_clock::now() : endTime_;
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startTime_);
    return duration.count() / 1000000000.0;
}

uint64_t Timer::getCurrentTimeNanos() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

uint64_t Timer::getCurrentTimeMicros() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

uint64_t Timer::getCurrentTimeMillis() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace utils
} // namespace aerojs 