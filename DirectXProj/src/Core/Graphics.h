#pragma once
#include "Core/stdafx.h"
#include "Graphics/Vertex.h"

class Shader;
class Mesh;

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

    // ---- 3D (터레인) ----
    //  Camera 컴포넌트가 Update 에서 넣어 준 행렬을 Render 단계에서 쓴다.
    void SetCamera3D(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection, const DirectX::XMFLOAT3& eyePosition);

    // 3D 메시 하나를 그린다. 깊이 테스트가 켜진 상태로 그려진다.
    void DrawMesh(const Mesh& mesh,
                  DirectX::FXMMATRIX world,
                  const DirectX::XMFLOAT4& color,
                  const DirectX::XMFLOAT4& params,
                  bool wireframe);

    float GetAspectRatio() const;
    DirectX::XMFLOAT3 GetEyePosition3D() const { return m_eyePosition; }

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
    bool CreateDepthBuffer();        // 깊이 버퍼 + 깊이 상태 (S29)
    bool CreateMeshPipeline();       // 터레인 셰이더, 상수 버퍼, 래스터라이저 상태

    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<IDXGISurface1>          m_backBufferSurface;   // GDI 오버레이용
    bool                           m_overlayActive = false;

    // ---- 깊이 버퍼 (S29) ----
    ComPtr<ID3D11Texture2D>         m_depthStencilTexture;
    ComPtr<ID3D11DepthStencilView>  m_depthStencilView;
    ComPtr<ID3D11DepthStencilState> m_depthEnabledState;    // 3D 메시용
    ComPtr<ID3D11DepthStencilState> m_depthDisabledState;   // 2D 스프라이트용

    // ---- 3D 메시 파이프라인 ----
    ComPtr<ID3D11Buffer>            m_meshConstantBuffer;   // b0 : WVP + World + color + params
    ComPtr<ID3D11RasterizerState>   m_rasterSolidState;     // 뒷면 컬링
    ComPtr<ID3D11RasterizerState>   m_rasterWireframeState; // 와이어프레임
    std::shared_ptr<Shader>         m_terrainShader;

    DirectX::XMFLOAT4X4 m_view3D{};
    DirectX::XMFLOAT4X4 m_projection3D{};
    DirectX::XMFLOAT3   m_eyePosition{ 0.0f, 0.0f, 0.0f };

    ComPtr<ID3D11Buffer>           m_constantBuffer;   // b0 : WVP + color
    ComPtr<ID3D11SamplerState>     m_samplerState;     // s0
    ComPtr<ID3D11BlendState>       m_blendState;       // 알파 블렌딩
    ComPtr<ID3D11RasterizerState>  m_rasterizerState;

    std::shared_ptr<Shader>        m_spriteShader;

    DirectX::XMFLOAT4X4 m_viewMatrix{};
    DirectX::XMFLOAT4X4 m_projectionMatrix{};

    float m_clearColor[4] = { 0.07f, 0.10f, 0.16f, 1.0f };   // 터레인 배경(어두운 남색)
    int   m_width = 0;
    int   m_height = 0;
};
