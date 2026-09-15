#include "Core/stdafx.h"
#include "Engine/WaterRenderer.h"
#include "Core/Graphics.h"
#include "Core/TimeManager.h"
#include "Graphics/Vertex.h"
#include "Engine/GameObject.h"
#include "Engine/Scene.h"
#include "Engine/GroundRaycast.h"
#include "Input/InputManager.h"
#include "Engine/GroundProvider.h"
#include "Editor/EditorStyle.h"

#include <cstdio>

#include <cmath>

using namespace DirectX;

void WaterRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    if (!m_graphics || !m_graphics->GetDevice())
        return;

    // 바다 격자와 파도 (S85)
    BuildOceanGrid();
    m_ocean.Initialize(m_graphics->GetDevice());

    // 카메라 주변 384 x 384 영역을 512 x 512 텍셀로 시뮬레이션한다 (텍셀 하나 0.75).
    m_ripples.Initialize(m_graphics->GetDevice(), 512, 384.0f);

    // 해안 높이 맵 (S81, S84) : 실수 128 x 128. 값 = 땅 높이 - 수위. 처음엔 전부 깊은 물.
    //  땅/물 한 비트만 담으면 벽 판정밖에 못 한다. 높이를 담아 두면
    //   - 선형 보간한 값이 0 이 되는 자리가 곧 해안선이라 계단 없이 매끄럽고
    //   - 진폭 창에서 지형을 높이별 색 · 등고선으로, 물은 깊이별로 칠할 수 있다.
    D3D11_TEXTURE2D_DESC shore = {};
    shore.Width            = kShoreSize;
    shore.Height           = kShoreSize;
    shore.MipLevels        = 1;
    shore.ArraySize        = 1;
    shore.Format           = DXGI_FORMAT_R32_FLOAT;
    shore.SampleDesc.Count = 1;
    shore.Usage            = D3D11_USAGE_DEFAULT;
    shore.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    std::vector<float> water(static_cast<size_t>(kShoreSize) * kShoreSize, -1000.0f);
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = water.data();
    data.SysMemPitch = sizeof(float) * kShoreSize;

    if (FAILED(m_graphics->GetDevice()->CreateTexture2D(&shore, &data, m_shoreTexture.GetAddressOf())) ||
        FAILED(m_graphics->GetDevice()->CreateShaderResourceView(m_shoreTexture.Get(), nullptr, m_shoreView.GetAddressOf())))
    {
        m_shoreView.Reset();
        m_shoreTexture.Reset();
    }
}

void WaterRenderer::OnDestroy()
{
    m_ripples.Release();
    m_ocean.Release();
    m_shoreView.Reset();
    m_shoreTexture.Reset();
    m_shoreProviders.clear();
    m_plane.Release();
    m_graphics = nullptr;
}

void WaterRenderer::Update()
{
    const float deltaTime = (std::min)(TimeManager::Get().GetDeltaTime(), 0.25f);
    m_time += deltaTime;

    // 바다 파도 텍스처 (S85) : 매 프레임 렌더 타깃에 새로 그린다
    if (m_graphics)
        m_ocean.Update(m_graphics->GetContext(), m_time, deltaTime);

    if (!m_graphics || !m_ripples.IsValid())
        return;

    const InputManager& input = InputManager::Get();

    // ---- 클릭 / 드래그로 물방울 (S79) ----
    //  누른 순간은 크게, 누른 채 끄는 동안은 지나간 자리마다 작게 떨어뜨린다.
    //  우클릭은 카메라가, 패널 위 마우스는 UI 가 쓴다.
    const bool pointerFree = !input.IsPointerOverUI() && !input.GetMouseButton(InputManager::Right);

    // 진폭 보기 창 위 : 텍스처 칸을 누르면 그 텍셀에, 그래프 칸을 누르면 아무 일도 없다.
    bool overTexture = false;
    const bool overPanel = IsInsideAmplitudePanel(input.GetMouseX(), input.GetMouseY(), &overTexture);

    if (pointerFree && input.GetMouseButton(InputManager::Left) && (!overPanel || overTexture))
    {
        XMFLOAT3 hit{};
        const bool picked = overTexture ? PickFromAmplitudeView(input.GetMouseX(), input.GetMouseY(), hit)
                                        : PickWaterSurface(input.GetMouseX(), input.GetMouseY(), hit);
        if (picked)
        {
            const bool pressed = input.GetMouseButtonDown(InputManager::Left);
            const float dx = hit.x - m_lastDrop.x;
            const float dz = hit.z - m_lastDrop.z;

            if (pressed || !m_dragging || dx * dx + dz * dz > 1.5f)
            {
                m_ripples.AddDrop(hit.x, hit.z, pressed ? 3.0f : 1.6f, pressed ? 3.0f : 0.9f);
                m_lastDrop = hit;
                m_dragging = true;
            }
        }
    }
    else
    {
        m_dragging = false;
    }

    // ---- 해안 마스크를 조금씩 굽는다 (S81) ----
    UpdateShoreMask();

    // ---- 고정 간격으로 시뮬레이션 진행 ----
    m_stepAccumulator += deltaTime;

    int steps = 0;
    while (m_stepAccumulator >= kStepSeconds && steps < kMaxStepsPerFrame)
    {
        if (m_rain)
            AddRainDrops();

        const XMFLOAT3 eye = m_graphics->GetEyePosition3D();
        m_ripples.Step(m_graphics->GetContext(), eye.x, eye.z);

        m_stepAccumulator -= kStepSeconds;
        ++steps;
    }

    // 너무 밀렸으면(씬 로딩 직후 등) 밀린 시간은 버린다. 한꺼번에 따라잡으면 그 프레임이 멈춘다.
    if (steps == kMaxStepsPerFrame)
        m_stepAccumulator = 0.0f;

    if (m_amplitudeView)
        UpdateAmplitudeView(deltaTime);
}

