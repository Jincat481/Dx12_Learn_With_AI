#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Mesh.h"
#include "Terrain/TerrainMeshBuilder.h"
#include "Terrain/HeightField.h"

class Graphics;

// =============================================================
// TerrainRenderer (스텝 1 : 평면 그리드)
//  격자 설정을 들고 있다가 Initialize 에서 메시를 만들고 매 프레임 그린다.
//
//  스텝 2 이후에 하이트맵을 넣더라도 이 클래스의 구조는 그대로 두고
//  TerrainMeshBuilder 가 채우는 y 값과 normal 만 달라지면 된다.
// =============================================================
class TerrainRenderer : public Component
{
public:
    TerrainRenderer() = default;
    ~TerrainRenderer() override = default;

    const char* GetTypeName() const override { return "TerrainRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Update() override;
    void Render() override;
    void OnDestroy() override;

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

    // 설정을 바꾸면 다음 프레임에 메시를 다시 만든다.
    void SetGrid(int cellsX, int cellsZ, float cellSize);
    const terrain::GridDesc& GetGridDesc() const { return m_desc; }

    void SetWireframe(bool wireframe) { m_wireframe = wireframe; }
    bool IsWireframe() const { return m_wireframe; }

    // ---- 스텝 2 : 하이트맵 ----
    void SetHeightEnabled(bool enabled);
    bool IsHeightEnabled() const { return m_heightEnabled; }

    void SetHeightParams(const terrain::HeightParams& params);
    const terrain::HeightParams& GetHeightParams() const { return m_height.GetParams(); }

    void Regenerate(unsigned seed);   // 새 seed 로 다시 만든다

    UINT GetVertexCount()   const { return m_mesh.GetVertexCount(); }
    UINT GetTriangleCount() const { return m_mesh.GetIndexCount() / 3; }

private:
    bool RebuildMesh();

    Graphics* m_graphics = nullptr;
    Mesh      m_mesh;

    terrain::GridDesc m_desc;
    terrain::HeightField m_height;
    terrain::HeightRange m_heightRange;
    bool m_heightEnabled = true;
    bool m_dirty = true;
    bool m_wireframe = false;

    DirectX::XMFLOAT4 m_color{ 0.30f, 0.42f, 0.34f, 1.0f };        // 지면 색
    DirectX::XMFLOAT4 m_wireColor{ 0.55f, 0.75f, 0.95f, 1.0f };    // 와이어프레임 색
};
