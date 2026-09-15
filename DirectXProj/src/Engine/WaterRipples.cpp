#include "Core/stdafx.h"
#include "Engine/WaterRipples.h"
#include "Graphics/Shader.h"
#include "Graphics/Vertex.h"
#include "Utils/Paths.h"

#include <DirectXPackedVector.h>
#include <cmath>

using namespace DirectX;

namespace
{
    // RippleSim.hlsl 의 cbuffer : float2 + float + float = 16 바이트
    struct SplatConstants
    {
        float originX;
        float originZ;
        float texelSize;
        float texels;
    };

    // RippleSim.hlsl 의 WaveParticle 과 같은 배치 (32 바이트)
    struct GpuParticle
    {
        float x;
        float z;
        float dirX;
        float dirZ;
        float amplitude;
        float radius;
        float padding[2];
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

    // ---- 셰이더 : 입자 하나를 사각형으로 펼치는 VS 와 물결 모양을 칠하는 PS ----
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
    cbDesc.ByteWidth      = sizeof(SplatConstants);
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (DX_FAILED(device->CreateBuffer(&cbDesc, nullptr, m_constants.GetAddressOf()), L"CreateBuffer(ripple constants)"))
    {
        Release();
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;     // 입자 방향에 따라 사각형이 뒤집혀도 그려져야 한다
    rasterDesc.DepthClipEnable = TRUE;
    if (DX_FAILED(device->CreateRasterizerState(&rasterDesc, m_rasterizer.GetAddressOf()), L"CreateRasterizerState(ripple)"))
    {
        Release();
        return false;
    }

    // 겹친 입자의 높이는 더해져야 한다 : 결과 = 기존 + 새 값
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (DX_FAILED(device->CreateBlendState(&blendDesc, m_additive.GetAddressOf()), L"CreateBlendState(ripple)"))
    {
        Release();
        return false;
    }

    // ---- 높이 텍스처 : 음수도 담아야 하므로 실수. 32비트 실수에 블렌드가 안 되는 장치면 16비트로 ----
    UINT support = 0;
    const bool floatBlend = SUCCEEDED(device->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT, &support)) &&
                            (support & D3D11_FORMAT_SUPPORT_BLENDABLE) != 0;
    m_format = floatBlend ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_FLOAT;
    if (!floatBlend)
        dxutil::DebugLog(L"[Ripples] R32_FLOAT 블렌드 미지원, R16_FLOAT 사용");

    D3D11_TEXTURE2D_DESC texture = {};
    texture.Width            = static_cast<UINT>(m_size);
    texture.Height           = static_cast<UINT>(m_size);
    texture.MipLevels        = 1;
    texture.ArraySize        = 1;
    texture.Format           = m_format;
    texture.SampleDesc.Count = 1;
    texture.Usage            = D3D11_USAGE_DEFAULT;
    texture.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    // 초기값을 명시적으로 0 으로 채운다. (0 비트는 32 · 16 비트 실수 모두 0.0)
    const UINT bytesPerTexel = floatBlend ? 4u : 2u;
    std::vector<uint8_t> zeros(static_cast<size_t>(m_size) * m_size * bytesPerTexel, 0);
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = zeros.data();
    data.SysMemPitch = bytesPerTexel * static_cast<UINT>(m_size);

    if (DX_FAILED(device->CreateTexture2D(&texture, &data, m_texture.GetAddressOf()), L"CreateTexture2D(ripple)") ||
        DX_FAILED(device->CreateRenderTargetView(m_texture.Get(), nullptr, m_target.GetAddressOf()), L"CreateRenderTargetView(ripple)") ||
        DX_FAILED(device->CreateShaderResourceView(m_texture.Get(), nullptr, m_view.GetAddressOf()), L"CreateShaderResourceView(ripple)"))
    {
        Release();
        return false;
    }

    // ---- 입자 버퍼 : 최대 입자 수만큼 한 번에 잡아 두고 매 프레임 덮어쓴다 ----
    m_particleCapacity = static_cast<UINT>(m_waves.GetSettings().maxParticles);

