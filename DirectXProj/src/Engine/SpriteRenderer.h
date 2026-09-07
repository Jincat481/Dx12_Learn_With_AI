#pragma once
#include "Core/stdafx.h"
#include "Engine/Component.h"
#include "Graphics/Texture.h"

class Graphics;

// =============================================================
// SpriteRenderer (과제 2 / S10)
//  텍스처 크기를 기준으로 정점 4개 · 인덱스 6개짜리 Quad 를 만든다.
//  정점은 오브젝트 로컬 좌표에서 원점을 중심으로 배치된다.
//
//      0(-hw, +hh) ---- 1(+hw, +hh)      삼각형 : 0-1-2, 0-2-3
//          |     \           |           UV     : 좌상단이 (0,0)
//      3(-hw, -hh) ---- 2(+hw, -hh)
// =============================================================
class SpriteRenderer : public Component
{
public:
    SpriteRenderer() = default;
    explicit SpriteRenderer(const std::wstring& texturePath);
    ~SpriteRenderer() override = default;

    const char* GetTypeName() const override { return "SpriteRenderer"; }

    void Initialize(Graphics* graphics) override;
    void Render() override;
    void OnDestroy() override;

    void SetTexture(const std::wstring& path);
    const std::wstring& GetTexturePath() const { return m_texturePath; }

    void SetColor(const DirectX::XMFLOAT4& color) { m_color = color; }
    const DirectX::XMFLOAT4& GetColor() const { return m_color; }

    // ---- 피킹 / 선택 하이라이트 ----
    // 월드 좌표 한 점이 이 스프라이트 위에 있는지 검사한다.
    // 월드 행렬의 역행렬로 로컬 공간에 되돌려 보므로 회전·스케일·부모 변환이 모두 반영된다.
    bool HitTest(const DirectX::XMFLOAT3& worldPoint);

    void SetSelected(bool selected) { m_selected = selected; }
    bool IsSelected() const { return m_selected; }

    void SetOutlineColor(const DirectX::XMFLOAT4& color) { m_outlineColor = color; }
    void SetOutlineThickness(float pixels) { m_outlineThickness = pixels; }

    float GetHalfWidth()  const { return m_halfWidth; }
    float GetHalfHeight() const { return m_halfHeight; }

    UINT GetTextureWidth()  const { return m_texture ? m_texture->GetWidth()  : 0; }
    UINT GetTextureHeight() const { return m_texture ? m_texture->GetHeight() : 0; }

    void ToJson(json::Value& out) const override;
    void FromJson(const json::Value& in) override;

private:
    bool CreateQuadBuffers();

    Graphics* m_graphics = nullptr;
    std::shared_ptr<Texture> m_texture;
    std::wstring m_texturePath;

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    UINT m_indexCount = 0;

    DirectX::XMFLOAT4 m_color{ 1.0f, 1.0f, 1.0f, 1.0f };

    // Quad 의 로컬 반지름(픽셀). HitTest 와 외곽선 크기 계산에 함께 쓴다.
    float m_halfWidth = 0.0f;
    float m_halfHeight = 0.0f;

    bool  m_selected = false;
    DirectX::XMFLOAT4 m_outlineColor{ 1.0f, 0.85f, 0.15f, 1.0f };
    float m_outlineThickness = 6.0f;   // 로컬 기준 두께(픽셀)
};
