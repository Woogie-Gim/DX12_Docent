#include "Timer.h"

// 생성자 (타이머 주파수 초기화)
Timer::Timer() : mDeltaTime(-1.0f), mPrevTime(0), mCurrTime(0)
{
    __int64 countsPerSec;
    QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
    mSecondsPerCount = 1.0 / (double)countsPerSec;

    QueryPerformanceCounter((LARGE_INTEGER*)&mPrevTime);
}

// 프레임 시간 갱신
void Timer::Tick()
{
    QueryPerformanceCounter((LARGE_INTEGER*)&mCurrTime);

    mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
    mPrevTime = mCurrTime;

    // 디버그 중단점 등으로 인해 시간이 튀는 현상 방지
    if (mDeltaTime < 0.0) mDeltaTime = 0.0;
}

// 델타 타임 반환
float Timer::DeltaTime() const
{
    return mDeltaTime;
}