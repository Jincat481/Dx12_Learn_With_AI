#pragma once
#include "Core/stdafx.h"

class Shader;

// =============================================================
// WaterRipples (S79, S81)
//  카메라 주변 정사각형 영역의 물결 높이를 GPU 에서 시뮬레이션한다.
//  컴퓨트 셰이더 없이 렌더 타깃(RTV) 핑퐁으로 계산한다.
//
//  높이 텍스처 세 장 (이전 / 현재 / 다음)
//   - 셋 다 RTV(쓰기) 와 SRV(읽기) 를 갖는다
//   - 한 단계 : "다음" 을 렌더 타깃으로 걸고, 이전 · 현재를 SRV 로 읽는 픽셀 셰이더로 화면을 채운다
//   - 끝나면 이름표만 한 칸씩 돌린다 (텍스처 복사 없음)
//  같은 텍스처를 읽기와 쓰기에 동시에 붙일 수 없으므로 세 장이 필요하다.
//
//  영역은 카메라를 따라간다. 옮길 때 텍스처를 복사하지 않고,
//  다음 단계에서 "옮긴 만큼 떨어진 텍셀" 을 읽게 해 물결이 월드에 붙어 있게 한다.
//
//  해안 마스크 (S81) : 땅인 곳을 알려 주면 그 텍셀을 벽으로 삼아 물결을 반사시킨다.
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

    // 월드 좌표에 물방울 하나를 예약한다. 다음 Step 에서 반영된다.
    void AddDrop(float worldX, float worldZ, float radius, float strength);

    // 해안 마스크 (1 = 땅). region : x 원점 X, y 원점 Z, z 한 변 길이. 텍스처 수명은 호출한 쪽이 관리한다.
    void SetShoreMask(ID3D11ShaderResourceView* mask, const DirectX::XMFLOAT3& region);

    // 영역 중심을 옮기고 한 단계 진행한다.
    void Step(ID3D11DeviceContext* context, float centerX, float centerZ);

    ID3D11ShaderResourceView* GetHeightSRV() const;

    // ---- 값 읽어 오기 (S82) ----
    //  Request : 지금 높이를 스테이징 텍스처로 복사하라고 명령만 넣는다 (기다리지 않는다)
    //  Poll    : 복사가 끝났으면 꺼내 온다. 아직이면 false 를 돌려주고 다음 프레임에 다시 본다
    void RequestReadback(ID3D11DeviceContext* context);
    bool PollReadback(ID3D11DeviceContext* context, std::vector<float>& outHeights, DirectX::XMFLOAT3& outRegion);
    bool IsReadbackPending() const { return m_readPending; }

    // x : 영역 원점 X   y : 영역 원점 Z   z : 한 변의 월드 길이
    DirectX::XMFLOAT3 GetRegion() const;
    int    GetSize() const { return m_size; }
    size_t GetQueuedDrops() const { return m_drops.size(); }

private:
    struct Drop
    {
        float x;
        float z;
        float radius;
        float strength;
    };

    ComPtr<ID3D11Texture2D>          m_textures[3];
    ComPtr<ID3D11RenderTargetView>   m_targets[3];
    ComPtr<ID3D11ShaderResourceView> m_views[3];

    int m_previous = 0;
    int m_current = 1;
    int m_next = 2;

    std::shared_ptr<Shader>       m_shader;
    ComPtr<ID3D11Buffer>          m_constants;
    ComPtr<ID3D11RasterizerState> m_rasterizer;
    ComPtr<ID3D11SamplerState>    m_linearSampler;

    ID3D11ShaderResourceView* m_shoreMask = nullptr;
    DirectX::XMFLOAT3         m_shoreRegion{ 0.0f, 0.0f, 1.0f };

    int   m_size = 512;
    float m_worldSize = 384.0f;
    float m_texelSize = 0.75f;
    float m_damping = 0.992f;

    int  m_originTexelX = 0;       // "현재" 텍스처가 쓰일 때의 영역 원점
    int  m_originTexelZ = 0;
    int  m_previousOriginX = 0;    // "이전" 텍스처가 쓰일 때의 영역 원점
    int  m_previousOriginZ = 0;
    bool m_hasOrigin = false;

    std::vector<Drop> m_drops;

    ComPtr<ID3D11Texture2D> m_staging;           // CPU 가 Map 할 수 있는 복사본
    DirectX::XMFLOAT3       m_readRegion{ 0.0f, 0.0f, 1.0f };
    bool                    m_readPending = false;

    static constexpr int    kMaxDropsPerStep = 4;
    static constexpr size_t kMaxQueuedDrops = 32;
    static constexpr int    kRecenterStep = 16;   // 영역은 16 텍셀 단위로만 옮긴다
};
