#include "Core/stdafx.h"
#include "Engine/WaterfallRenderer.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Engine/GameObject.h"
#include "Engine/GroundProvider.h"
#include "Engine/Scene.h"
#include "Engine/WaterRenderer.h"
#include "Graphics/Vertex.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z); }
    XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b) { return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z); }
    XMFLOAT3 Scale(const XMFLOAT3& a, float s) { return XMFLOAT3(a.x * s, a.y * s, a.z * s); }
    float Dot(const XMFLOAT3& a, const XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    float Length(const XMFLOAT3& a) { return std::sqrt(Dot(a, a)); }

    XMFLOAT3 Cross(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return XMFLOAT3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }

    XMFLOAT3 Normalize(const XMFLOAT3& a, const XMFLOAT3& fallback = XMFLOAT3(0.0f, 0.0f, 1.0f))
    {
        const float length = Length(a);
        return length > 1.0e-5f ? Scale(a, 1.0f / length) : fallback;
    }
}

void WaterfallRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    m_needsRebuild = true;
}

void WaterfallRenderer::OnDestroy()
{
    m_mesh.Release();
    m_nodes.clear();
    m_splashPoints.clear();
    m_mistPoints.clear();
    m_providers.clear();
    m_water = nullptr;
}

void WaterfallRenderer::SetSearchArea(float centerX, float centerZ, float radius)
{
    m_centerX = centerX;
    m_centerZ = centerZ;
    m_radius = (std::max)(40.0f, radius);
    m_needsRebuild = true;
}

void WaterfallRenderer::Update()
{
    if (!m_graphics)
        return;

    const float deltaTime = (std::min)(TimeManager::Get().GetDeltaTime(), 0.25f);
    m_time += deltaTime;

    // 지형은 씬이 시작되고 몇 프레임 뒤에야 준비된다. 물길이 없으면 자주, 있으면 가끔 살핀다.
    m_checkTimer += deltaTime;
    if (m_checkTimer >= (m_nodes.empty() ? 0.35f : 1.0f))
    {
        m_checkTimer = 0.0f;

        // 지형이 바뀌면(N 새 지형 · 브러시 · 침식) 발원지 높이가 달라진다
        if (!m_needsRebuild && !m_nodes.empty())
        {
            float height = 0.0f;
            const XMFLOAT3& source = m_nodes.front().position;
            if (SampleGround(source.x, source.z, height) && std::fabs(height - m_sourceHeight) > 0.5f)
                m_needsRebuild = true;
        }

        if (m_needsRebuild || m_nodes.empty())
            Rebuild();
    }

    SpawnSplashes(deltaTime);
}

void WaterfallRenderer::RenderTransparent()
{
    if (!m_enabled || !m_graphics || !m_mesh.IsValid())
        return;

    // 반사 패스에서는 그리지 않는다. 깊이 복사본이 그 패스의 것이 아니다.
    if (m_graphics->GetRenderPass() != Graphics::RenderPass::Main)
        return;

    Graphics::WaterfallDrawParams params;
    params.time = m_time;
    params.debug = m_debug;

    m_graphics->DrawWaterfall(m_mesh, XMMatrixIdentity(), params);
}

// -------------------------------------------------------------
// 지면 질의
// -------------------------------------------------------------
void WaterfallRenderer::CollectProviders()
{
    m_providers.clear();
    m_water = nullptr;

    GameObject* owner = GetOwner();
    Scene* scene = owner ? owner->GetScene() : nullptr;
    if (!scene)
        return;

    for (const auto& object : scene->GetGameObjects())
    {
        if (!object || object->IsPendingDestroy() || !object->IsActive())
            continue;

        for (const auto& entry : object->GetComponentMap())
        {
            for (const auto& component : entry.second)
            {
                if (const IGroundProvider* provider = dynamic_cast<const IGroundProvider*>(component.get()))
                    m_providers.push_back(provider);

                // 물결(S89)을 일으키려면 수면이 필요하다. 수위도 그쪽을 따라간다.
                if (WaterRenderer* water = dynamic_cast<WaterRenderer*>(component.get()))
                {
                    m_water = water;
                    m_waterLevel = water->GetWaterLevel();
                }
            }
        }
    }
}

