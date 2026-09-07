#include "Core/stdafx.h"
#include "Graphics/TextureManager.h"

using namespace DirectX;

TextureManager& TextureManager::Get()
{
    static TextureManager instance;
    return instance;
}

void TextureManager::Initialize(ID3D11Device* device)
{
    m_device = device;
}

void TextureManager::Shutdown()
{
    m_cache.clear();
    m_fallback.reset();
    m_device = nullptr;

    // COM 이 살아 있는 동안 WIC 팩토리를 놓아준다.
    Texture::ShutdownWIC();
}

std::shared_ptr<Texture> TextureManager::GetFallback()
{
    if (m_fallback)
        return m_fallback;

    if (!m_device)
        return nullptr;

    auto texture = std::make_shared<Texture>();
    if (!texture->CreateCheckerboard(m_device, 128, 128, 16,
                                     XMFLOAT4(1.0f, 0.25f, 0.6f, 1.0f),
                                     XMFLOAT4(0.15f, 0.15f, 0.2f, 1.0f)))
    {
        return nullptr;
    }

    m_fallback = texture;
    return m_fallback;
}

std::shared_ptr<Texture> TextureManager::Load(const std::wstring& path)
{
    if (!m_device)
    {
        dxutil::DebugLog(L"[TextureManager] Initialize 가 먼저 호출되어야 한다.");
        return nullptr;
    }

    if (path.empty())
        return GetFallback();

    // 1) 캐시 조회
    auto it = m_cache.find(path);
    if (it != m_cache.end())
        return it->second;

    // 2) 실제 로드
    auto texture = std::make_shared<Texture>();
    if (!texture->LoadFromFile(m_device, path))
    {
        dxutil::DebugLog(L"[TextureManager] 로드 실패, 대체 텍스처를 사용한다 : %s", path.c_str());
        auto fallback = GetFallback();
        m_cache[path] = fallback;      // 매 프레임 재시도하지 않도록 실패도 캐시한다.
        return fallback;
    }

    m_cache[path] = texture;
    return texture;
}
