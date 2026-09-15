#pragma once
#include "Core/stdafx.h"
#include "Engine/WaveParticles.h"

class Shader;

// =============================================================
// WaterRipples (S79, S81, S89)
//  카메라 주변 정사각형 영역의 물결 높이 텍스처를 만든다.
//
//  S79 : 격자 파동 방정식을 렌더 타깃 핑퐁으로 풀었다. 선형이라 마주 오는 물결이 서로 통과했다.
//  S89 : 물결을 방향과 세기를 가진 입자(WaveParticles)로 바꿨다. 부딪힌 물결은 서로 힘을 깎는다.
//        계산은 CPU 에서 하고, 입자를 높이 텍스처(RTV)에 가산 블렌드로 그린다.
//
//  높이 텍스처의 쓰임새(수면 정점 · 법선 · 진폭 창)는 그대로다.
//
//  영역은 카메라를 따라 16 텍셀 단위로 옮긴다. 입자는 월드 좌표에 있으므로 옮겨도 물결이 밀리지 않는다.
//  해안 (S81) : 땅 높이 맵을 받아 입자가 땅에 닿으면 반사시킨다.
// =============================================================
class WaterRipples
{
public:
    WaterRipples() = default;
    ~WaterRipples() = default;

    WaterRipples(const WaterRipples&) = delete;
    WaterRipples& operator=(const WaterRipples&) = delete;

    bool Initialize(ID3D11Device* device, int size, float worldSize);
    void Release();
    bool IsValid() const;

    // 월드 좌표에 물방울 하나. 반경(m)과 세기
    void AddDrop(float worldX, float worldZ, float radius, float strength);

    // 해안 높이 맵 (size x size, "땅 높이 - 수위"). region : x 원점 X, y 원점 Z, z 한 변 길이
    void SetShore(const std::vector<float>& heights, int size, const DirectX::XMFLOAT3& region);

    // 영역 중심을 옮기고 입자를 한 단계 진행한다 (CPU)
    void Step(float centerX, float centerZ);

    // 지금 입자들을 높이 텍스처에 그린다 (GPU). 한 프레임에 한 번이면 된다
    void Render(ID3D11DeviceContext* context);

    ID3D11ShaderResourceView* GetHeightSRV() const { return m_view.Get(); }

    // ---- 값 읽어 오기 (S82) ----
    //  Request : 지금 높이를 스테이징 텍스처로 복사하라고 명령만 넣는다 (기다리지 않는다)
    //  Poll    : 복사가 끝났으면 꺼내 온다. 아직이면 false 를 돌려주고 다음 프레임에 다시 본다
    void RequestReadback(ID3D11DeviceContext* context);
    bool PollReadback(ID3D11DeviceContext* context, std::vector<float>& outHeights, DirectX::XMFLOAT3& outRegion);
    bool IsReadbackPending() const { return m_readPending; }

    // x : 영역 원점 X   y : 영역 원점 Z   z : 한 변의 월드 길이
    DirectX::XMFLOAT3 GetRegion() const;
    int GetSize() const { return m_size; }

    int GetParticleCount() const { return static_cast<int>(m_waves.GetParticles().size()); }
    int GetCollidingCount() const { return m_waves.GetCollidingCount(); }

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11RenderTargetView>   m_target;
    ComPtr<ID3D11ShaderResourceView> m_view;
    DXGI_FORMAT                      m_format = DXGI_FORMAT_R32_FLOAT;

    std::shared_ptr<Shader>       m_shader;
    ComPtr<ID3D11Buffer>          m_constants;
    ComPtr<ID3D11RasterizerState> m_rasterizer;
    ComPtr<ID3D11BlendState>      m_additive;

    // 입자 목록을 GPU 로 : 구조체 버퍼 + SRV. 정점 셰이더가 SV_InstanceID 로 하나씩 읽는다
    ComPtr<ID3D11Buffer>             m_particleBuffer;
    ComPtr<ID3D11ShaderResourceView> m_particleView;
    UINT                             m_particleCapacity = 0;
    UINT                             m_renderedCount = 0;

    WaveParticles m_waves;

    int   m_size = 512;
    float m_worldSize = 384.0f;
    float m_texelSize = 0.75f;

    int  m_originTexelX = 0;
    int  m_originTexelZ = 0;

    ComPtr<ID3D11Texture2D> m_staging;           // CPU 가 Map 할 수 있는 복사본
    DirectX::XMFLOAT3       m_readRegion{ 0.0f, 0.0f, 1.0f };
    bool                    m_readPending = false;

    static constexpr int   kRecenterStep = 16;          // 영역은 16 텍셀 단위로만 옮긴다
    static constexpr float kStrengthToHeight = 1.5f;    // 물방울 세기 → 처음 고리 높이
    static constexpr float kBoundsMargin = 48.0f;       // 영역 밖으로 이만큼 나가면 입자를 지운다
};
