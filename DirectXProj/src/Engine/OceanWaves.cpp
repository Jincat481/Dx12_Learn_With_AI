#include "Core/stdafx.h"
#include "Engine/OceanWaves.h"
#include "Graphics/Shader.h"
#include "Graphics/Vertex.h"
#include "Utils/Paths.h"

#include <cmath>
#include <random>
#include <map>

using namespace DirectX;

namespace
{
    constexpr float kGravity = 9.81f;
    constexpr float kTwoPi = 6.28318530718f;

    // OceanSim.hlsl 의 cbuffer : float4 2개 + float4[64] = 16 * 66 = 1056 바이트
    struct OceanSimConstants
    {
        float tile;
        float time;
        float choppiness;
        float waveCount;
        float foamThreshold;
        float foamGain;
        float foamDecay;
        float textureSize;
        float waves[OceanWaves::kMaxWaves * 4];
    };

    struct CascadeSetup
    {
        float tile;
        float minWavelength;
        float maxWavelength;
    };

    constexpr CascadeSetup kSetups[OceanWaves::kCascadeCount] =
    {
        { 800.0f, 40.0f, 801.0f },
        {  71.0f,  7.0f,  40.0f },
        {  13.0f,  1.3f,   7.0f },
    };
}

bool OceanWaves::Initialize(ID3D11Device* device)
{
    Release();

    if (!device)
        return false;

    Shader::Desc desc;
    desc.vsPath = Paths::Resolve(L"Shaders/OceanSim.hlsl");
    desc.psPath = desc.vsPath;
    desc.vsEntry = "VSMain";
    desc.psEntry = "PSMain";
    desc.layout = TerrainVertex::kLayout;          // 정점 입력은 쓰지 않는다
    desc.layoutCount = TerrainVertex::kLayoutCount;

    m_shader = std::make_shared<Shader>();
    if (!m_shader->Load(device, desc))
    {
        dxutil::DebugLog(L"[Ocean] 셰이더 로드 실패 : %s", m_shader->GetLastError().c_str());
        Release();
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(OceanSimConstants);
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (DX_FAILED(device->CreateBuffer(&cbDesc, nullptr, m_constants.GetAddressOf()), L"CreateBuffer(ocean constants)"))
    {
        Release();
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    if (DX_FAILED(device->CreateRasterizerState(&rasterDesc, m_rasterizer.GetAddressOf()), L"CreateRasterizerState(ocean)"))
    {
        Release();
        return false;
    }

    // 반정밀도(16비트) 실수 : 32비트 실수는 밉맵 자동 생성이 선택 기능이라 장치에 따라 안 된다.
    //  변위 수 m, 기울기 1 근처의 값에는 16비트로 충분하다.
    D3D11_TEXTURE2D_DESC texture = {};
    texture.Width            = kTextureSize;
    texture.Height           = kTextureSize;
    texture.MipLevels        = 0;                                       // 밉 전체
    texture.ArraySize        = 1;
    texture.Format           = DXGI_FORMAT_R16G16B16A16_FLOAT;
    texture.SampleDesc.Count = 1;
    texture.Usage            = D3D11_USAGE_DEFAULT;
    texture.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture.MiscFlags        = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.GetAddressOf());
    const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    for (int c = 0; c < kCascadeCount; ++c)
    {
        Cascade& cascade = m_cascades[c];
        cascade.tile = kSetups[c].tile;
        cascade.minWavelength = kSetups[c].minWavelength;
        cascade.maxWavelength = kSetups[c].maxWavelength;

        for (int set = 0; set < 2; ++set)
        {
            if (DX_FAILED(device->CreateTexture2D(&texture, nullptr, cascade.displacement[set].GetAddressOf()), L"CreateTexture2D(ocean displacement)") ||
                DX_FAILED(device->CreateRenderTargetView(cascade.displacement[set].Get(), nullptr, cascade.displacementRTV[set].GetAddressOf()), L"CreateRenderTargetView(ocean displacement)") ||
                DX_FAILED(device->CreateShaderResourceView(cascade.displacement[set].Get(), nullptr, cascade.displacementSRV[set].GetAddressOf()), L"CreateShaderResourceView(ocean displacement)") ||
                DX_FAILED(device->CreateTexture2D(&texture, nullptr, cascade.slope[set].GetAddressOf()), L"CreateTexture2D(ocean slope)") ||
                DX_FAILED(device->CreateRenderTargetView(cascade.slope[set].Get(), nullptr, cascade.slopeRTV[set].GetAddressOf()), L"CreateRenderTargetView(ocean slope)") ||
                DX_FAILED(device->CreateShaderResourceView(cascade.slope[set].Get(), nullptr, cascade.slopeSRV[set].GetAddressOf()), L"CreateShaderResourceView(ocean slope)"))
            {
                Release();
                return false;
            }

            // 데이터 없이 만든 텍스처의 내용은 보장되지 않는다. 거품이 첫 프레임에 튀지 않게 비운다.
            if (context)
            {
                context->ClearRenderTargetView(cascade.displacementRTV[set].Get(), zero);
                context->ClearRenderTargetView(cascade.slopeRTV[set].Get(), zero);
            }
        }
    }

    m_current = 0;
    m_dirty = true;
    return true;
}

void OceanWaves::Release()
{
    for (Cascade& cascade : m_cascades)
    {
        for (int set = 0; set < 2; ++set)
        {
            cascade.displacementSRV[set].Reset();
            cascade.displacementRTV[set].Reset();
            cascade.displacement[set].Reset();
            cascade.slopeSRV[set].Reset();
            cascade.slopeRTV[set].Reset();
            cascade.slope[set].Reset();
        }
        cascade.waves.clear();
    }

    m_rasterizer.Reset();
    m_constants.Reset();
    m_shader.reset();
}

bool OceanWaves::IsValid() const
{
    return m_shader && m_shader->IsValid() && m_constants && m_rasterizer && m_cascades[0].displacement[0];
}

void OceanWaves::SetWind(float speed, float directionDegrees)
{
    m_windSpeed = (std::max)(0.5f, speed);
    m_windDirection = std::fmod(directionDegrees + 360.0f, 360.0f);
    m_dirty = true;
}

// 완전히 발달한 바다의 꼭대기 각진동수 ωp = 0.855 · g / V,  깊은 물에서 파장 = 2π g / ωp²
float OceanWaves::GetPeakWavelength() const
{
    const float peakOmega = 0.855f * kGravity / m_windSpeed;
    return kTwoPi * kGravity / (peakOmega * peakOmega);
}

ID3D11ShaderResourceView* OceanWaves::GetDisplacementSRV(int cascade) const
{
    return (cascade >= 0 && cascade < kCascadeCount) ? m_cascades[cascade].displacementSRV[m_current].Get() : nullptr;
}

ID3D11ShaderResourceView* OceanWaves::GetSlopeSRV(int cascade) const
{
    return (cascade >= 0 && cascade < kCascadeCount) ? m_cascades[cascade].slopeSRV[m_current].Get() : nullptr;
}

float OceanWaves::GetTileSize(int cascade) const
{
    return (cascade >= 0 && cascade < kCascadeCount) ? m_cascades[cascade].tile : 1.0f;
}

// -------------------------------------------------------------
// 스펙트럼 (S85)
//
//  JONSWAP (각진동수 ω 기준, 단위 m²·s)
//      S(ω) = α g² / ω⁵ · exp(-5/4 · (ωp/ω)⁴) · γ^r        r = exp(-(ω-ωp)² / (2 σ² ωp²))
//   - exp(-5/4 ...) : 꼭대기보다 긴 파도는 바람이 아직 못 키웠다
//   - 1/ω⁵         : 짧은 파도일수록 에너지가 급격히 줄어든다
//   - γ^r (γ=3.3)  : 꼭대기 근처에 에너지가 뾰족하게 몰린다. 파도 줄기가 또렷해지는 이유다
//
//  뽑는 법
//   - 파장대를 로그 간격 구간으로 나누고 구간마다 여러 방향으로 뽑는다 (긴 파도만 뽑히지 않게)
//   - 방향은 바람 방향 둘레의 정규분포. 꼭대기 근처는 좁게(±20°), 짧은 파도는 넓게(±40°)
//   - 타일에서 이어지도록 파수를 격자 k = 2π(n, m)/타일 에 붙이고, 같은 칸에 모이면 에너지를 더한다
//   - 진폭 A = √(2 · 에너지)       (사인파 하나의 분산은 A² / 2)
//  마지막에 바람 파도의 유의파고를 0.21 V²/g 에 맞추고, 다른 방향에서 온 긴 너울을 더한다.
// -------------------------------------------------------------
void OceanWaves::BuildSpectrum()
{
    m_dirty = false;

    const float peakOmega = 0.855f * kGravity / m_windSpeed;
    const float windRadians = m_windDirection * (kTwoPi / 360.0f);

    std::mt19937 rng(20260915u);     // 같은 바람이면 항상 같은 바다
    std::normal_distribution<float> gaussian(0.0f, 1.0f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    auto jonswap = [peakOmega](float omega)
    {
        constexpr float alpha = 0.0081f;
        constexpr float gamma = 3.3f;
        const float sigma = (omega <= peakOmega) ? 0.07f : 0.09f;
        const float ratio = peakOmega / omega;
        const float peak = std::exp(-(omega - peakOmega) * (omega - peakOmega) / (2.0f * sigma * sigma * peakOmega * peakOmega));
        return alpha * kGravity * kGravity / std::pow(omega, 5.0f) *
               std::exp(-1.25f * ratio * ratio * ratio * ratio) * std::pow(gamma, peak);
    };

    constexpr int kBins = 14;
    constexpr int kDirectionsPerBin = 4;

    double windVariance = 0.0;

    for (Cascade& cascade : m_cascades)
    {
        std::map<std::pair<int, int>, float> energyAt;   // 격자 칸 → 에너지

        const float omegaLow = std::sqrt(kGravity * kTwoPi / cascade.maxWavelength);
        const float omegaHigh = std::sqrt(kGravity * kTwoPi / cascade.minWavelength);
        const float logLow = std::log(omegaLow);
        const float logHigh = std::log(omegaHigh);

        for (int bin = 0; bin < kBins; ++bin)
        {
            const float binLow = std::exp(logLow + (logHigh - logLow) * bin / kBins);
            const float binHigh = std::exp(logLow + (logHigh - logLow) * (bin + 1) / kBins);
            const float dOmega = (binHigh - binLow) / kDirectionsPerBin;

            for (int d = 0; d < kDirectionsPerBin; ++d)
            {
                const float omega = binLow + (binHigh - binLow) * unit(rng);
                const float energy = jonswap(omega) * dOmega;
                const float k = omega * omega / kGravity;

                // 꼭대기보다 짧은 파도일수록 방향이 넓게 퍼진다
                const float spread = 0.35f + 0.35f * (std::min)((std::max)(omega / peakOmega - 1.0f, 0.0f), 2.0f) * 0.5f;
                const float theta = windRadians + gaussian(rng) * spread;

                const int n = static_cast<int>(std::lround(k * std::cos(theta) * cascade.tile / kTwoPi));
                const int m = static_cast<int>(std::lround(k * std::sin(theta) * cascade.tile / kTwoPi));
                if (n == 0 && m == 0)
                    continue;

                energyAt[{ n, m }] += energy;
            }
        }

        cascade.waves.clear();
        const float dk = kTwoPi / cascade.tile;
        for (const auto& entry : energyAt)
        {
            const float amplitude = std::sqrt(2.0f * entry.second);
            cascade.waves.push_back({ entry.first.first * dk, entry.first.second * dk, amplitude, kTwoPi * unit(rng) });
        }

        std::sort(cascade.waves.begin(), cascade.waves.end(),
                  [](const Wave& a, const Wave& b) { return a.amplitude > b.amplitude; });

        // 첫 타일은 너울 자리를 남겨 둔다
        const size_t limit = static_cast<size_t>(&cascade == &m_cascades[0] ? kMaxWaves - 6 : kMaxWaves);
        if (cascade.waves.size() > limit)
            cascade.waves.resize(limit);

        for (const Wave& wave : cascade.waves)
            windVariance += 0.5 * static_cast<double>(wave.amplitude) * wave.amplitude;
    }

    // ---- 바람 파도의 유의파고를 실측 공식에 맞춘다 ----
    const float windHeight = 0.21f * m_windSpeed * m_windSpeed / kGravity;
    const float currentHeight = static_cast<float>(4.0 * std::sqrt(windVariance));
    const float scale = (currentHeight > 1e-6f) ? windHeight / currentHeight : 0.0f;

    for (Cascade& cascade : m_cascades)
        for (Wave& wave : cascade.waves)
            wave.amplitude *= scale;

    // ---- 너울 : 먼 바다의 폭풍이 보낸 긴 파도. 바람과 다른 방향에서 좁게 몰려온다 ----
    Cascade& longest = m_cascades[0];
    const float swellHeight = (std::max)(0.5f, windHeight * 0.35f);
    const float swellAmplitude = std::sqrt(2.0f * (swellHeight / 4.0f) * (swellHeight / 4.0f) / 6.0f);
    const float swellRadians = windRadians - 0.6f;

    for (int i = 0; i < 6; ++i)
    {
        const float wavelength = 160.0f + 60.0f * unit(rng);
        const float k = kTwoPi / wavelength;
        const float theta = swellRadians + gaussian(rng) * 0.08f;
        const int n = static_cast<int>(std::lround(k * std::cos(theta) * longest.tile / kTwoPi));
        const int m = static_cast<int>(std::lround(k * std::sin(theta) * longest.tile / kTwoPi));
        if (n == 0 && m == 0)
            continue;

        const float dk = kTwoPi / longest.tile;
        longest.waves.push_back({ n * dk, m * dk, swellAmplitude, kTwoPi * unit(rng) });
    }

    m_significantHeight = std::sqrt(windHeight * windHeight + swellHeight * swellHeight);

    // ---- 흰 파도머리 : 바람이 셀수록 급격히 늘어난다 (관측 : 덮는 면적 ∝ V^3.4) ----
    // ---- 뾰족함 상한 ----
    //  게르스트너로 수평으로 미는 양이 크면 마루에서 면이 뒤집혀 검은 주름과 덩어리 거품이 생긴다.
    //  모든 파도가 한 점에서 마루를 맞춰도 접히지 않는 조건 : λ · Σ A·k ≤ 1
    //  실제로는 모두 겹치는 일이 드물어 1.8 배까지 허용한다.
    double steepness = 0.0;
    for (const Cascade& cascade : m_cascades)
        for (const Wave& wave : cascade.waves)
            steepness += static_cast<double>(wave.amplitude) * std::sqrt(wave.kx * wave.kx + wave.kz * wave.kz);
    m_choppinessLimit = (steepness > 1e-6) ? (std::min)(1.0f, static_cast<float>(1.8 / steepness)) : 1.0f;

    // ---- 흰 파도머리 : 바람이 셀수록 늘어난다 (관측 : 덮는 면적 ∝ V^3.4). 약한 바람엔 거의 없다 ----
    m_foamThreshold = 0.25f + 0.35f * (std::min)((std::max)((m_windSpeed - 5.0f) / 13.0f, 0.0f), 1.0f);

    dxutil::DebugLog(L"[Ocean] 바람 %.1f m/s, %.0f도 → 파고 %.2f m (너울 %.2f m), 주요 파장 %.0f m, 뾰족함 상한 %.2f, 파도 %zu / %zu / %zu개",
                     m_windSpeed, m_windDirection, m_significantHeight, swellHeight, GetPeakWavelength(), m_choppinessLimit,
                     m_cascades[0].waves.size(), m_cascades[1].waves.size(), m_cascades[2].waves.size());
}

// -------------------------------------------------------------
// 한 프레임 (S85, S86)
//  타일마다 : 지난 프레임 변위(거품)를 읽어 새 변위 · 기울기를 MRT 로 그린다 → 밉맵
// -------------------------------------------------------------
void OceanWaves::Update(ID3D11DeviceContext* context, float time, float deltaTime)
{
    if (!context || !IsValid())
        return;

    if (m_dirty)
        BuildSpectrum();

    D3D11_VIEWPORT savedViewport = {};
    UINT savedViewportCount = 1;
    context->RSGetViewports(&savedViewportCount, &savedViewport);

    D3D11_VIEWPORT viewport = {};
    viewport.Width    = static_cast<float>(kTextureSize);
    viewport.Height   = static_cast<float>(kTextureSize);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    context->RSSetState(m_rasterizer.Get());
    m_shader->Bind(context);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const int next = 1 - m_current;
    const float foamDecay = std::exp(-(std::max)(deltaTime, 0.0f) / kFoamLifetime);

    for (Cascade& cascade : m_cascades)
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->Map(m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            continue;

        OceanSimConstants* cb = static_cast<OceanSimConstants*>(mapped.pData);
        cb->tile = cascade.tile;
        cb->time = time;
        cb->choppiness = m_choppiness * m_choppinessLimit;
        cb->waveCount = static_cast<float>(cascade.waves.size());
        cb->foamThreshold = m_foamThreshold;
        cb->foamGain = kFoamGain;
        cb->foamDecay = foamDecay;
        cb->textureSize = static_cast<float>(kTextureSize);

        for (int i = 0; i < kMaxWaves; ++i)
        {
            float* slot = cb->waves + i * 4;
            if (i < static_cast<int>(cascade.waves.size()))
            {
                const Wave& wave = cascade.waves[static_cast<size_t>(i)];
                slot[0] = wave.kx;
                slot[1] = wave.kz;
                slot[2] = wave.amplitude;
                slot[3] = wave.phase;
            }
            else
            {
                slot[0] = slot[1] = slot[2] = slot[3] = 0.0f;
            }
        }
        context->Unmap(m_constants.Get(), 0);

        ID3D11RenderTargetView* targets[2] = { cascade.displacementRTV[next].Get(), cascade.slopeRTV[next].Get() };
        context->OMSetRenderTargets(2, targets, nullptr);

        ID3D11ShaderResourceView* previous = cascade.displacementSRV[m_current].Get();
        context->PSSetShaderResources(0, 1, &previous);
        context->PSSetConstantBuffers(0, 1, m_constants.GetAddressOf());

        context->Draw(3, 0);

        ID3D11ShaderResourceView* nullView = nullptr;
        context->PSSetShaderResources(0, 1, &nullView);
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    m_current = next;

    // 밉맵 : 멀리 있는 수면은 텍셀 여러 개가 픽셀 하나에 들어간다. 평균을 미리 만들어 두어야 반짝이지 않는다.
    for (Cascade& cascade : m_cascades)
    {
        context->GenerateMips(cascade.displacementSRV[m_current].Get());
        context->GenerateMips(cascade.slopeSRV[m_current].Get());
    }

    if (savedViewportCount > 0)
        context->RSSetViewports(1, &savedViewport);
}
