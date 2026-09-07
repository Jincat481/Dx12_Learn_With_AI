#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"
#include "Terrain/HeightField.h"

class Graphics;

// =============================================================
// TessellatedTerrainRenderer (스텝 7 / S59~S62)
//  하드웨어 테셀레이션으로 지형을 그린다.
//
//  스텝 6 의 LOD 는 미리 만들어 둔 인덱스 중에서 하나를 고르는 방식이었다.
//  여기서는 **정점 자체를 GPU 가 그 자리에서 만들어 낸다.**
//
//   정점 버퍼에는 성긴 제어점만 넣는다 (예 : 32 x 32 패치)
//        ↓ Hull   : 이 패치를 몇 등분할지 정한다 (카메라 거리 기준)
//        ↓ 테셀레이터 (고정 기능)
//        ↓ Domain : 쪼개진 점마다 위치를 만들고 높이맵으로 올린다
//
//  높이가 CPU 함수가 아니라 텍스처여야 하는 이유가 여기 있다.
//  정점이 GPU 에서 생기므로 높이도 GPU 가 읽을 수 있어야 한다.
// =============================================================
class TessellatedTerrainRenderer : public Component
{
public:
    TessellatedTerrainRenderer() = default;
    ~TessellatedTerrainRenderer() override = default;

    const char* GetTypeName() const override { return "TessellatedTerrainRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void Render() override;
    void OnDestroy() override;

    void SetGrid(int patchesX, int patchesZ, float patchSize);
    void SetHeightParams(const terrain::HeightParams& params);
    void Regenerate(unsigned seed);

    void SetWireframe(bool wireframe) { m_wireframe = wireframe; }
    bool IsWireframe() const { return m_wireframe; }
    void ToggleWireframe() { m_wireframe = !m_wireframe; }

    // 분할 계수 범위. 최대값을 올리면 가까운 곳이 더 촘촘해진다.
    void SetTessellationRange(float minFactor, float maxFactor);
    float GetMinFactor() const { return m_minFactor; }
    float GetMaxFactor() const { return m_maxFactor; }
    void AdjustMaxFactor(float delta);

    void SetDistanceRange(float distance) { m_distanceRange = distance; }

    int GetPatchCount() const { return m_patchesX * m_patchesZ; }

private:
    bool BuildPatchGrid();
    bool BuildHeightTexture();

    Graphics* m_graphics = nullptr;
    Mesh m_patches;

    terrain::HeightField m_height;

    ComPtr<ID3D11Texture2D>          m_heightTexture;
    ComPtr<ID3D11ShaderResourceView> m_heightSRV;

    int   m_patchesX = 32;
    int   m_patchesZ = 32;
    float m_patchSize = 32.0f;      // 패치 한 변의 월드 길이

    int   m_heightMapSize = 512;    // 높이 텍스처 해상도

    float m_minFactor = 1.0f;
    float m_maxFactor = 24.0f;
    float m_distanceRange = 900.0f;

    float m_minHeight = 0.0f;
    float m_maxHeight = 1.0f;

    bool m_wireframe = false;
    bool m_dirty = true;
};