bool WaterfallRenderer::SampleGround(float x, float z, float& outHeight) const
{
    bool found = false;
    float best = 0.0f;

    for (const IGroundProvider* provider : m_providers)
    {
        float height = 0.0f;
        if (!provider->TryGetGroundHeight(x, z, height))
            continue;

        best = found ? (std::max)(best, height) : height;
        found = true;
    }

    outHeight = best;
    return found;
}

// 높이 맵의 기울기로 지면 법선을 구한다
XMFLOAT3 WaterfallRenderer::GroundNormal(float x, float z) const
{
    constexpr float kStep = 2.0f;

    float left = 0.0f, right = 0.0f, back = 0.0f, front = 0.0f;
    if (!SampleGround(x - kStep, z, left) || !SampleGround(x + kStep, z, right) ||
        !SampleGround(x, z - kStep, back) || !SampleGround(x, z + kStep, front))
        return XMFLOAT3(0.0f, 1.0f, 0.0f);

    const float dx = (right - left) / (2.0f * kStep);
    const float dz = (front - back) / (2.0f * kStep);
    return Normalize(XMFLOAT3(-dx, 1.0f, -dz), XMFLOAT3(0.0f, 1.0f, 0.0f));
}

// -------------------------------------------------------------
// 물길 찾기
//  성기게 훑어 "물 위로 높고 경사가 급한" 후보를 모으고, 각 후보에서 실제로 물을 흘려 본다.
//  낙차(특히 공중으로 떨어지는 높이)가 가장 크고 바다까지 닿는 물길을 고른다.
// -------------------------------------------------------------
bool WaterfallRenderer::FindSource(XMFLOAT3& outSource) const
{
    struct Candidate
    {
        XMFLOAT3 position;
        float    score;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(512);

    constexpr float kScanStep = 12.0f;
    for (float z = m_centerZ - m_radius; z <= m_centerZ + m_radius; z += kScanStep)
    {
        for (float x = m_centerX - m_radius; x <= m_centerX + m_radius; x += kScanStep)
        {
            float height = 0.0f;
            if (!SampleGround(x, z, height))
                continue;

            // 수면에서 충분히 높아야 낙차가 나온다
            if (height < m_waterLevel + 25.0f)
                continue;

            const XMFLOAT3 normal = GroundNormal(x, z);
            const float steep = 1.0f - normal.y;                 // 0 평지 ~ 1 절벽

            const float dx = x - m_centerX;
            const float dz = z - m_centerZ;
            const float distance = std::sqrt(dx * dx + dz * dz);

            const float score = (height - m_waterLevel) * 0.30f + steep * 220.0f - distance * 0.05f;
            candidates.push_back({ XMFLOAT3(x, height, z), score });
        }
    }

    if (candidates.empty())
        return false;

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

    // 비슷한 자리를 여러 번 시험하지 않도록 서로 떨어진 후보만 남긴다
    std::vector<XMFLOAT3> picked;
    for (const Candidate& candidate : candidates)
    {
        if (picked.size() >= kMaxCandidates)
            break;

        bool tooClose = false;
        for (const XMFLOAT3& other : picked)
        {
            const float dx = candidate.position.x - other.x;
            const float dz = candidate.position.z - other.z;
            if (dx * dx + dz * dz < 50.0f * 50.0f)
            {
                tooClose = true;
                break;
            }
        }

        if (!tooClose)
            picked.push_back(candidate.position);
    }

    // 후보마다 실제로 물을 흘려 보고 점수를 매긴다
    std::vector<std::pair<float, XMFLOAT3>> ranked;
    for (const XMFLOAT3& source : picked)
    {
        std::vector<Node> path;
        if (!TracePath(source, path) || path.size() < 8)
            continue;

        float fall = 0.0f;
        for (size_t i = 1; i < path.size(); ++i)
        {
            if (path[i].airborne)
                fall += (std::max)(0.0f, path[i - 1].position.y - path[i].position.y);
        }

        const float drop = source.y - path.back().position.y;
        const bool reachesSea = path.back().position.y <= m_waterLevel + 1.5f;

        // 공중 낙차가 클수록 폭포답다. 바다까지 닿으면 더 좋다(물결을 일으킨다).
        ranked.push_back({ fall * 12.0f + drop * 0.5f + (reachesSea ? 40.0f : 0.0f), source });
    }

    if (ranked.empty())
        return false;

    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    outSource = ranked[static_cast<size_t>(m_choice) % ranked.size()].second;
    return true;
}

// -------------------------------------------------------------
// 물 한 덩이를 중력으로 굴린다
//  지면을 탈 때 : 중력을 경사면에 투영한 만큼 빨라지고 마찰로 느려진다
//  지면이 발밑에서 꺼지면 : 공중으로 떨어진다 (이 구간이 폭포다)
//  다시 닿으면 : 튀며 속도를 잃고 계속 흐른다
// -------------------------------------------------------------
bool WaterfallRenderer::TracePath(const XMFLOAT3& source, std::vector<Node>& outNodes) const
{
    outNodes.clear();

    XMFLOAT3 position = source;
    if (!SampleGround(position.x, position.z, position.y))
        return false;

    // 샘에서 솟은 물은 이미 조금 흐르고 있다. 정지 상태로 두면 완만한 곳에서 출발하지 못한다.
    const XMFLOAT3 startNormal = GroundNormal(position.x, position.z);
    XMFLOAT3 velocity = Scale(Normalize(XMFLOAT3(-startNormal.x, 0.0f, -startNormal.z), XMFLOAT3(0.0f, 0.0f, 1.0f)), 2.0f);
    bool airborne = false;
    float distance = 0.0f;
    float fallen = 0.0f;
    int slowSteps = 0;

    for (int step = 0; step < kMaxSteps; ++step)
    {
        const XMFLOAT3 normal = GroundNormal(position.x, position.z);

        if (!airborne)
        {
            // 중력에서 지면이 받쳐 주는 몫을 빼면 경사면을 따라 미끄러지는 힘만 남는다
            const XMFLOAT3 gravity{ 0.0f, -kGravity, 0.0f };
            const XMFLOAT3 slide = Sub(gravity, Scale(normal, Dot(gravity, normal)));

            velocity = Add(velocity, Scale(slide, kStepSeconds));
            velocity = Scale(velocity, (std::max)(0.0f, 1.0f - kFriction * kStepSeconds));

            // 거의 멈췄다면 가장 낮은 쪽으로 살짝 밀어 준다 (평지에서 고이지 않게)
            if (Length(velocity) < 0.6f)
                velocity = Add(velocity, Scale(XMFLOAT3(-normal.x, 0.0f, -normal.z), 4.0f * kStepSeconds));
        }
        else
        {
            velocity.y -= kGravity * kStepSeconds;
        }

        const float speed = Length(velocity);
        if (speed > kMaxSpeed)
            velocity = Scale(velocity, kMaxSpeed / speed);

        XMFLOAT3 next = Add(position, Scale(velocity, kStepSeconds));

        float ground = 0.0f;
        if (!SampleGround(next.x, next.z, ground))
            break;   // 지형 밖

        if (!airborne)
        {
            if (next.y - ground > kAirGap)
            {
                // 지면이 물보다 빨리 꺼졌다 : 여기서부터 공중으로 떨어진다
                airborne = true;
                fallen = 0.0f;
            }
            else
            {
                next.y = ground;

                // 새 자리의 경사면을 따라가도록 속도에서 지면을 파고드는 몫을 뺀다
                const XMFLOAT3 nextNormal = GroundNormal(next.x, next.z);
                const float into = Dot(velocity, nextNormal);
                if (into < 0.0f)
                    velocity = Sub(velocity, Scale(nextNormal, into));
            }
        }
        else if (next.y <= ground)
        {
            // 바닥을 때렸다 : 수직 속도를 잃고 다시 지면을 탄다
            next.y = ground;
            airborne = false;
            fallen = 0.0f;
            velocity.y = 0.0f;
            velocity = Scale(velocity, 0.45f);
        }

        const XMFLOAT3 delta = Sub(next, position);
        const float moved = Length(delta);
        distance += moved;
        if (airborne)
            fallen += moved;

        Node node;
        node.position = next;
        node.flow = Normalize(delta, XMFLOAT3(0.0f, -1.0f, 0.0f));
        node.speed = Length(velocity);
        node.distance = distance;
        node.fallen = fallen;
        node.airborne = airborne;

        node.width = kBaseWidth * (0.75f + 0.5f * std::sqrt((std::max)(distance, 0.0f) / 40.0f));
        if (airborne)
            node.width *= 1.0f + fallen * 0.02f;    // 떨어지며 조금 퍼진다
        node.width = (std::min)(node.width, kMaxWidth);

        outNodes.push_back(node);
        position = next;

        if (position.y <= m_waterLevel + 0.25f)
            break;   // 바다 · 호수에 닿았다

        // 평지에 고여 더 가지 못하면 끝낸다
        slowSteps = (!airborne && moved < 0.06f) ? slowSteps + 1 : 0;
        if (slowSteps > 12)
            break;
    }

    if (outNodes.size() < 8)
        return false;

    // 좌우 방향은 이전 것을 이어받아야 리본이 꼬이지 않는다.
    //  지면을 탈 때는 경사면 위에서, 공중에서는 수평으로 눕힌다.
    //  똑바로 떨어지는 구간은 진행 방향과 위쪽이 나란해 좌우를 정할 수 없으므로 직전 값을 그대로 쓴다.
    XMFLOAT3 side = Normalize(Cross(outNodes.front().flow, XMFLOAT3(0.0f, 1.0f, 0.0f)), XMFLOAT3(1.0f, 0.0f, 0.0f));
    for (Node& node : outNodes)
    {
        const XMFLOAT3 up = node.airborne ? XMFLOAT3(0.0f, 1.0f, 0.0f) : GroundNormal(node.position.x, node.position.z);
        XMFLOAT3 candidate = Cross(node.flow, up);

        if (Length(candidate) > 0.2f)
        {
            candidate = Normalize(candidate, side);
            if (Dot(candidate, side) < 0.0f)
                candidate = Scale(candidate, -1.0f);
            side = candidate;
        }

        node.side = side;
    }

    return true;
}

// 낙차가 가장 큰 구간을 옆에서 보는 자리
bool WaterfallRenderer::GetViewpoint(XMFLOAT3& outEye, XMFLOAT3& outTarget) const
{
    if (m_nodes.size() < 2)
        return false;

    // 한 걸음에 가장 많이 떨어진 곳 = 폭포의 한가운데
    size_t best = 0;
    float bestDrop = -1.0f;
    for (size_t i = 1; i < m_nodes.size(); ++i)
    {
        const float drop = m_nodes[i - 1].position.y - m_nodes[i].position.y;
        if (drop > bestDrop)
        {
            bestDrop = drop;
            best = i;
        }
    }

    const Node& node = m_nodes[best];
    outTarget = node.position;

    // 물이 흘러가는 쪽(골짜기)으로 나가서 되돌아본다. 산비탈 쪽에 서면 화면이 지형으로 막힌다.
    //  아래쪽 마디들의 평균 방향을 쓰면 한 마디의 흔들림에 덜 휘둘린다.
    XMFLOAT3 downstream{ 0.0f, 0.0f, 0.0f };
    for (size_t i = best; i < m_nodes.size() && i < best + 20; ++i)
        downstream = Add(downstream, XMFLOAT3(m_nodes[i].flow.x, 0.0f, m_nodes[i].flow.z));

    downstream = Normalize(downstream, XMFLOAT3(m_nodes[best].side.x, 0.0f, m_nodes[best].side.z));

    // 정면으로만 보면 물줄기가 한 줄로 겹쳐 보이므로 옆으로도 조금 비킨다
    const XMFLOAT3 side = Normalize(Cross(downstream, XMFLOAT3(0.0f, 1.0f, 0.0f)), XMFLOAT3(1.0f, 0.0f, 0.0f));

    // 낙차가 클수록 멀리서 봐야 폭포가 화면에 다 들어온다
    const float distance = (std::clamp)(m_fallHeight * 2.4f + 40.0f, 60.0f, 170.0f);

    outEye = XMFLOAT3(node.position.x + downstream.x * distance + side.x * distance * 0.45f,
                      node.position.y + distance * 0.35f,
                      node.position.z + downstream.z * distance + side.z * distance * 0.45f);

    // 산속에 파묻히지 않게 : 그 자리의 지면보다 확실히 위로 올린다
    float ground = 0.0f;
    if (SampleGround(outEye.x, outEye.z, ground))
        outEye.y = (std::max)(outEye.y, ground + 14.0f);

    return true;
}

void WaterfallRenderer::NextSite()
{
    ++m_choice;
    m_needsRebuild = true;
    m_checkTimer = 0.0f;
    Rebuild();
}

void WaterfallRenderer::Rebuild()
{
    m_needsRebuild = false;
    m_mesh.Release();
    m_nodes.clear();
    m_splashPoints.clear();
    m_mistPoints.clear();
    m_dropHeight = 0.0f;
    m_fallHeight = 0.0f;
    m_pathLength = 0.0f;

    CollectProviders();
    if (m_providers.empty() || !m_graphics || !m_graphics->GetDevice())
    {
        m_needsRebuild = true;
        return;
    }

    XMFLOAT3 source{};
    if (!FindSource(source) || !TracePath(source, m_nodes))
    {
        m_nodes.clear();
        m_needsRebuild = true;
        return;
    }

    m_sourceHeight = m_nodes.front().position.y;
    m_dropHeight = m_nodes.front().position.y - m_nodes.back().position.y;
    m_pathLength = m_nodes.back().distance;

    for (size_t i = 1; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].airborne)
            m_fallHeight += (std::max)(0.0f, m_nodes[i - 1].position.y - m_nodes[i].position.y);

        // 공중에서 떨어지다 바닥을 때린 자리
        const bool landed = m_nodes[i - 1].airborne && !m_nodes[i].airborne;
        const bool last = (i + 1 == m_nodes.size());

        // 그 자리가 바다면 물결을 일으킨다 (S89)
        if ((landed || last) && m_nodes[i].position.y <= m_waterLevel + 1.0f)
            m_splashPoints.push_back(m_nodes[i].position);

        // 세게 떨어진 자리에서는 물보라가 피어오른다
        if ((landed && m_nodes[i - 1].fallen > 3.0f) || last)
            m_mistPoints.push_back(m_nodes[i].position);
    }

    BuildMesh();

    dxutil::DebugLog(L"[Waterfall] 물길 : 발원지 (%.0f, %.0f, %.0f)  낙차 %.1f m (공중 %.1f m)  길이 %.0f m  점 %d 개",
                     source.x, m_sourceHeight, source.z, m_dropHeight, m_fallHeight, m_pathLength,
                     static_cast<int>(m_nodes.size()));
}