// =============================================================
// 진폭 보기 (S82)
// =============================================================
void WaterRenderer::ToggleAmplitudeView()
{
    m_amplitudeView = !m_amplitudeView;
    if (!m_amplitudeView)
    {
        m_hasReadback = false;
        m_history.clear();
    }
}

// 화면 오른쪽 아래에 세로로 쌓는다 : 머리말 / 텍스처 / 숫자 / 단면 그래프 / 기록 그래프
WaterRenderer::AmplitudeLayout WaterRenderer::ComputeAmplitudeLayout(int viewportWidth, int viewportHeight)
{
    constexpr int margin = 12;
    constexpr int padding = 8;
    constexpr int header = 22;
    constexpr int gap = 8;
    constexpr int textHeight = 4 * 18;
    constexpr int profileHeight = 90;
    constexpr int historyHeight = 56;

    const int width = kViewSize + padding * 2;
    const int height = header + gap + kViewSize + gap + textHeight + gap + profileHeight + gap + historyHeight + gap;

    const int left = viewportWidth - margin - width;
    const int top = (std::max)(margin, viewportHeight - margin - height);

    AmplitudeLayout layout{};
    layout.panel = { left, top, left + width, top + height };

    int y = top + header + gap;
    layout.texture = { left + padding, y, left + padding + kViewSize, y + kViewSize };
    y += kViewSize + gap;
    layout.text = { left + padding, y, left + width - padding, y + textHeight };
    y += textHeight + gap;
    layout.profile = { left + padding, y, left + width - padding, y + profileHeight };
    y += profileHeight + gap;
    layout.history = { left + padding, y, left + width - padding, y + historyHeight };
    return layout;
}

bool WaterRenderer::IsInsideAmplitudePanel(int x, int y, bool* insideTexture) const
{
    if (insideTexture)
        *insideTexture = false;

    if (!m_amplitudeView || !m_graphics)
        return false;

    const AmplitudeLayout layout = ComputeAmplitudeLayout(m_graphics->GetWidth(), m_graphics->GetHeight());
    auto inside = [x, y](const RECT& r) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; };

    if (insideTexture)
        *insideTexture = inside(layout.texture);

    return inside(layout.panel);
}

// 텍스처 창의 픽셀 → 월드 좌표. 창의 위쪽이 +Z 다.
bool WaterRenderer::PickFromAmplitudeView(int x, int y, XMFLOAT3& outHit) const
{
    bool overTexture = false;
    if (!IsInsideAmplitudePanel(x, y, &overTexture) || !overTexture)
        return false;

    const AmplitudeLayout layout = ComputeAmplitudeLayout(m_graphics->GetWidth(), m_graphics->GetHeight());
    const float u = (x - layout.texture.left + 0.5f) / static_cast<float>(kViewSize);
    const float v = 1.0f - (y - layout.texture.top + 0.5f) / static_cast<float>(kViewSize);

    const XMFLOAT3 region = m_ripples.GetRegion();
    outHit = XMFLOAT3(region.x + u * region.z, m_level, region.y + v * region.z);
    return true;
}

