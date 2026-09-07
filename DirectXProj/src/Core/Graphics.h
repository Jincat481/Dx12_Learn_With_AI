#pragma once
#include "Core/stdafx.h"
#include "Graphics/Vertex.h"

class Shader;

// =============================================================
// Graphics (과제 1, 2, 6 / S03, S09, S12)
//  - D3D11 디바이스 / 스왑 체인 / 렌더 타깃 뷰 / 뷰포트 초기화
//  - BeginFrame : 화면을 파란색으로 Clear
//  - EndFrame   : Present
//  - DrawSprite : 정점/인덱스 버퍼, 셰이더, 상수 버퍼, 샘플러를 묶고 DrawIndexed(6)
// =============================================================
class Graphics
{
public:
    Graphics() = default;
    ~Graphics();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // 스프라이트 한 장을 그린다. world 는 Transform 의 월드 행렬.
    //  silhouette == true 면 텍스처의 알파만 사용해 color 단색으로 채운다(선택 테두리용).
    void DrawSprite(ID3D11Buffer* vertexBuffer,
                    ID3D11Buffer* indexBuffer,
                    UINT indexCount,
                    ID3D11ShaderResourceView* srv,
                    DirectX::FXMMATRIX world,
                    const DirectX::XMFLOAT4& color,
                    bool silhouette = false);

    // ---- 피킹 (2D) ----
    // 클라이언트 좌표(마우스) → z = 0 평면의 월드 좌표.
    // 직교 투영이라 x, y 는 깊이와 무관하게 결정된다.
    DirectX::XMFLOAT3 ScreenToWorld(int screenX, int screenY) const;

    DirectX::XMMATRIX GetViewMatrix() const       { return DirectX::XMLoadFloat4x4(&m_viewMatrix); }
    DirectX::XMMATRIX GetProjectionMatrix() const { return DirectX::XMLoadFloat4x4(&m_projectionMatrix); }

    // ---- GDI 오버레이 ----
    //  백버퍼를 GDI 호환으로 만들어 두고 DC 를 빌려 그 위에 직접 그린다.
    //  Hierarchy 패널 같은 디버그 UI 용. 반드시 씬 Render 이후 ~ Present 이전에 사용한다.
    //  BeginOverlay 와 EndOverlay 사이에는 D3D 렌더링을 하지 않는다.
    HDC  BeginOverlay();
    void EndOverlay();

    ID3D11Device*        GetDevice()  const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

    int GetWidth()  const { return m_width; }
    int GetHeight() const { return m_height; }

    void SetClearColor(float r, float g, float b, float a);

private:
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateRenderTargetView();
    bool CreateSpritePipeline();     // 셰이더, 상수 버퍼, 샘플러, 블렌드 상태

    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<IDXGISurface1>          m_backBufferSurface;   // GDI 오버레이용
    bool                           m_overlayActive = false;

    ComPtr<ID3D11Buffer>           m_constantBuffer;   // b0 : WVP + color
    ComPtr<ID3D11SamplerState>     m_samplerState;     // s0
    ComPtr<ID3D11BlendState>       m_blendState;       // 알파 블렌딩
    ComPtr<ID3D11RasterizerState>  m_rasterizerState;

    std::shared_ptr<Shader>        m_spriteShader;

    DirectX::XMFLOAT4X4 m_viewMatrix{};
    DirectX::XMFLOAT4X4 m_projectionMatrix{};

    float m_clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };   // 과제 1 : 파란색 화면
    int   m_width = 0;
    int   m_height = 0;
};
