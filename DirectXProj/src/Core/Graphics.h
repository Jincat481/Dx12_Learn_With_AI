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

    // 3D 메시를 그릴 때 셰이더로 넘길 값들
    struct MeshDrawParams
    {
        DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 params{ 0.0f, 0.0f, 0.0f, 0.0f };        // x,y 격자 칸 수 / z 와이어프레임 / w 높이 사용
        DirectX::XMFLOAT4 heightRange{ 0.0f, 1.0f, 0.0f, 0.0f };   // 색상 램프용 최저/최고 높이
        DirectX::XMFLOAT4 lightDirection{ -0.45f, -1.0f, 0.35f, 0.28f };  // xyz 방향 / w 환경광
        DirectX::XMFLOAT4 splat{ 24.0f, 0.0f, 0.0f, 0.0f };               // x 타일 / y 스플래팅 / z 디버그색 / w 모프
        DirectX::XMFLOAT4 lodSelect{ 0.0f, 0.0f, 0.0f, 0.0f };            // 현재 LOD 성분만 1
        DirectX::XMFLOAT4 surface{ 0.0f, 12.0f, 4.0f, 0.0f };             // x 트라이플래너 / y 타일 월드 크기 / z 날카로움
        DirectX::XMFLOAT4 brush{ 0.0f, 0.0f, 0.0f, 0.0f };                // xy 중심 / z 반경 / w 도구+1 (0 끔)
        bool wireframe = false;

        // 인덱스 버퍼의 일부만 그릴 때 사용한다(청크 LOD). count 가 0 이면 메시 전체.
        UINT indexOffset = 0;
        UINT indexCount = 0;

        // 스플래팅 레이어 : 흙 / 풀 / 바위 / 눈 (t0~t3)
        ID3D11ShaderResourceView* layers[4] = { nullptr, nullptr, nullptr, nullptr };
    };

    // 3D 메시 하나를 그린다. 깊이 테스트가 켜진 상태로 그려진다.
    void DrawMesh(const Mesh& mesh, DirectX::FXMMATRIX world, const MeshDrawParams& drawParams);

    // ---- 테셀레이션 지형 (스텝 7) ----
    struct TessDrawParams
    {
        DirectX::XMFLOAT4 tess{ 1.0f, 16.0f, 800.0f, 1.0f };
        DirectX::XMFLOAT4 lightDirection{ -0.45f, -1.0f, 0.35f, 0.28f };
        DirectX::XMFLOAT4 heightRange{ 0.0f, 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 params{ 0.0f, 1024.0f, 1.0f / 512.0f, 0.0f };
        ID3D11ShaderResourceView* heightMap = nullptr;
        bool wireframe = false;
    };

    // 제어점 4개짜리 패치 목록을 그린다. 실제 삼각형은 GPU 가 만든다.
    void DrawTessellatedPatches(const Mesh& mesh, DirectX::FXMMATRIX world, const TessDrawParams& params);

    // ---- 하늘 (스텝 8, 9) ----
    struct SkyDrawParams
    {
        DirectX::XMFLOAT4 horizonColor{ 0.62f, 0.72f, 0.86f, 1.0f };
        DirectX::XMFLOAT4 zenithColor{ 0.12f, 0.30f, 0.62f, 1.0f };
        DirectX::XMFLOAT4 sunDirection{ -0.45f, -1.0f, 0.35f, 0.0f };
        DirectX::XMFLOAT4 params{ 0.0f, 0.0f, 0.5f, 0.02f };
    };

    // 하늘은 깊이에 쓰지 않고 가장 먼저 그린다.
    void DrawSky(const Mesh& mesh, DirectX::FXMMATRIX world, const SkyDrawParams& params);

    // ---- 대기 : 거리 안개 (S64) ----
    //  안개는 지형 하나가 아니라 "그 씬의 공기" 라 Graphics 가 한 벌만 들고 있다.
    //  하늘(SkyRenderer)이 Update 에서 채우고, 메시·테셀레이션을 그릴 때 모두 같은 값을 쓴다.
    //  EndFrame 에서 꺼지므로 하늘이 없는 씬으로 넘어가도 안개가 남지 않는다.
    struct FogSettings
    {
        DirectX::XMFLOAT4 color{ 0.62f, 0.72f, 0.86f, 0.0f };        // rgb 색 / a 켜짐
        DirectX::XMFLOAT4 params{ 150.0f, 900.0f, 0.002f, 0.45f };   // x 시작 / y 끝 / z 밀도 / w 태양 산란
    };

    void SetFog(const FogSettings& fog) { m_fog = fog; }
    const FogSettings& GetFog() const { return m_fog; }

    // ---- 렌더 패스 (S76) ----
    //  같은 씬을 한 프레임에 여러 번 그린다. 지금 무엇을 그리는 중인지 컴포넌트가 물어볼 수 있다.
    enum class RenderPass { Main, Reflection };
    RenderPass GetRenderPass() const { return m_renderPass; }

    // 반사 패스 : 반사 텍스처를 렌더 타깃으로 걸고, 카메라를 수면 기준으로 뒤집고,
    //  수면 아래를 잘라 내는 평면을 켠다. End 에서 모두 되돌린다.
    bool BeginReflectionPass(float waterLevel);
    void EndReflectionPass();

    // ---- 물 (S76~S78) ----
    struct WaterDrawParams
    {
        DirectX::XMFLOAT4 shallowColor{ 0.08f, 0.38f, 0.40f, 1.0f };
        DirectX::XMFLOAT4 deepColor{ 0.010f, 0.050f, 0.085f, 18.0f };   // a : 완전히 깊어지는 두께
        DirectX::XMFLOAT4 wave{ 1.0f, 0.35f, 1.0f, 0.03f };            // 파장 배율 / 법선 세기 / 속도 / 굴절 왜곡
        DirectX::XMFLOAT4 lightDirection{ -0.45f, -1.0f, 0.35f, 0.0f };
        float time = 0.0f;
        bool  reflection = true;
        bool  refraction = true;
        bool  foam = true;

        // 바다 파도 (S85~S87)
        ID3D11ShaderResourceView* oceanDisplacement[3] = { nullptr, nullptr, nullptr };
        ID3D11ShaderResourceView* oceanSlope[3] = { nullptr, nullptr, nullptr };
        DirectX::XMFLOAT3 oceanTiles{ 400.0f, 71.0f, 13.0f };
        float significantHeight = 1.0f;
        float rippleHeightScale = 0.25f;  // 클릭 물결 시뮬레이션 값 → 수면 높이(m)
        float shoreCalmDepth = 10.0f;     // 이만큼 깊어야 파도가 다 살아난다
        float whitecaps = 1.0f;

        // 클릭 물결 (S79)
        ID3D11ShaderResourceView* rippleHeight = nullptr;
        DirectX::XMFLOAT4 rippleRegion{ 0.0f, 0.0f, 1.0f, 0.0f };   // xy 원점 / z 크기 / w 법선 세기
        float rippleTexel = 1.0f / 512.0f;
        bool  rippleDebug = false;       // 물결 높이를 색으로 보기

        // 해안 마스크 (S81) : 디버그 보기에서 벽으로 쓰는 땅 영역을 노랗게 칠한다
        ID3D11ShaderResourceView* shoreMask = nullptr;
        DirectX::XMFLOAT4 shoreRegion{ 0.0f, 0.0f, 1.0f, 0.0f };
    };

    // 불투명 결과를 복사해 두고(한 프레임에 한 번) 수면을 그린다.
    void DrawWater(const Mesh& mesh, DirectX::FXMMATRIX world, const WaterDrawParams& params);

    // ---- 물결 높이 텍스처 보기 (S82) ----
    //  화면의 정사각형 창 하나에 높이 텍스처를 색으로 그린다. GDI 오버레이보다 먼저 불러야 한다.
    struct RippleViewParams
    {
        ID3D11ShaderResourceView* height = nullptr;
        ID3D11ShaderResourceView* shore = nullptr;
        DirectX::XMFLOAT4 region{ 0.0f, 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4 shoreRegion{ 0.0f, 0.0f, 1.0f, 0.0f };   // w : 있음
        DirectX::XMFLOAT4 marker{ 0.0f, 0.0f, 0.0f, 0.0f };        // xy 점 / z 단면 줄 / w 있음
        float amplitudeScale = 1.0f;
        int   x = 0;
        int   y = 0;
        int   size = 256;
    };
    void DrawRippleView(const RippleViewParams& params);

    // ---- 피킹 (3D, S67) ----
    //  화면 좌표 → 월드 공간의 레이. 같은 화면 점을 가장 가까운 깊이(0)와
    //  가장 먼 깊이(1)로 되돌린 두 점을 이으면 그 픽셀을 지나는 시선이 된다.
    bool ScreenToRay(int screenX, int screenY, DirectX::XMFLOAT3& outOrigin, DirectX::XMFLOAT3& outDirection) const;

    float GetAspectRatio() const;
    DirectX::XMFLOAT3 GetEyePosition3D() const { return m_eyePosition; }
    DirectX::XMMATRIX GetView3D() const       { return DirectX::XMLoadFloat4x4(&m_view3D); }
    DirectX::XMMATRIX GetProjection3D() const { return DirectX::XMLoadFloat4x4(&m_projection3D); }

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
    bool CreateSkyPipeline();        // 하늘 셰이더와 상수 버퍼
    bool CreateTessPipeline();       // 테셀레이션 셰이더와 상수 버퍼
    bool CreatePassTargets();        // 반사 렌더 타깃, 화면 색 / 깊이 복사본 (S76, S77)
    bool CreateWaterPipeline();      // 물 셰이더와 상수 버퍼
    bool CreateRippleViewPipeline(); // 물결 텍스처 보기 (S82)
    void CaptureOpaqueScene();       // 불투명까지 그린 화면 색과 깊이를 복사한다

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
    ComPtr<ID3D11DepthStencilState> m_depthSkyState;        // 하늘 : 테스트만 하고 쓰지 않는다

    // ---- 3D 메시 파이프라인 ----
    ComPtr<ID3D11Buffer>            m_meshConstantBuffer;   // b0 : WVP + World + color + params
    ComPtr<ID3D11RasterizerState>   m_rasterSolidState;     // 뒷면 컬링
    ComPtr<ID3D11RasterizerState>   m_rasterWireframeState; // 와이어프레임
    ComPtr<ID3D11SamplerState>      m_wrapSamplerState;     // 지형 텍스처 타일링용 (WRAP)
    std::shared_ptr<Shader>         m_terrainShader;

    ComPtr<ID3D11Buffer>            m_skyConstantBuffer;
    std::shared_ptr<Shader>         m_skyShader;

    ComPtr<ID3D11Buffer>            m_tessConstantBuffer;
    std::shared_ptr<Shader>         m_tessShader;

    DirectX::XMFLOAT4X4 m_view3D{};
    DirectX::XMFLOAT4X4 m_projection3D{};
    DirectX::XMFLOAT3   m_eyePosition{ 0.0f, 0.0f, 0.0f };
    FogSettings         m_fog;             // 하늘이 매 프레임 채우고 EndFrame 에서 끈다

    // ---- 렌더 패스 (S76, S77) ----
    RenderPass          m_renderPass = RenderPass::Main;
    DirectX::XMFLOAT4   m_clipPlane{ 0.0f, 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT4X4 m_savedView3D{};
    DirectX::XMFLOAT3   m_savedEyePosition{ 0.0f, 0.0f, 0.0f };

    ComPtr<ID3D11Texture2D>          m_reflectionTexture;
    ComPtr<ID3D11RenderTargetView>   m_reflectionRTV;
    ComPtr<ID3D11ShaderResourceView> m_reflectionSRV;
    ComPtr<ID3D11Texture2D>          m_reflectionDepth;
    ComPtr<ID3D11DepthStencilView>   m_reflectionDSV;

    ComPtr<ID3D11Texture2D>          m_sceneColorCopy;
    ComPtr<ID3D11ShaderResourceView> m_sceneColorSRV;
    ComPtr<ID3D11Texture2D>          m_sceneDepthCopy;
    ComPtr<ID3D11ShaderResourceView> m_sceneDepthSRV;
    bool                             m_sceneCaptured = false;

    ComPtr<ID3D11Buffer>             m_waterConstantBuffer;
    std::shared_ptr<Shader>          m_waterShader;
    ComPtr<ID3D11SamplerState>       m_oceanSampler;   // 파도 텍스처 : 반복 + 비등방 (S85)

    ComPtr<ID3D11Buffer>             m_rippleViewConstantBuffer;
    std::shared_ptr<Shader>          m_rippleViewShader;

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