// 지나온 점마다 좌우로 정점을 놓아 띠를 만든다
void WaterfallRenderer::BuildMesh()
{
    if (m_nodes.size() < 2 || !m_graphics || !m_graphics->GetDevice())
        return;

    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(m_nodes.size() * 2);
    indices.reserve((m_nodes.size() - 1) * 6);

    for (const Node& node : m_nodes)
    {
        // 판의 법선 : 좌우 × 진행 방향. 지면을 탈 때는 지면 법선과, 공중에서는 수평 방향과 같아진다.
        XMFLOAT3 normal = Normalize(Cross(node.side, node.flow), XMFLOAT3(0.0f, 1.0f, 0.0f));
        if (normal.y < 0.0f)
            normal = Scale(normal, -1.0f);

        const XMFLOAT3 lift = Scale(normal, kLift);
        const XMFLOAT3 half = Scale(node.side, node.width * 0.5f);
        const float v = node.distance / kTextureLength;
        const XMFLOAT4 flow(node.speed, node.airborne ? 1.0f : 0.0f, node.fallen, node.distance);

        TerrainVertex left{};
        left.position = Add(Sub(node.position, half), lift);
        left.normal = normal;
        left.uv = XMFLOAT2(0.0f, v);
        left.morphTargets = flow;
        left.biome = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

        TerrainVertex right = left;
        right.position = Add(Add(node.position, half), lift);
        right.uv = XMFLOAT2(1.0f, v);

        vertices.push_back(left);
        vertices.push_back(right);
    }

    for (uint32_t i = 0; i + 1 < static_cast<uint32_t>(m_nodes.size()); ++i)
    {
        const uint32_t base = i * 2;
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base);
        indices.push_back(base + 3);
        indices.push_back(base + 1);
    }

    // ---- 물보라 : 떨어지는 자리마다 빌보드 사각형 여러 장 ----
    //  네 정점을 같은 점에 두고 모서리 부호만 실어 보낸다. 실제로 펴는 일은 카메라를 아는 정점 셰이더가 한다.
    //  씨앗을 다르게 주어 피어올랐다 사라지는 한살이가 서로 어긋나게 한다.
    std::uniform_real_distribution<float> spread(-4.5f, 4.5f);
    std::uniform_real_distribution<float> puffSize(2.5f, 5.5f);
    std::uniform_real_distribution<float> rise(5.0f, 14.0f);
    std::uniform_real_distribution<float> rate(0.10f, 0.22f);
    std::uniform_real_distribution<float> seed(0.0f, 1.0f);

    const XMFLOAT2 corners[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
    const XMFLOAT2 uvs[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

    for (const XMFLOAT3& point : m_mistPoints)
    {
        for (int puff = 0; puff < kPuffsPerPoint; ++puff)
        {
            const XMFLOAT3 center(point.x + spread(m_rng), point.y + 1.0f, point.z + spread(m_rng));
            const XMFLOAT4 life(puffSize(m_rng), rise(m_rng), rate(m_rng), seed(m_rng));
            const uint32_t base = static_cast<uint32_t>(vertices.size());

            for (int corner = 0; corner < 4; ++corner)
            {
                TerrainVertex vertex{};
                vertex.position = center;
                vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
                vertex.uv = uvs[corner];
                vertex.morphTargets = life;
                vertex.biome = XMFLOAT4(1.0f, corners[corner].x, corners[corner].y, 0.0f);
                vertices.push_back(vertex);
            }

            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    m_mesh.Create(m_graphics->GetDevice(),
                  vertices.data(), static_cast<UINT>(vertices.size()), sizeof(TerrainVertex),
                  indices.data(), static_cast<UINT>(indices.size()));
}

// 떨어지는 자리에 물방울을 떨어뜨려 물결(S89)을 일으킨다
void WaterfallRenderer::SpawnSplashes(float deltaTime)
{
    if (!m_enabled || !m_water || m_splashPoints.empty())
    {
        m_splashTimer = 0.0f;
        return;
    }

    m_splashTimer += deltaTime;

    std::uniform_real_distribution<float> jitter(-1.8f, 1.8f);
    std::uniform_real_distribution<float> radius(1.6f, 2.6f);
    std::uniform_real_distribution<float> strength(0.45f, 0.85f);

    while (m_splashTimer >= kSplashInterval)
    {
        m_splashTimer -= kSplashInterval;

        for (const XMFLOAT3& point : m_splashPoints)
            m_water->AddSplash(point.x + jitter(m_rng), point.z + jitter(m_rng), radius(m_rng), strength(m_rng));
    }
}

void WaterfallRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["centerX"] = json::Value(m_centerX);
    out["centerZ"] = json::Value(m_centerZ);
    out["radius"]  = json::Value(m_radius);
    out["level"]   = json::Value(m_waterLevel);
    out["enabled"] = json::Value(m_enabled);
}

void WaterfallRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);
    if (const json::Value* value = in.Find("centerX")) m_centerX = value->AsFloat(m_centerX);
    if (const json::Value* value = in.Find("centerZ")) m_centerZ = value->AsFloat(m_centerZ);
    if (const json::Value* value = in.Find("radius"))  m_radius  = value->AsFloat(m_radius);
    if (const json::Value* value = in.Find("level"))   m_waterLevel = value->AsFloat(m_waterLevel);
    if (const json::Value* value = in.Find("enabled")) m_enabled = value->AsBool(m_enabled);
    m_needsRebuild = true;
}
