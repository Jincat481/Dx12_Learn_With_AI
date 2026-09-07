#include "Core/stdafx.h"
#include "Core/Graphics.h"
#include "Graphics/Shader.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/TextureManager.h"
#include "Utils/Paths.h"

using namespace DirectX;

Graphics::~Graphics()
{
    Shutdown();
}

void Graphics::SetClearColor(float r, float g, float b, float a)
{
    m_clearColor[0] = r; m_clearColor[1] = g; m_clearColor[2] = b; m_clearColor[3] = a;
}

bool Graphics::Initialize(HWND hwnd, int width, int height)
{
    m_width = width;
    m_height = height;

    if (!CreateDeviceAndSwapChain(hwnd)) return false;
    if (!CreateRenderTargetView())       return false;
    if (!CreateDepthBuffer())            return false;
    if (!CreateSpritePipeline())         return false;
    if (!CreateMeshPipeline())           return false;

    TextureManager::Get().Initialize(m_device.Get());

    // 2D 카메라 : 화면 중앙이 원점, 단위는 픽셀, +Y 가 위쪽.
    const XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, -10.0f, 1.0f),
                                           XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
                                           XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixOrthographicLH(static_cast<float>(width),
                                                       static_cast<float>(height),
                                                       0.1f, 1000.0f);
    XMStoreFloat4x4(&m_viewMatrix, view);
    XMStoreFloat4x4(&m_projectionMatrix, projection);

    // Camera 컴포넌트가 없을 때를 대비한 기본 3D 카메라
    SetCamera3D(XMMatrixLookAtLH(XMVectorSet(40.0f, 40.0f, -40.0f, 1.0f),
                                 XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
                                 XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
                XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), GetAspectRatio(), 0.5f, 1000.0f),
                XMFLOAT3(40.0f, 40.0f, -40.0f));

    dxutil::DebugLog(L"[Graphics] 초기화 완료 (%dx%d)", width, height);
    return true;
}

void Graphics::Shutdown()
{
    if (m_context)
    {
        m_context->ClearState();
        m_context->Flush();
    }

    m_spriteShader.reset();
    m_rasterizerState.Reset();
    m_blendState.Reset();
    m_samplerState.Reset();
    m_constantBuffer.Reset();
    m_terrainShader.reset();
    m_rasterWireframeState.Reset();
    m_rasterSolidState.Reset();
    m_meshConstantBuffer.Reset();
    m_depthDisabledState.Reset();
    m_depthEnabledState.Reset();
    m_depthStencilView.Reset();
    m_depthStencilTexture.Reset();

    m_backBufferSurface.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool Graphics::CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount                        = 1;
    desc.BufferDesc.Width                   = static_cast<UINT>(m_width);
    desc.BufferDesc.Height                  = static_cast<UINT>(m_height);
    // GDI 오버레이(Hierarchy 패널)를 쓰려면 백버퍼가 BGRA + GDI 호환이어야 한다.
    desc.BufferDesc.Format                  = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator   = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow                       = hwnd;
    desc.SampleDesc.Count                   = 1;
    desc.SampleDesc.Quality                 = 0;
    desc.Windowed                           = TRUE;
    desc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
    desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;      // 디버그 레이어 : 잘못된 API 사용을 출력창에 알려준다
#endif

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
    D3D_FEATURE_LEVEL obtained = {};

    HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
        &desc, m_swapChain.GetAddressOf(), m_device.GetAddressOf(),
        &obtained, m_context.GetAddressOf());

#ifdef _DEBUG
    if (FAILED(hr))
    {
        // 디버그 레이어(그래픽 도구)가 설치되어 있지 않은 환경을 위한 재시도
        dxutil::DebugLog(L"[Graphics] 디버그 레이어 없이 다시 시도한다.");
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = ::D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
            &desc, m_swapChain.GetAddressOf(), m_device.GetAddressOf(),
            &obtained, m_context.GetAddressOf());
    }
#endif

    return DX_CHECK(hr, L"D3D11CreateDeviceAndSwapChain");
}

bool Graphics::CreateRenderTargetView()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    if (DX_FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())), L"IDXGISwapChain::GetBuffer"))
        return false;

    if (DX_FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf()),
                  L"CreateRenderTargetView"))
        return false;

    // GDI 오버레이용 DXGI 표면. 실패해도 렌더링 자체는 계속되게 둔다.
    if (FAILED(backBuffer.As(&m_backBufferSurface)))
        dxutil::DebugLog(L"[Graphics] IDXGISurface1 을 얻지 못했다. Hierarchy 패널이 표시되지 않는다.");

    // 뷰포트 : 클립 공간(-1..1)을 픽셀 좌표로 옮기는 규칙
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = static_cast<float>(m_width);
    viewport.Height   = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    return true;
}