    D3D11_BUFFER_DESC particleDesc = {};
    particleDesc.ByteWidth           = sizeof(GpuParticle) * m_particleCapacity;
    particleDesc.Usage               = D3D11_USAGE_DYNAMIC;
    particleDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    particleDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    particleDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    particleDesc.StructureByteStride = sizeof(GpuParticle);

    D3D11_SHADER_RESOURCE_VIEW_DESC particleView = {};
    particleView.Format              = DXGI_FORMAT_UNKNOWN;
    particleView.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    particleView.Buffer.FirstElement = 0;
    particleView.Buffer.NumElements  = m_particleCapacity;

    if (DX_FAILED(device->CreateBuffer(&particleDesc, nullptr, m_particleBuffer.GetAddressOf()), L"CreateBuffer(wave particles)") ||
        DX_FAILED(device->CreateShaderResourceView(m_particleBuffer.Get(), &particleView, m_particleView.GetAddressOf()), L"CreateShaderResourceView(wave particles)"))
    {
        Release();
        return false;
    }

    m_renderedCount = 0;
    return true;
}

void WaterRipples::Release()
{
    m_view.Reset();
    m_target.Reset();
    m_texture.Reset();
    m_particleView.Reset();
    m_particleBuffer.Reset();
    m_particleCapacity = 0;
    m_renderedCount = 0;
    m_staging.Reset();
    m_readPending = false;
    m_additive.Reset();
    m_rasterizer.Reset();
    m_constants.Reset();
    m_shader.reset();
    m_waves.Clear();
}

bool WaterRipples::IsValid() const
{
    return m_shader && m_shader->IsValid() && m_constants && m_rasterizer && m_additive && m_texture && m_particleView;
}

void WaterRipples::AddDrop(float worldX, float worldZ, float radius, float strength)
{
    m_waves.Emit(worldX, worldZ, radius, strength * kStrengthToHeight);
}

void WaterRipples::SetShore(const std::vector<float>& heights, int size, const XMFLOAT3& region)
{
    m_waves.SetShore(heights, size, region.x, region.y, region.z);
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
        m_texture->GetDesc(&desc);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.BindFlags      = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        if (!device || FAILED(device->CreateTexture2D(&desc, nullptr, m_staging.GetAddressOf())))
            return;
    }

    // 명령이 줄에 들어간 순간의 내용이 복사된다. 뒤이어 Render 가 텍스처를 바꿔도 이 복사본은 그대로다.
    context->CopyResource(m_staging.Get(), m_texture.Get());
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
    for (int row = 0; row < m_size; ++row)
    {
        const size_t outRow = static_cast<size_t>(row) * m_size;

        if (m_format == DXGI_FORMAT_R32_FLOAT)
        {
            const float* source = static_cast<const float*>(mapped.pData) + static_cast<size_t>(row) * (mapped.RowPitch / sizeof(float));
            std::copy(source, source + m_size, outHeights.begin() + outRow);
        }
        else
        {
            const uint16_t* source = static_cast<const uint16_t*>(mapped.pData) + static_cast<size_t>(row) * (mapped.RowPitch / sizeof(uint16_t));
            for (int column = 0; column < m_size; ++column)
                outHeights[outRow + column] = PackedVector::XMConvertHalfToFloat(source[column]);
        }
    }

    context->Unmap(m_staging.Get(), 0);
    outRegion = m_readRegion;
    return true;
}

XMFLOAT3 WaterRipples::GetRegion() const
{
    return XMFLOAT3(m_originTexelX * m_texelSize, m_originTexelZ * m_texelSize, m_worldSize);
}