void WaterRenderer::UpdateAmplitudeView(float deltaTime)
{
    ID3D11DeviceContext* context = m_graphics->GetContext();

    // 1) 지난번에 부탁한 복사가 끝났으면 받는다
    if (m_ripples.PollReadback(context, m_readback, m_readbackRegion))
    {
        m_hasReadback = true;
        AnalyzeReadback(true);
    }

    // 2) 0.1초마다 새 복사를 부탁한다
    m_readbackTimer += deltaTime;
    if (m_readbackTimer >= kReadbackInterval && !m_ripples.IsReadbackPending())
    {
        m_ripples.RequestReadback(context);
        m_readbackTimer = 0.0f;
    }

    // 3) 커서 : 텍스처 창 위면 그 텍셀, 아니면 수면
    const InputManager& input = InputManager::Get();
    m_probeValid = false;

    if (!input.IsPointerOverUI() && !input.GetMouseButton(InputManager::Right))
    {
        bool overTexture = false;
        const bool overPanel = IsInsideAmplitudePanel(input.GetMouseX(), input.GetMouseY(), &overTexture);

        if (overTexture)
            m_probeValid = PickFromAmplitudeView(input.GetMouseX(), input.GetMouseY(), m_probeWorld);
        else if (!overPanel)
            m_probeValid = PickWaterSurface(input.GetMouseX(), input.GetMouseY(), m_probeWorld);
    }

    if (m_hasReadback)
        AnalyzeReadback(false);   // 커서가 움직였을 수 있으니 단면과 커서 값을 다시 고른다
}

// -------------------------------------------------------------
// 읽어 온 높이에서 숫자를 뽑는다
//  최대 진폭 : |높이| 의 최댓값과 그 자리
//  RMS       : sqrt(평균(높이²)) — 물결 전체의 세기. 감쇠로 줄어드는 모습이 잘 보인다
// -------------------------------------------------------------
void WaterRenderer::AnalyzeReadback(bool freshData)
{
    const int size = m_ripples.GetSize();
    if (m_readback.size() != static_cast<size_t>(size) * size || size <= 0)
        return;

    float maxAbs = 0.0f;
    int maxIndex = 0;
    double sumSquares = 0.0;

    for (size_t i = 0; i < m_readback.size(); ++i)
    {
        const float value = m_readback[i];
        const float absValue = std::fabs(value);
        sumSquares += static_cast<double>(value) * value;
        if (absValue > maxAbs)
        {
            maxAbs = absValue;
            maxIndex = static_cast<int>(i);
        }
    }

    m_maxAmplitude = maxAbs;
    m_maxColumn = maxIndex % size;
    m_maxRow = maxIndex / size;
    m_rmsAmplitude = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(m_readback.size())));

    // 기록은 새로 읽어 온 순간에만 쌓는다 (커서 때문에 같은 데이터로 다시 불려도 쌓지 않는다)
    if (freshData)
    {
        m_history.push_back(maxAbs);
        if (m_history.size() > static_cast<size_t>(kHistorySize))
            m_history.erase(m_history.begin());
    }

    // 커서 자리의 텍셀
    const float texel = m_readbackRegion.z / static_cast<float>(size);
    int probeColumn = -1;
    int probeRow = -1;

    if (m_probeValid)
    {
        probeColumn = static_cast<int>(std::floor((m_probeWorld.x - m_readbackRegion.x) / texel));
        probeRow = static_cast<int>(std::floor((m_probeWorld.z - m_readbackRegion.y) / texel));
        if (probeColumn < 0 || probeRow < 0 || probeColumn >= size || probeRow >= size)
            probeColumn = probeRow = -1;
    }

    m_probeValue = (probeRow >= 0) ? m_readback[static_cast<size_t>(probeRow) * size + probeColumn] : 0.0f;

    // 단면 : 커서가 있으면 커서 줄, 없으면 가장 크게 출렁이는 줄
    m_profileFromCursor = (probeRow >= 0);
    m_profileRow = m_profileFromCursor ? probeRow : m_maxRow;
    m_profileColumn = m_profileFromCursor ? probeColumn : m_maxColumn;

    m_profile.assign(m_readback.begin() + static_cast<size_t>(m_profileRow) * size,
                     m_readback.begin() + static_cast<size_t>(m_profileRow + 1) * size);
}

namespace
{
    void DrawPolyline(HDC hdc, const std::vector<POINT>& points, COLORREF color)
    {
        if (points.size() < 2)
            return;

        HPEN pen = ::CreatePen(PS_SOLID, 1, color);
        HGDIOBJ old = ::SelectObject(hdc, pen);
        ::Polyline(hdc, points.data(), static_cast<int>(points.size()));
        ::SelectObject(hdc, old);
        ::DeleteObject(pen);
    }

