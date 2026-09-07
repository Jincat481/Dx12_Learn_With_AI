#pragma once
#include "Core/stdafx.h"

// =============================================================
// TimeManager (과제 2 / S07, S08)
//  QueryPerformanceCounter 기반 고해상도 타이머.
//  중단점(break point) 이후 delta time 이 폭주하지 않도록 상한을 둔다.
// =============================================================
class TimeManager
{
public:
    static TimeManager& Get();

    void Initialize();
    void Update();

    float  GetDeltaTime() const { return m_deltaTime; }
    float  GetTotalTime() const { return m_totalTime; }
    float  GetFPS()       const { return m_fps; }
    uint64_t GetFrameCount() const { return m_frameCount; }

    void SetMaxDeltaTime(float seconds) { m_maxDeltaTime = seconds; }

private:
    TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    LARGE_INTEGER m_frequency = {};
    LARGE_INTEGER m_prevCounter = {};

    float    m_deltaTime = 0.0f;
    float    m_totalTime = 0.0f;
    float    m_maxDeltaTime = 0.25f;   // 한 프레임 최대 0.25초로 제한

    float    m_fps = 0.0f;
    float    m_fpsTimer = 0.0f;
    int      m_fpsFrameCount = 0;
    uint64_t m_frameCount = 0;
};
