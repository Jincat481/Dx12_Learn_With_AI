#include "Core/stdafx.h"
#include "Graphics/Mesh.h"

bool Mesh::Create(ID3D11Device* device,
                  const void* vertices, UINT vertexCount, UINT stride,
                  const uint32_t* indices, UINT indexCount)
{
    if (!device || !vertices || !indices || vertexCount == 0 || indexCount == 0 || stride == 0)
        return false;

    Release();

    // ---- 정점 버퍼 ----
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = stride * vertexCount;
    vbDesc.Usage     = D3D11_USAGE_IMMUTABLE;   // 만든 뒤 바꾸지 않는다
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    if (DX_FAILED(device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.GetAddressOf()),
                  L"Mesh::CreateBuffer(vertex)"))
        return false;

    // ---- 인덱스 버퍼 (32비트) ----
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t)) * indexCount;
    ibDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    if (DX_FAILED(device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.GetAddressOf()),
                  L"Mesh::CreateBuffer(index)"))
    {
        m_vertexBuffer.Reset();
        return false;
    }

    m_vertexCount = vertexCount;
    m_indexCount = indexCount;
    m_stride = stride;
    return true;
}

void Mesh::Release()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_vertexCount = 0;
    m_indexCount = 0;
    m_stride = 0;
}