    void DrawLine(HDC hdc, int x0, int y0, int x1, int y1, COLORREF color)
    {
        HPEN pen = ::CreatePen(PS_SOLID, 1, color);
        HGDIOBJ old = ::SelectObject(hdc, pen);
        ::MoveToEx(hdc, x0, y0, nullptr);
        ::LineTo(hdc, x1, y1);
        ::SelectObject(hdc, old);
        ::DeleteObject(pen);
    }
}

void WaterRenderer::DrawAmplitudeOverlay(HDC hdc, int viewportWidth, int viewportHeight) const
{
    if (!hdc || !m_amplitudeView)
        return;

    const AmplitudeLayout layout = ComputeAmplitudeLayout(viewportWidth, viewportHeight);
    const RECT& panel = layout.panel;
    const RECT& texture = layout.texture;

    // 텍스처 칸은 D3D 가 이미 그렸다. 그 칸을 덮지 않도록 둘레만 칠한다.
    editor::FillSolid(hdc, { panel.left, panel.top, panel.right, texture.top }, editor::kPanelBackground);
    editor::FillSolid(hdc, { panel.left, texture.bottom, panel.right, panel.bottom }, editor::kPanelBackground);
    editor::FillSolid(hdc, { panel.left, texture.top, texture.left, texture.bottom }, editor::kPanelBackground);
    editor::FillSolid(hdc, { texture.right, texture.top, panel.right, texture.bottom }, editor::kPanelBackground);
    editor::FillSolid(hdc, { panel.left, panel.top, panel.right, panel.top + 22 }, editor::kHeaderBackground);
    editor::FrameSolid(hdc, panel, editor::kPanelBorder);
    editor::FrameSolid(hdc, { texture.left - 1, texture.top - 1, texture.right + 1, texture.bottom + 1 }, editor::kPanelBorder);

    HFONT oldFont = static_cast<HFONT>(::SelectObject(hdc, editor::GetUIFontBold()));
    const int oldBkMode = ::SetBkMode(hdc, TRANSPARENT);

    wchar_t line[160];
    _snwprintf_s(line, _countof(line), _TRUNCATE, L"물결 높이 텍스처  %d x %d · 지형도  (2 닫기)",
                 m_ripples.GetSize(), m_ripples.GetSize());
    editor::DrawLabel(hdc, panel.left + 8, panel.top + 4, line, editor::kTextNormal);

    ::SelectObject(hdc, editor::GetUIFont());

    const int size = m_ripples.GetSize();
    const float texel = m_readbackRegion.z / static_cast<float>((std::max)(1, size));
    const float scale = (std::max)(m_maxAmplitude, 0.05f);   // 그래프 눈금 : 지금 최대 진폭에 맞춘다

    int y = layout.text.top;
    if (!m_hasReadback)
    {
        editor::DrawLabel(hdc, layout.text.left, y, L"값을 읽어 오는 중...", editor::kTextDim);
    }
    else
    {
        _snwprintf_s(line, _countof(line), _TRUNCATE, L"최대 진폭  %.3f   (x %.0f, z %.0f)", m_maxAmplitude,
                     m_readbackRegion.x + (m_maxColumn + 0.5f) * texel, m_readbackRegion.y + (m_maxRow + 0.5f) * texel);
        editor::DrawLabel(hdc, layout.text.left, y, line, editor::kTextSelected);
        y += 18;

        _snwprintf_s(line, _countof(line), _TRUNCATE, L"RMS (전체 세기)  %.4f", m_rmsAmplitude);
        editor::DrawLabel(hdc, layout.text.left, y, line, editor::kTextNormal);
        y += 18;

        if (m_probeValid)
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"커서 높이  %+.3f   (x %.0f, z %.0f)",
                         m_probeValue, m_probeWorld.x, m_probeWorld.z);
        else
            _snwprintf_s(line, _countof(line), _TRUNCATE, L"커서 높이  -   (수면이나 텍스처 위에 올리기)");
        editor::DrawLabel(hdc, layout.text.left, y, line, editor::kTextNormal);
        y += 18;

        _snwprintf_s(line, _countof(line), _TRUNCATE, L"단면 : %s 줄   눈금 ±%.3f",
                     m_profileFromCursor ? L"커서" : L"최대 진폭", scale);
        editor::DrawLabel(hdc, layout.text.left, y, line, editor::kTextDim);

    }

    // ---- 단면 그래프 : 한 줄의 높이를 파형으로 ----
    const RECT& profile = layout.profile;
    editor::FillSolid(hdc, profile, editor::kFieldBackground);
    editor::FrameSolid(hdc, profile, editor::kFieldBorder);

    const int profileMid = (profile.top + profile.bottom) / 2;
    const int profileHalf = (profile.bottom - profile.top) / 2 - 3;
    DrawLine(hdc, profile.left + 1, profileMid, profile.right - 1, profileMid, RGB(80, 80, 80));

    if (m_hasReadback && m_profile.size() == static_cast<size_t>(size))
    {
        const int width = profile.right - profile.left - 2;
        std::vector<POINT> points;
        points.reserve(static_cast<size_t>(width));

        for (int px = 0; px < width; ++px)
        {
            // 픽셀 하나에 텍셀 두 개가 들어간다. 봉우리를 놓치지 않도록 절댓값이 큰 쪽을 쓴다.
            const int begin = px * size / width;
            const int end = (std::max)(begin + 1, (px + 1) * size / width);
            float value = 0.0f;
            for (int i = begin; i < end && i < size; ++i)
                if (std::fabs(m_profile[static_cast<size_t>(i)]) > std::fabs(value))
                    value = m_profile[static_cast<size_t>(i)];

            const float t = (std::max)(-1.0f, (std::min)(1.0f, value / scale));
            points.push_back({ profile.left + 1 + px, profileMid - static_cast<LONG>(t * profileHalf) });
        }
        DrawPolyline(hdc, points, RGB(120, 200, 255));

        if (m_profileColumn >= 0)
        {
            const int markerX = profile.left + 1 + m_profileColumn * width / size;
            DrawLine(hdc, markerX, profile.top + 1, markerX, profile.bottom - 1, RGB(255, 220, 60));
        }
    }

    // ---- 기록 그래프 : 최대 진폭이 시간에 따라 줄어드는 모습 (감쇠) ----
    const RECT& history = layout.history;
    editor::FillSolid(hdc, history, editor::kFieldBackground);
    editor::FrameSolid(hdc, history, editor::kFieldBorder);
    editor::DrawLabel(hdc, history.left + 4, history.top + 2, L"최대 진폭 기록 (최근 8초)", editor::kTextDim);

    if (m_history.size() >= 2)
    {
        float historyMax = 0.05f;
        for (float value : m_history)
            historyMax = (std::max)(historyMax, value);

        const int width = history.right - history.left - 2;
        const int bottom = history.bottom - 3;
        const int graphHeight = history.bottom - history.top - 22;

        std::vector<POINT> points;
        points.reserve(m_history.size());
        for (size_t i = 0; i < m_history.size(); ++i)
        {
            const int x = history.left + 1 + static_cast<int>(i) * width / (kHistorySize - 1);
            const int yValue = bottom - static_cast<int>(m_history[i] / historyMax * graphHeight);
            points.push_back({ x, yValue });
        }
        DrawPolyline(hdc, points, RGB(255, 140, 90));
    }

    ::SetBkMode(hdc, oldBkMode);
    ::SelectObject(hdc, oldFont);
}

