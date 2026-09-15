#pragma once
#include <cstdint>
#include <vector>

// =============================================================
// WaveParticles (S89)
//  물결 하나를 "퍼져 나가는 고리 위의 입자들" 로 표현한다. (Yuksel 2007, Wave Particles)
//  D3D 를 쓰지 않는 순수 CPU 계산이라 앱 밖에서 따로 돌려 검증할 수 있다.
//
//  격자 파동 방정식(S79)은 선형이라 마주 오는 두 물결이 서로를 그대로 통과한다.
//  격자에는 높이만 있고 "이 물결이 어느 쪽으로 가는지" 가 없어서 부딪힘을 계산할 자리가 없다.
//  입자는 방향과 세기를 직접 들고 있으므로 만났을 때 서로 깎아 낼 수 있다.
//
//  입자 하나
//   - 위치, 진행 방향, 반경(물결 폭)
//   - 세기(amplitude) : 이 입자가 높이에 더하는 몫. 고리 높이 = 세기 · 반경 / 이웃 간격
//   - 벌어진 각(spread) : 옆 입자와의 각도. 멀리 갈수록 간격 = 이동 거리 · 벌어진 각 이 넓어진다
//   - 고리 번호 : 같은 물방울에서 나온 입자끼리는 부딪히지 않는다
//
//  한 단계
//   1) 이동    : 해안 높이 맵에서 땅에 닿으면 경사 방향으로 반사
//   2) 쪼개기  : 간격이 반경의 절반을 넘으면 입자를 셋으로 (세기 1/3, 각 1/3) → 고리가 끊기지 않는다
//   3) 정리    : 너무 약하거나 영역 밖이면 지운다
//   4) 부딪힘  : 다른 고리의 마주 오는 입자와 겹치면 둘 다 "약한 쪽 높이" 만큼 깎는다
//                → 약한 물결은 사라지고, 강한 물결은 차이만큼 남아 계속 나아간다
// =============================================================
class WaveParticles
{
public:
    struct Particle
    {
        float    x;
        float    z;
        float    dirX;
        float    dirZ;
        float    amplitude;
        float    radius;
        float    spread;     // 옆 입자와 벌어진 각 (라디안)
        float    traveled;   // 퍼지기 시작한 점에서 온 거리
        float    damping;    // 단계마다 곱하는 감쇠 (짧은 물결일수록 작다)
        uint32_t ring;
    };

    struct Settings
    {
        float speed = 10.0f;               // m/s
        float stepSeconds = 1.0f / 60.0f;
        float damping = 0.996f;            // 반경 kReferenceRadius 인 물결이 단계마다 곱하는 값 (초당 약 0.79 배)
        float cancelDepth = 4.0f;          // 정면으로 한 번 스쳐 지나갈 때 약한 쪽이 e^-(0.93 · 이 값) 까지 줄어든다
        int   maxParticles = 60000;
    };

    void Clear();

    // 반경 radius(m), 고리 높이 height 인 물방울 하나
    void Emit(float x, float z, float radius, float height);
    void Step();

    // 이 사각형 밖으로 나간 입자는 지운다
    void SetBounds(float minX, float minZ, float maxX, float maxZ);

    // 해안 높이 맵 : size x size, "땅 높이 - 수위" (양수 = 땅). 행은 +Z 로 커진다
    void SetShore(const std::vector<float>& heights, int size, float originX, float originZ, float worldSize);

    const std::vector<Particle>& GetParticles() const { return m_particles; }
    Settings&       GetSettings() { return m_settings; }
    const Settings& GetSettings() const { return m_settings; }

    // 이 입자가 속한 고리의 높이 (이웃과 겹쳐 더해진 값)
    static float RingHeight(const Particle& particle);

    int   GetCollidingCount() const { return m_collidingCount; }   // 이번 단계에 깎인 입자 수
    float GetCancelledHeight() const { return m_cancelledHeight; }

    static constexpr float kMinRadius = 1.2f;
    static constexpr float kMaxRadius = 5.0f;
    static constexpr float kReferenceRadius = 4.0f;     // 클릭 물결의 반경. 감쇠의 기준

    // 이보다 낮은 고리는 부딪힘 검사에서 뺀다. 깎아도 눈에 보이지 않고, 빗방울 입자가 대부분 여기에 속한다
    static constexpr float kCollideMinHeight = 0.02f;

private:
    void Move();
    void Subdivide();
    void RemoveDead();
    void Collide();

    float SampleShore(float x, float z) const;   // 맵 밖이거나 맵이 없으면 깊은 물(-1000)

    Settings m_settings;
    std::vector<Particle> m_particles;
    uint32_t m_nextRing = 1;

    float m_minX = -1.0e6f;
    float m_minZ = -1.0e6f;
    float m_maxX = 1.0e6f;
    float m_maxZ = 1.0e6f;

    std::vector<float> m_shore;
    int   m_shoreSize = 0;
    float m_shoreX = 0.0f;
    float m_shoreZ = 0.0f;
    float m_shoreWorld = 1.0f;

    // 부딪힘 검사용 격자 (칸 크기 = 최대 반경). 칸마다 연결 리스트
    std::vector<int>   m_cellHead;
    std::vector<int>   m_cellNext;
    std::vector<float> m_cancel;

    int   m_collidingCount = 0;
    float m_cancelledHeight = 0.0f;
};
