#pragma once
#include <windows.h>

class Timer
{
public:
    Timer();

    // 프레임당 시간 계산
    void Tick();
    // 델타 타임 반환 (초 단위)
    float DeltaTime() const;

private:
    double mSecondsPerCount;
    float mDeltaTime;
    __int64 mPrevTime;
    __int64 mCurrTime;
};