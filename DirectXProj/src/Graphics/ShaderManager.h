#pragma once
#include "Core/stdafx.h"
#include "Graphics/Shader.h"

#if HOT_RELOAD_ENABLED
    #include <filesystem>
#endif

// =============================================================
// ShaderManager (과제 6 / S25, S26)
//  이름으로 Shader 를 보관하고, HLSL 파일의 최종 수정 시간을 기억한다.
//  HOT_RELOAD_ENABLED 가 켜진 빌드에서만 감시 코드가 컴파일된다.
// =============================================================
class ShaderManager
{
public:
    static ShaderManager& Get();

    void Register(const std::string& name, const std::shared_ptr<Shader>& shader);
    std::shared_ptr<Shader> Find(const std::string& name) const;

    // 매 프레임 호출. 내부에서 검사 주기를 두어 파일 시스템 접근을 줄인다.
    void Update(float deltaTime);

    void SetCheckInterval(float seconds) { m_checkInterval = seconds; }
    void Shutdown();

private:
    ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

#if HOT_RELOAD_ENABLED
    using FileTime = std::filesystem::file_time_type;

    struct Entry
    {
        std::string             name;
        std::shared_ptr<Shader> shader;
        FileTime                vsTime{};
        FileTime                psTime{};
        FileTime                failedVsTime{};   // 컴파일에 실패한 버전(같은 내용으로 재시도하지 않는다)
        FileTime                failedPsTime{};
        bool                    hasFailed = false;
    };

    static bool TryGetWriteTime(const std::wstring& path, FileTime& out);
    void CheckAndReload(Entry& entry);
#else
    struct Entry
    {
        std::string             name;
        std::shared_ptr<Shader> shader;
    };
#endif

    std::vector<Entry> m_entries;
    float m_checkInterval = 0.25f;
    float m_timer = 0.0f;
};
