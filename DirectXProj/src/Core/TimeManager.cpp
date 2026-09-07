#include "Core/stdafx.h"
#include "Core/TimeManager.h"

TimeManager& TimeManager::Get()
{
    static TimeManager instance;
    return instance;
}

void TimeManager::Initialize()
{
    ::QueryPerformanceFrequency(&m_frequency);
    ::QueryPerformanceCounter(&m_prevCounter);

    m_deltaTime = 0.0f;
    m_totalTime = 0.0f;
    m_fps = 0.0f;
    m_fpsTimer = 0.0f;
    m_fpsFrameCount = 0;
    m_frameCount = 0;
}

void TimeManager::Update()
{
    LARGE_INTEGER current = {};
    ::QueryPerformanceCounter(&current);

    const long long ticks = current.QuadPart - m_prevCounter.QuadPart;
    m_prevCounter = current;

    if (m_frequency.QuadPart == 0)
    {
        m_deltaTime = 0.0f;
        return;
    }

    m_deltaTime = static_cast<float>(static_cast<double>(ticks) / static_cast<double>(m_frequency.QuadPart));

    // 디버깅으로 멈춰 있었던 시간이 그대로 게임 로직에 들어가면 객체가 순간이동한다.
    if (m_deltaTime < 0.0f)            m_deltaTime = 0.0f;
    if (m_deltaTime > m_maxDeltaTime)  m_deltaTime = m_maxDeltaTime;

    m_totalTime += m_deltaTime;
    ++m_frameCount;

    // 1초 단위 FPS 집계
    m_fpsTimer += m_deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsTimer >= 1.0f)
    {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsTimer;
        m_fpsTimer = 0.0f;
        m_fpsFrameCount = 0;
    }
}
