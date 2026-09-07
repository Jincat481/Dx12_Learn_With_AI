#pragma once
#include "Core/stdafx.h"

// =============================================================
// Mesh (S33)
//  정점/인덱스 버퍼 한 쌍을 들고 있는 가장 단순한 메시.
//  스프라이트처럼 정점 4개가 아니라 수천~수만 개를 다루므로
//  인덱스는 32비트(uint32_t)를 쓴다. 16비트는 정점 65,536개가 한계다.
// =============================================================
class Mesh
{
public:
    Mesh() = default;
    ~Mesh() = default;

    bool Create(ID3D11Device* device,
                const void* vertices, UINT vertexCount, UINT stride,
                const uint32_t* indices, UINT indexCount);

    void Release();

    bool IsValid() const { return m_vertexBuffer && m_indexBuffer && m_indexCount > 0; }

    ID3D11Buffer* GetVertexBuffer() const { return m_vertexBuffer.Get(); }
    ID3D11Buffer* GetIndexBuffer()  const { return m_indexBuffer.Get(); }
    UINT GetIndexCount()  const { return m_indexCount; }
    UINT GetVertexCount() const { return m_vertexCount; }
    UINT GetStride()      const { return m_stride; }

private:
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;

    UINT m_vertexCount = 0;
    UINT m_indexCount = 0;
    UINT m_stride = 0;
};