bool Graphics::CreateSpritePipeline()
{
    // ---- 상수 버퍼 (b0) : 매 Draw 마다 갱신하므로 DYNAMIC + CPU 쓰기 ----
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(SpriteConstantBuffer);   // 16의 배수여야 한다
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (DX_FAILED(m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf()), L"CreateBuffer(constant)"))
        return false;

    // ---- 샘플러 (s0) ----
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter        = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU      = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV      = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW      = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD        = D3D11_FLOAT32_MAX;

    if (DX_FAILED(m_device->CreateSamplerState(&samplerDesc, m_samplerState.GetAddressOf()), L"CreateSamplerState"))
        return false;

    // ---- 알파 블렌딩 : PNG 의 투명 영역 처리 ----
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    if (DX_FAILED(m_device->CreateBlendState(&blendDesc, m_blendState.GetAddressOf()), L"CreateBlendState"))
        return false;

    // ---- 래스터라이저 : 2D 는 뒷면 컬링을 끄는 편이 편하다 ----
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;

    if (DX_FAILED(m_device->CreateRasterizerState(&rasterDesc, m_rasterizerState.GetAddressOf()), L"CreateRasterizerState"))
        return false;

    // ---- 스프라이트 셰이더 ----
    Shader::Desc shaderDesc;
    shaderDesc.vsPath = Paths::Resolve(L"Shaders/SpriteVS.hlsl");
    shaderDesc.psPath = Paths::Resolve(L"Shaders/SpritePS.hlsl");
    shaderDesc.vsEntry = "main";
    shaderDesc.psEntry = "main";
    shaderDesc.layout = Vertex::kLayout;
    shaderDesc.layoutCount = Vertex::kLayoutCount;
    shaderDesc.cacheCompiled = (SHADER_CACHE_ENABLED != 0);
    shaderDesc.useCache      = (SHADER_CACHE_ENABLED != 0);

    m_spriteShader = std::make_shared<Shader>();
    if (!m_spriteShader->Load(m_device.Get(), shaderDesc))
    {
        dxutil::DebugLog(L"[Graphics] 스프라이트 셰이더 로드 실패 :\n%s", m_spriteShader->GetLastError().c_str());
        return false;
    }

#if HOT_RELOAD_ENABLED
    // Debug 빌드에서만 파일 감시 대상으로 등록한다.
    ShaderManager::Get().Register("Sprite", m_spriteShader);
#endif

    return true;
}

void Graphics::BeginFrame()
{
    if (!m_context)
        return;

    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), m_clearColor);

    // 깊이는 가장 먼 값(1.0)으로 초기화한다. 이걸 빼먹으면 이전 프레임의
    // 깊이가 남아 새 프레임의 물체가 가려진다. (S29)
    if (m_depthStencilView)
        m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->RSSetState(m_rasterizerState.Get());
}

void Graphics::EndFrame()
{
    if (m_swapChain)
        m_swapChain->Present(1, 0);      // 1 = VSync
}

// -------------------------------------------------------------
// GDI 오버레이
//  GetDC(FALSE) 는 기존 백버퍼 내용을 지우지 않으므로
//  씬 위에 UI 를 덧그릴 수 있다. 반드시 짝을 맞춰 ReleaseDC 한다.
// -------------------------------------------------------------
HDC Graphics::BeginOverlay()
{
    if (!m_backBufferSurface || m_overlayActive)
        return nullptr;

    HDC hdc = nullptr;
    const HRESULT hr = m_backBufferSurface->GetDC(FALSE /*Discard*/, &hdc);
    if (FAILED(hr))
    {
        dxutil::DebugLog(L"[Graphics] IDXGISurface1::GetDC 실패 (0x%08X)", static_cast<unsigned>(hr));
        return nullptr;
    }

    m_overlayActive = true;
    return hdc;
}

void Graphics::EndOverlay()
{
    if (!m_backBufferSurface || !m_overlayActive)
        return;

    m_backBufferSurface->ReleaseDC(nullptr);
    m_overlayActive = false;
}

