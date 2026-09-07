#pragma once
#include "Core/stdafx.h"
#include "Graphics/Texture.h"

// =============================================================
// TextureManager (과제 2 / S08)
//  경로를 키로 하는 캐시. 같은 이미지를 두 번 GPU 에 올리지 않는다.
//  파일이 없으면 체커보드 대체 텍스처를 만들어 실행이 멈추지 않게 한다.
// =============================================================
class TextureManager
{
public:
    static TextureManager& Get();

    void Initialize(ID3D11Device* device);
    void Shutdown();

    // 경로별 캐시 조회 → 없으면 로드 → 실패하면 대체 텍스처
    std::shared_ptr<Texture> Load(const std::wstring& path);

    std::shared_ptr<Texture> GetFallback();

    size_t GetCachedCount() const { return m_cache.size(); }

private:
    TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::wstring, std::shared_ptr<Texture>> m_cache;
    std::shared_ptr<Texture> m_fallback;
};