// -------------------------------------------------------------
// 마우스 레이와 수면의 교점
//  수면은 y = 수위인 평면이라 식 하나로 풀린다 : origin.y + t · dir.y = 수위
//  다만 그 앞에서 섬이나 산에 먼저 닿았다면 물을 클릭한 것이 아니다.
// -------------------------------------------------------------
bool WaterRenderer::PickWaterSurface(int mouseX, int mouseY, XMFLOAT3& outHit) const
{
    XMFLOAT3 origin{};
    XMFLOAT3 direction{};
    if (!m_graphics->ScreenToRay(mouseX, mouseY, origin, direction))
        return false;

    if (direction.y > -1.0e-4f)
        return false;   // 수평이나 위를 보면 수면에 닿지 않는다

    const float t = (m_level - origin.y) / direction.y;
    if (t <= 0.0f)
        return false;   // 카메라가 물속에 있다

    const GameObject* owner = GetOwner();
    const Scene* scene = owner ? owner->GetScene() : nullptr;

    XMFLOAT3 groundHit{};
    if (scene && ground::RaycastScene(*scene, origin, direction, t, groundHit))
        return false;   // 수면보다 앞에서 땅에 막혔다

    outHit = XMFLOAT3(origin.x + direction.x * t, m_level, origin.z + direction.z * t);
    return true;
}

