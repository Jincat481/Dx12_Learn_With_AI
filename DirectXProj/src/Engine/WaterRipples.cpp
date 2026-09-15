#include "Core/stdafx.h"
#include "Engine/WaterRipples.h"
#include "Graphics/Shader.h"
#include "Graphics/Vertex.h"
#include "Utils/Paths.h"

#include <cmath>

using namespace DirectX;

namespace
{
    // RippleSim.hlsl 의 cbuffer : int2 + float + uint (16) + float4[4] (64) + float4 (16) = 96 바이트
    struct RippleConstants
    {
        int32_t  shiftX;
        int32_t  shiftZ;
        float    damping;
        uint32_t dropCount;
        float    drops[16];
        float    shore[4];
    };
}

bool WaterRipples::Initialize(ID3D11Device* device, int size, float worldSize)
{
    Release();

    if (!device)
        return false;

    m_size = (std::max)(64, size);
    m_worldSize = (std::max)(1.0f, worldSize);
    m_texelSize = m_worldSize / static_cast<float>(m_size);

    // ---- 셰이더 : 한 파일에 화면 덮는 VS 와 시뮬레이션 PS 가 함께 있다 ----
    Shader::Desc desc;
    desc.vsPath = Paths::Resolve(L"Shaders/RippleSim.hlsl");
    desc.psPath = desc.vsPath;
    desc.vsEntry = "VSMain";
    desc.psEntry = "PSMain";
    desc.layout = TerrainVertex::kLayout;          // VS 는 정점 입력을 쓰지 않는다. 그릴 때 레이아웃을 뗀다
    desc.layoutCount = TerrainVertex::kLayoutCount;

    m_shader = std::make_shared<Shader>();
    if (!m_shader->Load(device, desc))
    {
        dxutil::DebugLog(L"[Ripples] 셰이더 로드 실패, 물결을 끈다 : %s", m_shader->GetLastError().c_str());
        Release();
        return false;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(RippleConstants);
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (DX_FAILED(device->CreateBuffer(&cbDesc, nullptr, m_constants.GetAddressOf()), L"CreateBuffer(ripple constants)"))
    {
        Release();
        return false;
    }

    // 화면 덮는 삼각형은 감김 방향과 상관없이 그려져야 한다.
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    if (DX_FAILED(device->CreateRasterizerState(&rasterDesc, m_rasterizer.GetAddressOf()), L"CreateRasterizerState(ripple)"))
    {
        Release();
        return false;
    }

    // 해안 마스크는 해상도가 낮으므로 선형으로 읽어 해안선을 매끄럽게 한다.
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MaxLOD   = D3D11_FLOAT32_MAX;
    if (DX_FAILED(device->CreateSamplerState(&samplerDesc, m_linearSampler.GetAddressOf()), L"CreateSamplerState(ripple)"))
    {
        Release();
        return false;
    }

    // ---- 높이 텍스처 세 장 : 렌더 타깃이자 셰이더 입력 ----
    D3D11_TEXTURE2D_DESC texture = {};
    texture.Width            = static_cast<UINT>(m_size);
    texture.Height           = static_cast<UINT>(m_size);
    texture.MipLevels        = 1;
    texture.ArraySize        = 1;
    texture.Format           = DXGI_FORMAT_R32_FLOAT;     // 음수 높이도 담아야 하므로 UNORM 이 아니라 실수
    texture.SampleDesc.Count = 1;
    texture.Usage            = D3D11_USAGE_DEFAULT;
    texture.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    // 초기값을 명시적으로 0 으로 채운다. 데이터 없이 만든 텍스처의 내용은 보장되지 않는다.
    std::vector<float> zeros(static_cast<size_t>(m_size) * m_size, 0.0f);
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = zeros.data();
    data.SysMemPitch = static_cast<UINT>(sizeof(float) * m_size);

    for (int i = 0; i < 3; ++i)
    {
        if (DX_FAILED(device->CreateTexture2D(&texture, &data, m_textures[i].GetAddressOf()), L"CreateTexture2D(ripple)") ||
            DX_FAILED(device->CreateRenderTargetView(m_textures[i].Get(), nullptr, m_targets[i].GetAddressOf()), L"CreateRenderTargetView(ripple)") ||
            DX_FAILED(device->CreateShaderResourceView(m_textures[i].Get(), nullptr, m_views[i].GetAddressOf()), L"CreateShaderResourceView(ripple)"))
        {
            Release();
            return false;
        }
    }

    m_previous = 0;
    m_current = 1;
    m_next = 2;
    m_hasOrigin = false;
    return true;
}

void WaterRipples::Release()
{
    for (int i = 0; i < 3; ++i)
    {
        m_views[i].Reset();
        m_targets[i].Reset();
        m_textures[i].Reset();
    }
    m_staging.Reset();
    m_readPending = false;
    m_linearSampler.Reset();
    m_rasterizer.Reset();
    m_constants.Reset();
    m_shader.reset();
    m_shoreMask = nullptr;
    m_drops.clear();
}

bool WaterRipples::IsValid() const
{
    return m_shader && m_shader->IsValid() && m_constants && m_rasterizer && m_linearSampler && m_textures[0];
}

void WaterRipples::AddDrop(float worldX, float worldZ, float radius, float strength)
{
    // 너무 많이 쌓이면 오래된 것부터 버린다. 빗방울이 밀려 화면과 어긋나지 않게.
    if (m_drops.size() >= kMaxQueuedDrops)
        m_drops.erase(m_drops.begin());

    m_drops.push_back({ worldX, worldZ, radius, strength });
}

void WaterRipples::SetShoreMask(ID3D11ShaderResourceView* mask, const XMFLOAT3& region)
{
    m_shoreMask = mask;
    m_shoreRegion = region;
}

ID3D11ShaderResourceView* WaterRipples::GetHeightSRV() const
{
    return m_views[m_current].Get();
}

// -------------------------------------------------------------
// GPU → CPU 읽어 오기 (S82)
//  렌더 타깃은 GPU 메모리라 CPU 가 바로 읽을 수 없다. STAGING 텍스처로 복사한 뒤 Map 한다.
//
//  그냥 Map(READ) 하면 CPU 가 "GPU 가 복사를 끝낼 때까지" 멈춰 기다린다(S72 에서는 시간을 재려고 일부러 그랬다).
//  매 프레임 보여 줄 값에 그 멈춤을 넣으면 화면이 끊긴다. 그래서
//   1) 이번 프레임에는 복사 명령만 넣고
//   2) 다음 프레임부터 D3D11_MAP_FLAG_DO_NOT_WAIT 로 "끝났으면 주고, 아니면 바로 돌아와" 라고 묻는다.
//  값은 한두 프레임 늦지만 멈춤이 없다.
// -------------------------------------------------------------
void WaterRipples::RequestReadback(ID3D11DeviceContext* context)
{
    if (!context || !IsValid() || m_readPending)
        return;

    if (!m_staging)
    {
        ComPtr<ID3D11Device> device;
        context->GetDevice(device.GetAddressOf());

        D3D11_TEXTURE2D_DESC desc = {};
        m_textures[0]->GetDesc(&desc);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        if (!device || FAILED(device->CreateTexture2D(&desc, nullptr, m_staging.GetAddressOf())))
            return;
    }

    // 명령이 줄에 들어간 순간의 내용이 복사된다. 뒤이어 Step 이 텍스처를 바꿔도 이 복사본은 그대로다.
    context->CopyResource(m_staging.Get(), m_textures[m_current].Get());
    m_readRegion = GetRegion();
    m_readPending = true;
}

bool WaterRipples::PollReadback(ID3D11DeviceContext* context, std::vector<float>& outHeights, XMFLOAT3& outRegion)
{
    if (!context || !m_staging || !m_readPending)
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const HRESULT hr = context->Map(m_staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);

    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
        return false;   // 아직 복사 중 : 다음 프레임에 다시

    m_readPending = false;
    if (FAILED(hr))
        return false;

    outHeights.resize(static_cast<size_t>(m_size) * m_size);

    // 한 줄의 실제 폭은 RowPitch 다. 텍스처 폭보다 클 수 있다.
    const UINT rowPitch = mapped.RowPitch / sizeof(float);
    const float* data = static_cast<const float*>(mapped.pData);

    for (int row = 0; row < m_size; ++row)
    {
        const float* source = data + static_cast<size_t>(row) * rowPitch;
        std::copy(source, source + m_size, outHeights.begin() + static_cast<size_t>(row) * m_size);
    }

    context->Unmap(m_staging.Get(), 0);
    outRegion = m_readRegion;
    return true;
}

XMFLOAT3 WaterRipples::GetRegion() const
{
    return XMFLOAT3(m_originTexelX * m_texelSize, m_originTexelZ * m_texelSize, m_worldSize);
}

void WaterRipples::Step(ID3D11DeviceContext* context, float centerX, float centerZ)
{
    if (!context || !IsValid())
        return;

    // ---- 영역 원점 ----
    //  카메라 중심에서 반 폭을 뺀 곳. 16 텍셀 단위로만 옮겨 매 프레임 조금씩 흔들리지 않게 한다.
    const float halfTexels = m_size * 0.5f;
    const int wantX = static_cast<int>(std::floor((centerX / m_texelSize - halfTexels) / kRecenterStep)) * kRecenterStep;
    const int wantZ = static_cast<int>(std::floor((centerZ / m_texelSize - halfTexels) / kRecenterStep)) * kRecenterStep;

    int shiftX = 0;
    int shiftZ = 0;
    if (m_hasOrigin)
    {
        // 원점이 +n 텍셀 옮겨졌다면, 새 텍셀 i 는 예전 텍셀 i + n 과 같은 월드 위치다.
        shiftX = wantX - m_originTexelX;
        shiftZ = wantZ - m_originTexelZ;
    }

    m_originTexelX = wantX;
    m_originTexelZ = wantZ;
    m_hasOrigin = true;

    // ---- 상수 ----
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    RippleConstants* cb = static_cast<RippleConstants*>(mapped.pData);
    cb->shiftX = shiftX;
    cb->shiftZ = shiftZ;
    cb->damping = m_damping;

    const int dropCount = (std::min)(kMaxDropsPerStep, static_cast<int>(m_drops.size()));
    cb->dropCount = static_cast<uint32_t>(dropCount);

    for (int i = 0; i < kMaxDropsPerStep; ++i)
    {
        float* slot = cb->drops + i * 4;
        if (i < dropCount)
        {
            const Drop& drop = m_drops[static_cast<size_t>(i)];
            slot[0] = drop.x / m_texelSize - static_cast<float>(m_originTexelX);
            slot[1] = drop.z / m_texelSize - static_cast<float>(m_originTexelZ);
            slot[2] = drop.radius / m_texelSize;
            slot[3] = drop.strength;
        }
        else
        {
            slot[0] = slot[1] = slot[2] = slot[3] = 0.0f;
        }
    }

    // 텍셀 번호 → 해안 마스크 UV.  월드 = 원점 + (텍셀 + 0.5) · 텍셀 크기,  UV = (월드 - 마스크 원점) / 마스크 크기
    if (m_shoreMask && m_shoreRegion.z > 0.0f)
    {
        cb->shore[0] = m_texelSize / m_shoreRegion.z;
        cb->shore[1] = (m_originTexelX * m_texelSize - m_shoreRegion.x) / m_shoreRegion.z;
        cb->shore[2] = (m_originTexelZ * m_texelSize - m_shoreRegion.y) / m_shoreRegion.z;
        cb->shore[3] = 1.0f;
    }
    else
    {
        cb->shore[0] = cb->shore[1] = cb->shore[2] = cb->shore[3] = 0.0f;
    }
    context->Unmap(m_constants.Get(), 0);

    m_drops.erase(m_drops.begin(), m_drops.begin() + dropCount);

    // ---- 한 단계 : "다음" 텍스처에 화면 덮는 삼각형을 그린다 ----
    //  뷰포트를 텍스처 크기로 바꿔야 픽셀 하나가 텍셀 하나에 맞는다. 끝나면 되돌린다.
    D3D11_VIEWPORT savedViewport = {};
    UINT savedViewportCount = 1;
    context->RSGetViewports(&savedViewportCount, &savedViewport);

    D3D11_VIEWPORT viewport = {};
    viewport.Width    = static_cast<float>(m_size);
    viewport.Height   = static_cast<float>(m_size);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView* target = m_targets[m_next].Get();
    context->OMSetRenderTargets(1, &target, nullptr);          // 깊이 버퍼 없이
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);    // 섞지 않고 덮어쓴다
    context->RSSetState(m_rasterizer.Get());

    m_shader->Bind(context);
    context->IASetInputLayout(nullptr);                        // 정점 버퍼 없이 SV_VertexID 로 그린다
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11ShaderResourceView* inputs[3] = { m_views[m_previous].Get(), m_views[m_current].Get(), m_shoreMask };
    context->PSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
    context->PSSetShaderResources(0, 3, inputs);
    context->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());

    context->Draw(3, 0);

    // 다음 단계에서 이 텍스처들의 역할이 바뀐다. 읽기/쓰기 바인딩을 모두 떼어 둔다.
    ID3D11ShaderResourceView* nullInputs[3] = { nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 3, nullInputs);
    context->OMSetRenderTargets(0, nullptr, nullptr);
    if (savedViewportCount > 0)
        context->RSSetViewports(1, &savedViewport);

    // ---- 이름표 돌리기 : 이전 ← 현재 ← 다음 ← (다 쓴) 이전 ----
    const int recycled = m_previous;
    m_previous = m_current;
    m_current = m_next;
    m_next = recycled;
}
