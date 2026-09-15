#pragma once
#include "Core/stdafx.h"

class Shader;

// =============================================================
// OceanWaves (S85, S86)
//  바람이 만든 바다 파도를 GPU 텍스처로 계산한다.
//
//  1) 스펙트럼 (CPU, 바람이 바뀔 때만)
//     JONSWAP 스펙트럼(북해 관측으로 만든 식)으로 "이 바람이면 어떤 파장의 파도에 에너지가 얼마나 있는가" 를 구한다.
//     파장대를 로그 간격으로 나눠 구간마다 파도를 뽑고, 방향은 바람 쪽으로 모이게 흩뿌린다.
//     멀리서 밀려온 긴 너울도 따로 더한다. 전체 파고는 실측 공식에 맞춘다.
//         유의파고 Hs ≈ 0.21 · V² / g        (V : 풍속, 완전히 발달한 바다 - 피어슨-모스코위츠)
//
//  2) 캐스케이드 3단
//     너울(수백 m)과 잔물결(1 m)을 한 텍스처에 담으면 해상도가 모자란다.
//     크기가 다른 타일 세 장에 파장대를 나눠 담고 수면에서 합친다.
//         800 m : 40 ~ 800 m 파장     71 m : 7 ~ 40 m     13 m : 1.3 ~ 7 m
//     타일 크기를 서로 나누어떨어지지 않게 골라 반복 무늬가 겹쳐 보이지 않게 한다.
//
//  3) 매 프레임 (GPU, 렌더 타깃)
//     타일마다 변위 · 기울기 텍스처를 MRT 로 그린다. 거품은 지난 프레임을 이어받아야 하므로
//     두 벌을 번갈아 쓰고, 끝나면 밉맵을 만들어 멀리서 반짝이지 않게 한다.
// =============================================================
class OceanWaves
{
public:
    static constexpr int kCascadeCount = 3;
    static constexpr int kMaxWaves = 64;
    static constexpr int kWavesPerCascade = 48;
    static constexpr int kTextureSize = 256;

    OceanWaves() = default;
    ~OceanWaves() = default;

    OceanWaves(const OceanWaves&) = delete;
    OceanWaves& operator=(const OceanWaves&) = delete;

    bool Initialize(ID3D11Device* device);
    void Release();
    bool IsValid() const;

    // 풍속(m/s)과 바람이 불어 가는 방향(도, +X 가 0 · +Z 가 90)
    void  SetWind(float speed, float directionDegrees);
    float GetWindSpeed() const { return m_windSpeed; }
    float GetWindDirection() const { return m_windDirection; }

    // 0 이면 사인파(둥근 마루), 1 이면 게르스트너(뾰족한 마루)
    void  SetChoppiness(float choppiness) { m_choppiness = choppiness; }
    float GetChoppiness() const { return m_choppiness; }

    float GetSignificantHeight() const { return m_significantHeight; }
    float GetPeakWavelength() const;     // 가장 에너지가 큰 파장(m)

    void Update(ID3D11DeviceContext* context, float time, float deltaTime);

    ID3D11ShaderResourceView* GetDisplacementSRV(int cascade) const;
    ID3D11ShaderResourceView* GetSlopeSRV(int cascade) const;
    float GetTileSize(int cascade) const;

private:
    struct Wave
    {
        float kx;
        float kz;
        float amplitude;
        float phase;
    };

    struct Cascade
    {
        float tile = 1.0f;
        float minWavelength = 1.0f;
        float maxWavelength = 1.0f;
        std::vector<Wave> waves;

        ComPtr<ID3D11Texture2D>          displacement[2];
        ComPtr<ID3D11RenderTargetView>   displacementRTV[2];
        ComPtr<ID3D11ShaderResourceView> displacementSRV[2];
        ComPtr<ID3D11Texture2D>          slope[2];
        ComPtr<ID3D11RenderTargetView>   slopeRTV[2];
        ComPtr<ID3D11ShaderResourceView> slopeSRV[2];
    };

    void BuildSpectrum();

    Cascade m_cascades[kCascadeCount];
    int     m_current = 0;          // 지금 읽는 쪽 (0 / 1)

    std::shared_ptr<Shader>       m_shader;
    ComPtr<ID3D11Buffer>          m_constants;
    ComPtr<ID3D11RasterizerState> m_rasterizer;

    float m_windSpeed = 8.0f;
    float m_windDirection = 60.0f;
    float m_choppiness = 1.0f;
    float m_significantHeight = 0.0f;
    bool  m_dirty = true;

    static constexpr float kFoamLifetime = 1.2f;    // 거품이 1/e 로 줄어드는 시간(초)
    float m_choppinessLimit = 1.0f;                 // 이보다 뾰족하게 밀면 면이 접힌다 (스펙트럼에서 계산)
    static constexpr float kFoamGain = 3.0f;
    float m_foamThreshold = 0.7f;                   // 야코비안이 이보다 작으면 거품 (바람이 셀수록 커진다)
};
