#include "Engine/WaveParticles.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265f;
    constexpr int   kRingParticles = 16;
    constexpr float kMinHeight = 0.005f;   // 이보다 낮은 고리는 보이지 않으므로 지운다
    constexpr float kNoShore = -1000.0f;

    float Spacing(const WaveParticles::Particle& p)
    {
        return (std::max)(p.traveled * p.spread, p.radius * 0.05f);
    }
}

float WaveParticles::RingHeight(const Particle& particle)
{
    // 반경 r 인 코사인 봉우리를 간격 Δ 로 늘어놓으면 합은 r / Δ 배가 된다
    return particle.amplitude * particle.radius / Spacing(particle);
}

void WaveParticles::Clear()
{
    m_particles.clear();
}

void WaveParticles::SetBounds(float minX, float minZ, float maxX, float maxZ)
{
    m_minX = minX;
    m_minZ = minZ;
    m_maxX = maxX;
    m_maxZ = maxZ;
}

void WaveParticles::SetShore(const std::vector<float>& heights, int size, float originX, float originZ, float worldSize)
{
    if (size <= 1 || worldSize <= 0.0f || heights.size() < static_cast<size_t>(size) * size)
    {
        m_shore.clear();
        m_shoreSize = 0;
        return;
    }

    m_shore.assign(heights.begin(), heights.begin() + static_cast<size_t>(size) * size);
    m_shoreSize = size;
    m_shoreX = originX;
    m_shoreZ = originZ;
    m_shoreWorld = worldSize;
}

float WaveParticles::SampleShore(float x, float z) const
{
    if (m_shoreSize <= 1)
        return kNoShore;

    // 칸 중심이 (i + 0.5) · 칸 크기 에 있다
    const float cell = m_shoreWorld / static_cast<float>(m_shoreSize);
    const float u = (x - m_shoreX) / cell - 0.5f;
    const float v = (z - m_shoreZ) / cell - 0.5f;
    const float last = static_cast<float>(m_shoreSize - 1);

    if (u < 0.0f || v < 0.0f || u > last || v > last)
        return kNoShore;

    const int column = (std::min)(static_cast<int>(u), m_shoreSize - 2);
    const int row = (std::min)(static_cast<int>(v), m_shoreSize - 2);
    const float fu = u - static_cast<float>(column);
    const float fv = v - static_cast<float>(row);

    auto at = [this](int c, int r) { return m_shore[static_cast<size_t>(r) * m_shoreSize + c]; };
    const float bottom = at(column, row) + (at(column + 1, row) - at(column, row)) * fu;
    const float top = at(column, row + 1) + (at(column + 1, row + 1) - at(column, row + 1)) * fu;
    return bottom + (top - bottom) * fv;
}

void WaveParticles::Emit(float x, float z, float radius, float height)
{
    if (height <= 0.0f || SampleShore(x, z) > 0.0f)
        return;

    const float r = (std::clamp)(radius * 1.6f, kMinRadius, kMaxRadius);
    const float spread = 2.0f * kPi / static_cast<float>(kRingParticles);
    const float start = r * 0.5f;

    // 처음 간격에서 고리 높이가 height 가 되도록 입자 하나의 몫을 정한다
    const float amplitude = height * (start * spread) / r;
    const uint32_t ring = m_nextRing++;

    // 점성 감쇠 : 물결이 사라지는 속도는 파수의 제곱(∝ 1 / 반경²)에 비례한다.
    //  빗방울 같은 작은 물결은 금방 사라지고 클릭 물결은 멀리 간다. 작은 고리가 오래 살아 계속 쪼개지면
    //  입자가 수만 개로 불어나 한 단계가 수십 ms 걸린다.
    const float relative = kReferenceRadius / r;
    const float damping = std::pow(m_settings.damping, (std::min)(relative * relative, 12.0f));

    // 새 물방울은 상한의 80% 까지만 받는다. 남은 자리는 이미 퍼지는 고리가 쪼개질 몫이다 (못 쪼개면 고리가 끊겨 보인다)
    if (m_particles.size() + kRingParticles > static_cast<size_t>(m_settings.maxParticles) * 4 / 5)
        return;

    for (int i = 0; i < kRingParticles; ++i)
    {
        const float angle = spread * static_cast<float>(i);
        Particle p{};
        p.dirX = std::cos(angle);
        p.dirZ = std::sin(angle);
        p.x = x + p.dirX * start;
        p.z = z + p.dirZ * start;
        p.amplitude = amplitude;
        p.radius = r;
        p.spread = spread;
        p.traveled = start;
        p.damping = damping;
        p.ring = ring;
        m_particles.push_back(p);
    }
}

void WaveParticles::Step()
{
    Move();
    Subdivide();
    RemoveDead();
    Collide();
}

