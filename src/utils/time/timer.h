/**
 * @file timer.h
 * @brief AeroJS タイマーユーティリティ
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_UTILS_TIME_TIMER_H
#define AEROJS_UTILS_TIME_TIMER_H

#include <chrono>
#include <cstdint>

namespace aerojs {
namespace utils {

/**
 * @brief 高精度タイマークラス
 */
class Timer {
public:
    Timer();
    ~Timer();

    // タイマー操作
    void start();
    void stop();
    void reset();

    // 時間取得
    uint64_t getElapsedNanoseconds() const;
    uint64_t getElapsedMicroseconds() const;
    uint64_t getElapsedMilliseconds() const;
    double getElapsedSeconds() const;

    // 静的メソッド
    static uint64_t getCurrentTimeNanos();
    static uint64_t getCurrentTimeMicros();
    static uint64_t getCurrentTimeMillis();

private:
    std::chrono::high_resolution_clock::time_point startTime_;
    std::chrono::high_resolution_clock::time_point endTime_;
    bool isRunning_;
};

} // namespace utils
} // namespace aerojs

#endif // AEROJS_UTILS_TIME_TIMER_H 