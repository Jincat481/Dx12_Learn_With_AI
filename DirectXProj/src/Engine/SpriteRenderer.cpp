#include "Core/stdafx.h"
#include "Engine/SpriteRenderer.h"
#include "Engine/GameObject.h"
#include "Engine/Transform.h"
#include "Core/Graphics.h"
#include "Graphics/TextureManager.h"
#include "Graphics/Vertex.h"
#include "Utils/StringUtil.h"
#include "Utils/Paths.h"

using namespace DirectX;

SpriteRenderer::SpriteRenderer(const std::wstring& texturePath)
    : m_texturePath(texturePath)
{
}

void SpriteRenderer::Initialize(Graphics* graphics)
{
    m_graphics = graphics;
    if (!m_graphics)
        return;

    m_texture = TextureManager::Get().Load(Paths::Resolve(m_texturePath));
    CreateQuadBuffers();
}

void SpriteRenderer::OnDestroy()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_texture.reset();
    m_indexCount = 0;
}

void SpriteRenderer::SetTexture(const std::wstring& path)
{
    m_texturePath = path;

    if (!m_graphics)
        return;    // 아직 Initialize 전이면 Initialize 에서 처리된다.

    m_texture = TextureManager::Get().Load(Paths::Resolve(m_texturePath));
    CreateQuadBuffers();   // 텍스처 크기가 바뀌면 Quad 도 다시 만든다.
}

bool SpriteRenderer::CreateQuadBuffers()
{
    if (!m_graphics || !m_graphics->GetDevice())
        return false;

    // 텍스처가 없으면 기본 크기로라도 그린다.
    const float width  = m_texture && m_texture->GetWidth()  ? static_cast<float>(m_texture->GetWidth())  : 100.0f;
    const float height = m_texture && m_texture->GetHeight() ? static_cast<float>(m_texture->GetHeight()) : 100.0f;

    const float halfW = width  * 0.5f;
    const float halfH = height * 0.5f;

    m_halfWidth  = halfW;
    m_halfHeight = halfH;

    // UV 원점은 이미지의 좌상단이고, 월드는 +Y 가 위쪽이다. (S10)
    const Vertex vertices[4] =
    {
        { XMFLOAT3(-halfW,  halfH, 0.0f), XMFLOAT2(0.0f, 0.0f) },   // 좌상
        { XMFLOAT3( halfW,  halfH, 0.0f), XMFLOAT2(1.0f, 0.0f) },   // 우상
        { XMFLOAT3( halfW, -halfH, 0.0f), XMFLOAT2(1.0f, 1.0f) },   // 우하
        { XMFLOAT3(-halfW, -halfH, 0.0f), XMFLOAT2(0.0f, 1.0f) },   // 좌하
    };

    const uint32_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    m_indexCount = 6;

    ID3D11Device* device = m_graphics->GetDevice();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    if (DX_FAILED(device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.ReleaseAndGetAddressOf()),
                  L"CreateBuffer(vertex)"))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    if (DX_FAILED(device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf()),
                  L"CreateBuffer(index)"))
        return false;

    return true;
}

void SpriteRenderer::Render()
{
    if (!m_graphics || !m_vertexBuffer || !m_indexBuffer)
        return;

    Transform* transform = GetTransform();
    if (!transform)
        return;

    const XMMATRIX world = transform->GetWorldMatrix();

    // 선택된 스프라이트는 같은 Quad 를 조금 크게, 단색 실루엣으로 먼저 그린다.
    // 그 위에 원본을 겹치면 테두리만 남아 외곽선처럼 보인다(깊이 버퍼 없이 화가 알고리즘).
    if (m_selected && m_halfWidth > 0.0f && m_halfHeight > 0.0f)
    {
        const float scaleX = (m_halfWidth  + m_outlineThickness) / m_halfWidth;
        const float scaleY = (m_halfHeight + m_outlineThickness) / m_halfHeight;

        m_graphics->DrawSprite(m_vertexBuffer.Get(),
                               m_indexBuffer.Get(),
                               m_indexCount,
                               m_texture ? m_texture->GetSRV() : nullptr,
                               XMMatrixScaling(scaleX, scaleY, 1.0f) * world,
                               m_outlineColor,
                               /*silhouette*/ true);
    }

    m_graphics->DrawSprite(m_vertexBuffer.Get(),
                           m_indexBuffer.Get(),
                           m_indexCount,
                           m_texture ? m_texture->GetSRV() : nullptr,
                           world,
                           m_color);
}

// -------------------------------------------------------------
// 히트 테스트 : 월드 점을 스프라이트 로컬 공간으로 되돌려 Quad 범위와 비교한다.
// -------------------------------------------------------------
bool SpriteRenderer::HitTest(const XMFLOAT3& worldPoint)
{
    Transform* transform = GetTransform();
    if (!transform || m_halfWidth <= 0.0f || m_halfHeight <= 0.0f)
        return false;

    const XMMATRIX world = transform->GetWorldMatrix();

    XMVECTOR determinant = XMMatrixDeterminant(world);
    if (XMVectorGetX(XMVectorAbs(determinant)) < 1.0e-12f)
        return false;    // 스케일 0 등으로 역행렬이 없다.

    const XMMATRIX inverse = XMMatrixInverse(&determinant, world);
    const XMVECTOR local = XMVector3TransformCoord(XMLoadFloat3(&worldPoint), inverse);

    const float localX = XMVectorGetX(local);
    const float localY = XMVectorGetY(local);

    return std::fabs(localX) <= m_halfWidth && std::fabs(localY) <= m_halfHeight;
}

// -------------------------------------------------------------
// 직렬화 (과제 4) : 경로는 UTF-8 로 저장한다.
// -------------------------------------------------------------
void SpriteRenderer::ToJson(json::Value& out) const
{
    Component::ToJson(out);

    out["texture"] = json::Value(StringUtil::WideToUtf8(m_texturePath));

    json::Value color = json::Value::MakeArray();
    color.Push(json::Value(m_color.x));
    color.Push(json::Value(m_color.y));
    color.Push(json::Value(m_color.z));
    color.Push(json::Value(m_color.w));
    out["color"] = std::move(color);
}

void SpriteRenderer::FromJson(const json::Value& in)
{
    Component::FromJson(in);

    if (const json::Value* texture = in.Find("texture"))
        m_texturePath = StringUtil::Utf8ToWide(texture->AsString());

    if (const json::Value* color = in.Find("color"))
    {
        m_color.x = color->At(0).AsFloat(1.0f);
        m_color.y = color->At(1).AsFloat(1.0f);
        m_color.z = color->At(2).AsFloat(1.0f);
        m_color.w = color->At(3).AsFloat(1.0f);
    }
}