void WaveParticles::Move()
{
    const float step = m_settings.speed * m_settings.stepSeconds;
    const float cell = m_shoreSize > 1 ? m_shoreWorld / static_cast<float>(m_shoreSize) : 1.0f;

    for (Particle& p : m_particles)
    {
        const float nextX = p.x + p.dirX * step;
        const float nextZ = p.z + p.dirZ * step;

        if (SampleShore(nextX, nextZ) > 0.0f)
        {
            // 땅에 닿았다 : 해안 높이의 경사가 땅 쪽을 가리키므로 그 반대가 물 쪽 법선이다
            float normalX = -(SampleShore(nextX + cell, nextZ) - SampleShore(nextX - cell, nextZ));
            float normalZ = -(SampleShore(nextX, nextZ + cell) - SampleShore(nextX, nextZ - cell));
            float length = std::sqrt(normalX * normalX + normalZ * normalZ);

            // 맵 가장자리 등으로 경사를 못 구하면 온 길로 되돌아간다
            if (!(length > 1.0e-4f) || length > 500.0f)
            {
                normalX = -p.dirX;
                normalZ = -p.dirZ;
                length = 1.0f;
            }
            normalX /= length;
            normalZ /= length;

            const float into = p.dirX * normalX + p.dirZ * normalZ;
            if (into < 0.0f)
            {
                p.dirX -= 2.0f * into * normalX;
                p.dirZ -= 2.0f * into * normalZ;
                const float dirLength = std::sqrt(p.dirX * p.dirX + p.dirZ * p.dirZ);
                p.dirX /= dirLength;
                p.dirZ /= dirLength;
            }
            // 이번 단계는 제자리 (다음 단계에 반사된 방향으로 나간다)
        }
        else
        {
            p.x = nextX;
            p.z = nextZ;
        }

        // 고리가 커지면 간격이 넓어져 높이가 1/R 로 준다. 에너지(높이² · 둘레)가 보존되려면 1/√R 이어야 하므로
        // 세기를 √R 만큼 키워 준다. 거기에 물의 점성 감쇠를 곱한다.
        const float before = p.traveled;
        p.traveled += step;
        p.amplitude *= p.damping * std::sqrt(p.traveled / before);
    }
}

void WaveParticles::Subdivide()
{
    const size_t count = m_particles.size();
    for (size_t i = 0; i < count; ++i)
    {
        const Particle& source = m_particles[i];
        if (source.traveled * source.spread <= source.radius * 0.5f)
            continue;

        if (static_cast<int>(m_particles.size()) + 2 > m_settings.maxParticles)
            break;

        // 벌어진 각을 셋으로 나눠 가운데는 그대로, 양옆은 ±각/3 만큼 돌린다
        Particle center = source;
        center.spread /= 3.0f;
        center.amplitude /= 3.0f;

        const float turn = center.spread;
        const float originX = center.x - center.dirX * center.traveled;   // 고리가 퍼지기 시작한 점
        const float originZ = center.z - center.dirZ * center.traveled;

        m_particles[i] = center;

        for (int side = -1; side <= 1; side += 2)
        {
            const float c = std::cos(turn);
            const float s = std::sin(turn) * static_cast<float>(side);

            Particle q = center;
            q.dirX = center.dirX * c - center.dirZ * s;
            q.dirZ = center.dirX * s + center.dirZ * c;
            q.x = originX + q.dirX * center.traveled;
            q.z = originZ + q.dirZ * center.traveled;
            m_particles.push_back(q);
        }
    }
}

void WaveParticles::RemoveDead()
{
    for (size_t i = 0; i < m_particles.size();)
    {
        const Particle& p = m_particles[i];
        const bool dead = RingHeight(p) < kMinHeight ||
                          p.x < m_minX || p.z < m_minZ || p.x > m_maxX || p.z > m_maxZ ||
                          SampleShore(p.x, p.z) > 0.0f;   // 해안 맵이 새로 구워져 땅 속에 남은 입자

        if (dead)
        {
            m_particles[i] = m_particles.back();
            m_particles.pop_back();
        }
        else
        {
            ++i;
        }
    }
}