void WaterRipples::Step(float centerX, float centerZ)
{
    if (!IsValid())
        return;

    // ---- 영역 원점 ----
    //  카메라 중심에서 반 폭을 뺀 곳. 16 텍셀 단위로만 옮겨 해안 맵을 매 프레임 다시 굽지 않게 한다.
    const float halfTexels = m_size * 0.5f;
    m_originTexelX = static_cast<int>(std::floor((centerX / m_texelSize - halfTexels) / kRecenterStep)) * kRecenterStep;
    m_originTexelZ = static_cast<int>(std::floor((centerZ / m_texelSize - halfTexels) / kRecenterStep)) * kRecenterStep;

    const XMFLOAT3 region = GetRegion();
    m_waves.SetBounds(region.x - kBoundsMargin, region.y - kBoundsMargin,
                      region.x + region.z + kBoundsMargin, region.y + region.z + kBoundsMargin);
    m_waves.Step();
}

// -------------------------------------------------------------
// 입자 → 높이 텍스처
//  1) 입자 목록을 구조체 버퍼에 복사한다
//  2) 높이 텍스처를 0 으로 지우고 렌더 타깃으로 건다
//  3) 입자마다 사각형 하나(정점 6개)를 그린다. 정점 버퍼 없이 SV_VertexID · SV_InstanceID 로 만든다
//  4) 가산 블렌드라 겹친 입자의 높이가 더해진다 → 이웃 입자들이 이어져 하나의 고리가 된다
// -------------------------------------------------------------
void WaterRipples::Render(ID3D11DeviceContext* context)
{
    if (!context || !IsValid())
        return;

    const std::vector<WaveParticles::Particle>& particles = m_waves.GetParticles();
    const UINT count = (std::min)(static_cast<UINT>(particles.size()), m_particleCapacity);

    // 지난번에도 비어 있었다면 텍스처는 이미 0 이다
    if (count == 0 && m_renderedCount == 0)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (count > 0)
    {
        if (FAILED(context->Map(m_particleBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return;

        GpuParticle* out = static_cast<GpuParticle*>(mapped.pData);
        for (UINT i = 0; i < count; ++i)
        {
            const WaveParticles::Particle& p = particles[i];
            out[i] = { p.x, p.z, p.dirX, p.dirZ, p.amplitude, p.radius, { 0.0f, 0.0f } };
        }
        context->Unmap(m_particleBuffer.Get(), 0);
    }

    if (FAILED(context->Map(m_constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    SplatConstants* cb = static_cast<SplatConstants*>(mapped.pData);
    const XMFLOAT3 region = GetRegion();
    cb->originX = region.x;
    cb->originZ = region.y;
    cb->texelSize = m_texelSize;
    cb->texels = static_cast<float>(m_size);
    context->Unmap(m_constants.Get(), 0);

    // 뷰포트를 텍스처 크기로 바꿔야 픽셀 하나가 텍셀 하나에 맞는다. 끝나면 되돌린다.
    D3D11_VIEWPORT savedViewport = {};
    UINT savedViewportCount = 1;
    context->RSGetViewports(&savedViewportCount, &savedViewport);

    D3D11_VIEWPORT viewport = {};
    viewport.Width    = static_cast<float>(m_size);
    viewport.Height   = static_cast<float>(m_size);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView* target = m_target.Get();
    context->OMSetRenderTargets(1, &target, nullptr);          // 깊이 버퍼 없이
    const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    context->ClearRenderTargetView(target, clear);

    if (count > 0)
    {
        context->OMSetBlendState(m_additive.Get(), nullptr, 0xFFFFFFFF);
        context->RSSetState(m_rasterizer.Get());

        m_shader->Bind(context);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11ShaderResourceView* particleView = m_particleView.Get();
        context->VSSetConstantBuffers(0, 1, m_constants.GetAddressOf());
        context->VSSetShaderResources(0, 1, &particleView);

        context->DrawInstanced(6, count, 0, 0);

        ID3D11ShaderResourceView* nullView = nullptr;
        context->VSSetShaderResources(0, 1, &nullView);
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    }

    // 뒤따르는 메인 패스가 이 텍스처를 읽으므로 렌더 타깃에서 떼어 둔다
    context->OMSetRenderTargets(0, nullptr, nullptr);
    if (savedViewportCount > 0)
        context->RSSetViewports(1, &savedViewport);

    m_renderedCount = count;
}