void Graphics::DrawSprite(ID3D11Buffer* vertexBuffer,
                          ID3D11Buffer* indexBuffer,
                          UINT indexCount,
                          ID3D11ShaderResourceView* srv,
                          FXMMATRIX world,
                          const XMFLOAT4& color,
                          bool silhouette)
{
    if (!m_context || !vertexBuffer || !indexBuffer || !m_spriteShader || !m_spriteShader->IsValid())
        return;

    // 1) WVP 를 만들어 상수 버퍼에 올린다.
    //    HLSL 은 기본이 column-major 이므로 CPU 에서 전치해서 넘긴다. (S12)
    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    const XMMATRIX wvp = world * view * projection;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        SpriteConstantBuffer* cb = static_cast<SpriteConstantBuffer*>(mapped.pData);
        XMStoreFloat4x4(&cb->wvp, XMMatrixTranspose(wvp));
        cb->color = color;
        cb->params = XMFLOAT4(silhouette ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        m_context->Unmap(m_constantBuffer.Get(), 0);
    }

    // 2) 파이프라인 바인딩
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;

    // 스프라이트는 그린 순서(화가 알고리즘)로 겹치므로 깊이 테스트를 끈다.
    m_context->OMSetDepthStencilState(m_depthDisabledState.Get(), 0);
    m_context->RSSetState(m_rasterizerState.Get());

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    m_spriteShader->Bind(m_context.Get());

    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetShaderResources(0, 1, &srv);
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // 3) Quad = 삼각형 2개 = 인덱스 6개
    m_context->DrawIndexed(indexCount, 0, 0);
}

// -------------------------------------------------------------
// 화면 좌표 → 월드 좌표 (2D 피킹)
//  1) 클라이언트 픽셀 → NDC(-1..1). y 는 위아래가 뒤집힌다.
//  2) (View * Projection) 의 역행렬로 월드로 되돌린다.
//  직교 투영이라 x, y 는 z 와 무관하므로 z = 0 평면 위의 점으로 돌려준다.
// -------------------------------------------------------------
XMFLOAT3 Graphics::ScreenToWorld(int screenX, int screenY) const
{
    if (m_width <= 0 || m_height <= 0)
        return XMFLOAT3(0.0f, 0.0f, 0.0f);

    const float ndcX = (2.0f * static_cast<float>(screenX) / static_cast<float>(m_width)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(screenY) / static_cast<float>(m_height));

    const XMMATRIX viewProjection = GetViewMatrix() * GetProjectionMatrix();

    XMVECTOR determinant = XMMatrixDeterminant(viewProjection);
    if (XMVectorGetX(XMVectorAbs(determinant)) < 1.0e-12f)
        return XMFLOAT3(0.0f, 0.0f, 0.0f);

    const XMMATRIX inverse = XMMatrixInverse(&determinant, viewProjection);
    const XMVECTOR world = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inverse);

    return XMFLOAT3(XMVectorGetX(world), XMVectorGetY(world), 0.0f);
}

// =============================================================
// 깊이 버퍼 (S29)
//  3D 는 그린 순서가 아니라 카메라와의 거리로 앞뒤가 정해진다.
//  픽셀마다 깊이를 저장해 두고, 더 먼 픽셀은 버린다.
// =============================================================
bool Graphics::CreateDepthBuffer()
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = static_cast<UINT>(m_width);
    desc.Height           = static_cast<UINT>(m_height);
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;   // 깊이 24비트 + 스텐실 8비트
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    if (DX_FAILED(m_device->CreateTexture2D(&desc, nullptr, m_depthStencilTexture.GetAddressOf()),
                  L"CreateTexture2D(depth)"))
        return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = desc.Format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    if (DX_FAILED(m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), &dsvDesc,
                                                   m_depthStencilView.GetAddressOf()),
                  L"CreateDepthStencilView"))
        return false;

    // 3D 메시용 : 깊이 테스트와 쓰기 모두 켠다.
    D3D11_DEPTH_STENCIL_DESC enabled = {};
    enabled.DepthEnable    = TRUE;
    enabled.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    enabled.DepthFunc      = D3D11_COMPARISON_LESS;          // 더 가까운 픽셀만 통과

    if (DX_FAILED(m_device->CreateDepthStencilState(&enabled, m_depthEnabledState.GetAddressOf()),
                  L"CreateDepthStencilState(enabled)"))
        return false;

    // 2D 스프라이트 / UI 용 : 깊이를 보지도 쓰지도 않는다.
    D3D11_DEPTH_STENCIL_DESC disabled = {};
    disabled.DepthEnable    = FALSE;
    disabled.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    disabled.DepthFunc      = D3D11_COMPARISON_ALWAYS;

    if (DX_FAILED(m_device->CreateDepthStencilState(&disabled, m_depthDisabledState.GetAddressOf()),
                  L"CreateDepthStencilState(disabled)"))
        return false;

    return true;
}