// -------------------------------------------------------------
// 부딪힘 : 마주 오는 물결끼리 힘을 깎아 낸다
//
//  입자 i, j 가 다른 고리이고 서로 반대쪽으로 가며(opposition = -dot(방향)) 반경 안에 겹치면
//     깎을 높이 = f · w(거리) · opposition · min(고리 높이 i, 고리 높이 j)
//  를 둘 다에서 뺀다. 같은 양을 빼므로 두 물결 높이의 "차이" 는 그대로 남는다.
//     3 과 1 이 정면으로 만나면 → 1 은 0 이 되고, 3 은 2 가 되어 계속 나아간다.
//  직각으로 엇갈리면 opposition = 0 이라 서로 그대로 통과한다.
//
//  고리 i 의 한 입자는 상대 고리의 입자 여러 개와 겹친다(간격 Δj 마다 하나). 그래서 j 한 개의 몫에
//  Δj / R 을 곱해 "상대 고리 전체" 와 부딪힌 양이 입자 수와 무관하게 되도록 한다.
//  f 는 정면으로 한 번 스쳐 지나가는 동안 합이 cancelDepth 가 되게 정한다 (겹치는 단계 수 ≈ R / (2 · 속도 · dt)).
// -------------------------------------------------------------
void WaveParticles::Collide()
{
    m_collidingCount = 0;
    m_cancelledHeight = 0.0f;

    const int count = static_cast<int>(m_particles.size());
    if (count < 2)
        return;

    const float cell = kMaxRadius;
    const int columns = (std::max)(1, static_cast<int>((m_maxX - m_minX) / cell) + 1);
    const int rows = (std::max)(1, static_cast<int>((m_maxZ - m_minZ) / cell) + 1);
    if (static_cast<long long>(columns) * rows > 4000000)
        return;   // 영역이 설정되지 않았다

    m_cellHead.assign(static_cast<size_t>(columns) * rows, -1);
    m_cellNext.resize(static_cast<size_t>(count));
    m_cancel.assign(static_cast<size_t>(count), 0.0f);

    auto cellOf = [&](const Particle& p, int& outColumn, int& outRow)
    {
        outColumn = (std::clamp)(static_cast<int>((p.x - m_minX) / cell), 0, columns - 1);
        outRow = (std::clamp)(static_cast<int>((p.z - m_minZ) / cell), 0, rows - 1);
    };

    for (int i = 0; i < count; ++i)
    {
        m_cellNext[static_cast<size_t>(i)] = -1;
        if (RingHeight(m_particles[static_cast<size_t>(i)]) < kCollideMinHeight)
            continue;   // 격자에 넣지 않으면 누구와도 짝이 되지 않는다

        int column = 0;
        int row = 0;
        cellOf(m_particles[static_cast<size_t>(i)], column, row);
        int& head = m_cellHead[static_cast<size_t>(row) * columns + column];
        m_cellNext[static_cast<size_t>(i)] = head;
        head = i;
    }

    const float closingPerStep = 2.0f * m_settings.speed * m_settings.stepSeconds;

    for (int i = 0; i < count; ++i)
    {
        const Particle& a = m_particles[static_cast<size_t>(i)];
        if (RingHeight(a) < kCollideMinHeight)
            continue;

        int column = 0;
        int row = 0;
        cellOf(a, column, row);

        for (int dr = -1; dr <= 1; ++dr)
        {
            const int r = row + dr;
            if (r < 0 || r >= rows)
                continue;

            for (int dc = -1; dc <= 1; ++dc)
            {
                const int c = column + dc;
                if (c < 0 || c >= columns)
                    continue;

                for (int j = m_cellHead[static_cast<size_t>(r) * columns + c]; j >= 0; j = m_cellNext[static_cast<size_t>(j)])
                {
                    if (j <= i)
                        continue;   // 한 쌍은 한 번만

                    const Particle& b = m_particles[static_cast<size_t>(j)];
                    if (a.ring == b.ring)
                        continue;

                    const float opposition = -(a.dirX * b.dirX + a.dirZ * b.dirZ);
                    if (opposition < 0.1f)
                        continue;

                    const float reach = 0.5f * (a.radius + b.radius);
                    const float dx = b.x - a.x;
                    const float dz = b.z - a.z;
                    const float distanceSq = dx * dx + dz * dz;
                    if (distanceSq >= reach * reach)
                        continue;

                    const float weight = 0.5f + 0.5f * std::cos(kPi * std::sqrt(distanceSq) / reach);
                    const float rate = (std::min)(1.0f, m_settings.cancelDepth * closingPerStep / reach);
                    const float amount = rate * weight * opposition * (std::min)(RingHeight(a), RingHeight(b));

                    m_cancel[static_cast<size_t>(i)] += amount * Spacing(b) / reach;
                    m_cancel[static_cast<size_t>(j)] += amount * Spacing(a) / reach;
                }
            }
        }
    }

    // 모두 계산한 뒤 한꺼번에 깎는다. (먼저 깎인 입자가 뒤 계산에 영향을 주지 않도록)
    for (int i = 0; i < count; ++i)
    {
        const float cancel = m_cancel[static_cast<size_t>(i)];
        if (cancel <= 0.0f)
            continue;

        Particle& p = m_particles[static_cast<size_t>(i)];
        const float height = RingHeight(p);
        p.amplitude *= (std::max)(0.0f, 1.0f - cancel / height);

        ++m_collidingCount;
        m_cancelledHeight += (std::min)(cancel, height);
    }
}