// -------------------------------------------------------------
// 해안 마스크 (S81)
//  물결 영역 위의 128 x 128 점마다 땅 높이를 물어, 수면보다 높으면 "땅(1)" 으로 적는다.
//  시뮬레이션은 땅인 텍셀의 높이를 0 에 묶어 벽으로 쓴다 → 물결이 섬과 해안에서 반사된다.
//
//  지면 질의는 노이즈 계산이라 1만 6천 번을 한 프레임에 하면 멈춘다.
//  한 프레임에 몇 줄씩만 굽고, 다 굽은 뒤에 한 번에 GPU 로 올린다(그동안은 이전 마스크를 쓴다).
//  영역이 크게 옮겨졌거나 수위가 바뀌었을 때만 새로 굽는다.
// -------------------------------------------------------------
void WaterRenderer::UpdateShoreMask()
{
    if (!m_shoreTexture || !m_graphics)
        return;

    if (m_shoreRow < 0)
    {
        const XMFLOAT3 region = m_ripples.GetRegion();
        const float moved = (std::max)(std::fabs(region.x - m_shoreRegion.x), std::fabs(region.y - m_shoreRegion.y));
        const bool levelChanged = std::fabs(m_level - m_shoreLevel) > 0.01f;

        if (m_hasShore && moved < region.z * 0.1f && !levelChanged)
            return;

        // 새로 굽기 시작 : 이번에 물어볼 지형 컴포넌트를 모아 둔다.
        m_shoreProviders.clear();
        const GameObject* owner = GetOwner();
        if (const Scene* scene = owner ? owner->GetScene() : nullptr)
        {
            for (const auto& object : scene->GetGameObjects())
            {
                if (!object || object->IsPendingDestroy() || !object->IsActive())
                    continue;

                for (const auto& entry : object->GetComponentMap())
                {
                    for (const auto& component : entry.second)
                    {
                        if (const IGroundProvider* provider = dynamic_cast<const IGroundProvider*>(component.get()))
                            m_shoreProviders.push_back(provider);
                    }
                }
            }
        }

        if (m_shoreProviders.empty())
            return;

        m_shoreBuildRegion = region;
        m_shoreBuildLevel = m_level;
        m_shoreBuilding.assign(static_cast<size_t>(kShoreSize) * kShoreSize, -1000.0f);
        m_shoreRow = 0;
    }

    const float cell = m_shoreBuildRegion.z / kShoreSize;
    const int lastRow = (std::min)(kShoreSize, m_shoreRow + kShoreRowsPerFrame);

    for (int row = m_shoreRow; row < lastRow; ++row)
    {
        // 마스크의 행은 +Z 로 커진다 (물결 텍스처와 같은 규약)
        const float z = m_shoreBuildRegion.y + (row + 0.5f) * cell;

        for (int column = 0; column < kShoreSize; ++column)
        {
            const float x = m_shoreBuildRegion.x + (column + 0.5f) * cell;

            // 수면 기준 높이. 양수(수면 위로 드러난 땅)만 벽이다.
            //  얕은 물까지 벽으로 두면 물가에 떨어뜨린 물방울이 바로 지워진다. 지형 밖은 깊은 물로 둔다.
            float relative = -1000.0f;
            for (const IGroundProvider* provider : m_shoreProviders)
            {
                float height = 0.0f;
                if (provider->TryGetGroundHeight(x, z, height))
                    relative = (std::max)(relative, height - m_shoreBuildLevel);
            }

            m_shoreBuilding[static_cast<size_t>(row) * kShoreSize + column] = relative;
        }
    }

    m_shoreRow = lastRow;

    if (m_shoreRow >= kShoreSize)
    {
        m_graphics->GetContext()->UpdateSubresource(m_shoreTexture.Get(), 0, nullptr,
                                                    m_shoreBuilding.data(), sizeof(float) * kShoreSize, 0);
        m_shoreRegion = m_shoreBuildRegion;
        m_shoreLevel = m_shoreBuildLevel;
        m_hasShore = true;
        m_shoreRow = -1;

        m_ripples.SetShoreMask(m_shoreView.Get(), m_shoreRegion);
    }
}

// 빗방울 : 카메라 주변에 작고 약한 물방울을 무작위로 떨어뜨린다.
void WaterRenderer::AddRainDrops()
{
    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();

    std::uniform_real_distribution<float> offset(-150.0f, 150.0f);
    std::uniform_real_distribution<float> radius(0.6f, 1.1f);
    std::uniform_real_distribution<float> strength(0.2f, 0.5f);

    for (int i = 0; i < 2; ++i)
        m_ripples.AddDrop(eye.x + offset(m_rng), eye.z + offset(m_rng), radius(m_rng), strength(m_rng));
}

