#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"
#include "Terrain/HeightField.h"
#include "Engine/GroundProvider.h"

class Graphics;
class ComputeShader;

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
//
//  에디터 연결 (S71, S72)
//   그 높이 텍스처를 CPU 에서 26만 번 샘플링해 올리는 대신
//   컴퓨트 셰이더로 GPU 에서 바로 만들 수 있다. U 로 CPU / GPU 를 바꿔 시간을 비교한다.
// =============================================================
class TessellatedTerrainRenderer : public Component, public IGroundProvider
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

    // ---- 에디터 연결 : 컴퓨트 셰이더로 높이맵 생성 (S71, S72) ----
    void SetGpuGeneration(bool enabled) { m_useGpuGeneration = enabled; m_dirty = true; }
    bool IsGpuGeneration() const { return m_useGpuGeneration; }
    void ToggleGpuGeneration() { SetGpuGeneration(!m_useGpuGeneration); }
    bool IsGpuGenerationAvailable() const;

    // 256 → 512 → 1024 → 2048 → 256 ...
    void CycleHeightMapSize();
    int  GetHeightMapSize() const { return m_heightMapSize; }

    // 마지막으로 잰 시간 (아직 안 쟀으면 음수)
    float GetLastCpuMs() const { return m_lastCpuMs; }
    float GetLastGpuMs() const { return m_lastGpuMs; }
    float GetLastReadbackMs() const { return m_lastReadbackMs; }
    float GetMinHeight() const { return m_minHeight; }
    float GetMaxHeight() const { return m_maxHeight; }

    // ---- 보강 : 지면 높이 (S66) ----
    bool TryGetGroundHeight(float x, float z, float& outHeight) const override;

private:
    bool BuildPatchGrid();
    bool BuildHeightTexture();
    bool EnsureHeightTexture(int size);
    void GenerateOnCpu();
    bool GenerateOnGpu();

    Graphics* m_graphics = nullptr;
    Mesh m_patches;

    terrain::HeightField m_height;

    ComPtr<ID3D11Texture2D>          m_heightTexture;
    ComPtr<ID3D11ShaderResourceView> m_heightSRV;
    ComPtr<ID3D11UnorderedAccessView> m_heightUAV;    // 컴퓨트 셰이더가 쓰는 쪽 (S71)
    int m_heightTextureSize = 0;

    // GPU 생성 (S71, S72)
    std::shared_ptr<ComputeShader> m_heightGen;
    ComPtr<ID3D11Buffer>    m_genConstants;
    ComPtr<ID3D11Texture2D> m_stagingTexel;           // 측정용 동기화 : 텍셀 하나만 읽어 온다
    ComPtr<ID3D11Texture2D> m_stagingFull;            // 높이 범위를 알기 위해 전체를 읽어 온다
    bool  m_useGpuGeneration = true;
    float m_lastCpuMs = -1.0f;
    float m_lastGpuMs = -1.0f;
    float m_lastReadbackMs = -1.0f;
    bool  m_gpuWarmedUp = false;                      // 첫 Dispatch 는 재지 않고 한 번 돌려 둔다

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