// =============================================================
// 3D 메시 파이프라인
// =============================================================
bool Graphics::CreateMeshPipeline()
{
    // ---- 상수 버퍼 ----
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth      = sizeof(TerrainConstantBuffer);   // 160 바이트 (16의 배수)
    cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (DX_FAILED(m_device->CreateBuffer(&cbDesc, nullptr, m_meshConstantBuffer.GetAddressOf()),
                  L"CreateBuffer(mesh constant)"))
        return false;

    // ---- 래스터라이저 상태 (S32) ----
    D3D11_RASTERIZER_DESC solid = {};
    solid.FillMode        = D3D11_FILL_SOLID;
    solid.CullMode        = D3D11_CULL_BACK;    // 뒷면은 그리지 않는다(정점 순서가 맞아야 한다)
    solid.FrontCounterClockwise = FALSE;        // 시계 방향이 앞면 (왼손 좌표계 기본)
    solid.DepthClipEnable = TRUE;

    if (DX_FAILED(m_device->CreateRasterizerState(&solid, m_rasterSolidState.GetAddressOf()),
                  L"CreateRasterizerState(solid)"))
        return false;

    D3D11_RASTERIZER_DESC wireframe = solid;
    wireframe.FillMode = D3D11_FILL_WIREFRAME;
    wireframe.CullMode = D3D11_CULL_NONE;       // 뒷면 삼각형 선도 보이게 둔다

    if (DX_FAILED(m_device->CreateRasterizerState(&wireframe, m_rasterWireframeState.GetAddressOf()),
                  L"CreateRasterizerState(wireframe)"))
        return false;

    // ---- 터레인 셰이더 ----
    Shader::Desc shaderDesc;
    shaderDesc.vsPath = Paths::Resolve(L"Shaders/TerrainVS.hlsl");
    shaderDesc.psPath = Paths::Resolve(L"Shaders/TerrainPS.hlsl");
    shaderDesc.vsEntry = "main";
    shaderDesc.psEntry = "main";
    shaderDesc.layout = TerrainVertex::kLayout;
    shaderDesc.layoutCount = TerrainVertex::kLayoutCount;
    shaderDesc.cacheCompiled = (SHADER_CACHE_ENABLED != 0);
    shaderDesc.useCache      = (SHADER_CACHE_ENABLED != 0);

    m_terrainShader = std::make_shared<Shader>();
    if (!m_terrainShader->Load(m_device.Get(), shaderDesc))
    {
        dxutil::DebugLog(L"[Graphics] 터레인 셰이더 로드 실패 :\n%s", m_terrainShader->GetLastError().c_str());
        return false;
    }

#if HOT_RELOAD_ENABLED
    ShaderManager::Get().Register("Terrain", m_terrainShader);
#endif

    return true;
}

float Graphics::GetAspectRatio() const
{
    if (m_height <= 0)
        return 1.0f;

    return static_cast<float>(m_width) / static_cast<float>(m_height);
}

void Graphics::SetCamera3D(FXMMATRIX view, CXMMATRIX projection, const XMFLOAT3& eyePosition)
{
    XMStoreFloat4x4(&m_view3D, view);
    XMStoreFloat4x4(&m_projection3D, projection);
    m_eyePosition = eyePosition;
}

void Graphics::DrawMesh(const Mesh& mesh,
                        FXMMATRIX world,
                        const XMFLOAT4& color,
                        const XMFLOAT4& params,
                        bool wireframe)
{
    if (!m_context || !mesh.IsValid() || !m_terrainShader || !m_terrainShader->IsValid())
        return;

    // 1) 상수 버퍼 갱신. HLSL 이 column-major 라 CPU 에서 전치해 넘긴다. (S12)
    const XMMATRIX view = XMLoadFloat4x4(&m_view3D);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projection3D);
    const XMMATRIX wvp = world * view * projection;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(m_context->Map(m_meshConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        TerrainConstantBuffer* cb = static_cast<TerrainConstantBuffer*>(mapped.pData);
        XMStoreFloat4x4(&cb->wvp, XMMatrixTranspose(wvp));
        XMStoreFloat4x4(&cb->world, XMMatrixTranspose(world));
        cb->color = color;
        cb->params = params;
        m_context->Unmap(m_meshConstantBuffer.Get(), 0);
    }

    // 2) 상태 설정
    m_context->OMSetDepthStencilState(m_depthEnabledState.Get(), 0);
    m_context->RSSetState(wireframe ? m_rasterWireframeState.Get() : m_rasterSolidState.Get());

    const UINT stride = mesh.GetStride();
    const UINT offset = 0;
    ID3D11Buffer* vertexBuffer = mesh.GetVertexBuffer();

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(mesh.GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);   // 32비트 인덱스

    m_terrainShader->Bind(m_context.Get());

    m_context->VSSetConstantBuffers(0, 1, m_meshConstantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_meshConstantBuffer.GetAddressOf());

    // 3) 그리기
    m_context->DrawIndexed(mesh.GetIndexCount(), 0, 0);
}