void WaterRenderer::RenderTransparent()
{
    if (!m_graphics || !m_plane.IsValid())
        return;

    // 반사 패스에서는 물 자신을 그리지 않는다. (자기 자신을 비출 수는 없다)
    if (m_graphics->GetRenderPass() != Graphics::RenderPass::Main)
        return;

    // 격자를 카메라 발밑으로 옮긴다. 가운데 정점 간격의 두 배 단위로만 옮겨 정점이 매 프레임 미끄러지지 않게 한다.
    //  파도는 월드 좌표로 읽으므로 격자가 옮겨져도 파도 모양은 그대로다.
    const XMFLOAT3 eye = m_graphics->GetEyePosition3D();
    const float snap = kNearExtent * 4.0f / static_cast<float>(kGridSize - 1);
    const XMMATRIX world = XMMatrixTranslation(std::floor(eye.x / snap) * snap, m_level, std::floor(eye.z / snap) * snap);

    Graphics::WaterDrawParams params;
    params.time = m_time;
    params.reflection = m_reflection;
    params.refraction = m_refraction;
    params.foam = m_foam;

    if (m_ocean.IsValid())
    {
        for (int i = 0; i < OceanWaves::kCascadeCount; ++i)
        {
            params.oceanDisplacement[i] = m_ocean.GetDisplacementSRV(i);
            params.oceanSlope[i] = m_ocean.GetSlopeSRV(i);
        }
        params.oceanTiles = XMFLOAT3(m_ocean.GetTileSize(0), m_ocean.GetTileSize(1), m_ocean.GetTileSize(2));
        params.significantHeight = m_ocean.GetSignificantHeight();
    }
    params.rippleDebug = m_rippleDebug;

    if (m_hasShore && m_shoreView)
    {
        params.shoreMask = m_shoreView.Get();
        params.shoreRegion = XMFLOAT4(m_shoreRegion.x, m_shoreRegion.y, m_shoreRegion.z, 1.0f);
    }

    if (m_ripples.IsValid())
    {
        const XMFLOAT3 region = m_ripples.GetRegion();
        params.rippleHeight = m_ripples.GetHeightSRV();
        params.rippleRegion = XMFLOAT4(region.x, region.y, region.z, 1.0f);
        params.rippleTexel = 1.0f / static_cast<float>(m_ripples.GetSize());
    }

    m_graphics->DrawWater(m_plane, world, params);

    // 진폭 보기 : 시뮬레이션 텍스처를 화면 창에 그대로 그린다 (S82)
    if (m_amplitudeView && m_ripples.IsValid())
    {
        const AmplitudeLayout layout = ComputeAmplitudeLayout(m_graphics->GetWidth(), m_graphics->GetHeight());
        const XMFLOAT3 region = m_ripples.GetRegion();

        Graphics::RippleViewParams view;
        view.height = m_ripples.GetHeightSRV();
        view.region = XMFLOAT4(region.x, region.y, region.z, 1.0f);
        view.amplitudeScale = (std::max)(m_maxAmplitude, 0.05f);
        view.x = layout.texture.left;
        view.y = layout.texture.top;
        view.size = kViewSize;

        if (m_hasShore && m_shoreView)
        {
            view.shore = m_shoreView.Get();
            view.shoreRegion = XMFLOAT4(m_shoreRegion.x, m_shoreRegion.y, m_shoreRegion.z, 1.0f);
        }

        // 표시 점과 단면 줄 : 읽어 온 시점의 텍셀을 월드로, 다시 지금 영역의 UV 로
        if (m_hasReadback && m_profileRow >= 0 && region.z > 0.0f)
        {
            const float texel = m_readbackRegion.z / static_cast<float>(m_ripples.GetSize());
            const float worldX = m_readbackRegion.x + (m_profileColumn + 0.5f) * texel;
            const float worldZ = m_readbackRegion.y + (m_profileRow + 0.5f) * texel;
            view.marker = XMFLOAT4((worldX - region.x) / region.z, (worldZ - region.y) / region.z,
                                   (worldZ - region.y) / region.z, 1.0f);
        }

        m_graphics->DrawRippleView(view);
    }
}

void WaterRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);
    out["level"]      = json::Value(m_level);
    out["reflection"] = json::Value(m_reflection);
    out["refraction"] = json::Value(m_refraction);
    out["foam"]       = json::Value(m_foam);
    out["windSpeed"]  = json::Value(m_ocean.GetWindSpeed());
    out["windDir"]    = json::Value(m_ocean.GetWindDirection());
    out["choppiness"] = json::Value(m_ocean.GetChoppiness());
    out["rain"]       = json::Value(m_rain);
}

void WaterRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);
    if (const json::Value* value = in.Find("level"))      m_level      = value->AsFloat(m_level);
    if (const json::Value* value = in.Find("reflection")) m_reflection = value->AsBool(m_reflection);
    if (const json::Value* value = in.Find("refraction")) m_refraction = value->AsBool(m_refraction);
    if (const json::Value* value = in.Find("foam"))       m_foam       = value->AsBool(m_foam);
    float windSpeed = m_ocean.GetWindSpeed();
    float windDirection = m_ocean.GetWindDirection();
    if (const json::Value* value = in.Find("windSpeed"))  windSpeed = value->AsFloat(windSpeed);
    if (const json::Value* value = in.Find("windDir"))    windDirection = value->AsFloat(windDirection);
    m_ocean.SetWind(windSpeed, windDirection);
    if (const json::Value* value = in.Find("choppiness")) m_ocean.SetChoppiness(value->AsFloat(m_ocean.GetChoppiness()));
    if (const json::Value* value = in.Find("rain"))       m_rain       = value->AsBool(m_rain);
}

// =============================================================
// 바다 (S85~S87)
// =============================================================

// -------------------------------------------------------------
// 카메라를 따라다니는 격자
//  -1 ~ 1 로 고르게 나눈 좌표 u 를  x = 부호(u) · (a|u| + b|u|⁵)  로 늘인다.
//   가운데는 a 가 지배해 0.3 m 간격(잔물결까지 모양이 나온다),
//   가장자리는 b|u|⁵ 가 지배해 수십 m 간격(멀리 있는 수면은 화면에서 작다).
//  정점 65,536 개로 반경 2 km 를 덮는다.
// -------------------------------------------------------------
bool WaterRenderer::BuildOceanGrid()
{
    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    auto stretch = [](float u)
    {
        const float a = std::fabs(u);
        const float outer = kHalfSize - kNearExtent;   // (far 는 Windows 헤더의 매크로라 이름으로 못 쓴다)
        return (u < 0.0f ? -1.0f : 1.0f) * (kNearExtent * a + outer * a * a * a * a * a);
    };

    std::vector<TerrainVertex> vertices(static_cast<size_t>(kGridSize) * kGridSize);
    for (int row = 0; row < kGridSize; ++row)
    {
        const float z = stretch(static_cast<float>(row) / (kGridSize - 1) * 2.0f - 1.0f);
        for (int column = 0; column < kGridSize; ++column)
        {
            TerrainVertex& vertex = vertices[static_cast<size_t>(row) * kGridSize + column];
            vertex = TerrainVertex{};
            vertex.position = XMFLOAT3(stretch(static_cast<float>(column) / (kGridSize - 1) * 2.0f - 1.0f), 0.0f, z);
            vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(kGridSize - 1) * (kGridSize - 1) * 6);
    for (int row = 0; row + 1 < kGridSize; ++row)
    {
        for (int column = 0; column + 1 < kGridSize; ++column)
        {
            const uint32_t a = static_cast<uint32_t>(row * kGridSize + column);
            const uint32_t b = a + 1;
            const uint32_t c = a + static_cast<uint32_t>(kGridSize);
            const uint32_t d = c + 1;
            indices.push_back(a); indices.push_back(c); indices.push_back(b);
            indices.push_back(b); indices.push_back(c); indices.push_back(d);
        }
    }

    return m_plane.Create(m_graphics->GetDevice(), vertices.data(), static_cast<UINT>(vertices.size()),
                          static_cast<UINT>(sizeof(TerrainVertex)), indices.data(), static_cast<UINT>(indices.size()));
}

void WaterRenderer::CycleWindSpeed()
{
    constexpr float kSpeeds[] = { 4.0f, 8.0f, 13.0f, 18.0f };
    const float current = m_ocean.GetWindSpeed();

    float next = kSpeeds[0];
    for (size_t i = 0; i < sizeof(kSpeeds) / sizeof(kSpeeds[0]); ++i)
    {
        if (kSpeeds[i] > current + 0.5f)
        {
            next = kSpeeds[i];
            break;
        }
    }

    m_ocean.SetWind(next, m_ocean.GetWindDirection());
}

void WaterRenderer::RotateWind(float degrees)
{
    m_ocean.SetWind(m_ocean.GetWindSpeed(), m_ocean.GetWindDirection() + degrees);
}

void WaterRenderer::ToggleChoppy()
{
    m_ocean.SetChoppiness(IsChoppy() ? 0.0f : 1.0f);
}
