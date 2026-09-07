#include "Core/stdafx.h"
#include "Graphics/ShaderManager.h"

ShaderManager& ShaderManager::Get()
{
    static ShaderManager instance;
    return instance;
}

void ShaderManager::Shutdown()
{
    m_entries.clear();
}

std::shared_ptr<Shader> ShaderManager::Find(const std::string& name) const
{
    for (const auto& entry : m_entries)
    {
        if (entry.name == name)
            return entry.shader;
    }
    return nullptr;
}

#if HOT_RELOAD_ENABLED

namespace fs = std::filesystem;

bool ShaderManager::TryGetWriteTime(const std::wstring& path, FileTime& out)
{
    if (path.empty())
        return false;

    std::error_code ec;
    const auto time = fs::last_write_time(path, ec);
    if (ec)
        return false;

    out = time;
    return true;
}

void ShaderManager::Register(const std::string& name, const std::shared_ptr<Shader>& shader)
{
    if (!shader)
        return;

    Entry entry;
    entry.name = name;
    entry.shader = shader;
    TryGetWriteTime(shader->GetDesc().vsPath, entry.vsTime);
    TryGetWriteTime(shader->GetDesc().psPath, entry.psTime);

    m_entries.push_back(std::move(entry));
    dxutil::DebugLog(L"[ShaderManager] Hot Reload 감시 등록 : %S", name.c_str());
}

void ShaderManager::CheckAndReload(Entry& entry)
{
    const Shader::Desc& desc = entry.shader->GetDesc();

    FileTime vsTime{}, psTime{};
    const bool hasVs = TryGetWriteTime(desc.vsPath, vsTime);
    const bool hasPs = TryGetWriteTime(desc.psPath, psTime);
    if (!hasVs || !hasPs)
        return;    // 저장 중이라 잠깐 접근이 안 될 수 있다. 다음 주기에 다시 본다.

    const bool changed = (vsTime != entry.vsTime) || (psTime != entry.psTime);
    if (!changed)
        return;

    // 직전에 실패한 것과 완전히 같은 내용이면 재시도하지 않는다(로그 폭주 방지).
    if (entry.hasFailed && vsTime == entry.failedVsTime && psTime == entry.failedPsTime)
        return;

    dxutil::DebugLog(L"[ShaderManager] 변경 감지 : %S → Reload", entry.name.c_str());

    if (entry.shader->Reload())
    {
        // 성공했을 때만 새 수정 시간을 확정한다.
        entry.vsTime = vsTime;
        entry.psTime = psTime;
        entry.hasFailed = false;
        dxutil::DebugLog(L"[ShaderManager] Reload 성공 : %S", entry.name.c_str());
    }
    else
    {
        entry.hasFailed = true;
        entry.failedVsTime = vsTime;
        entry.failedPsTime = psTime;
        dxutil::DebugLog(L"[ShaderManager] Reload 실패 : %S\n%s",
                         entry.name.c_str(), entry.shader->GetLastError().c_str());
    }
}

void ShaderManager::Update(float deltaTime)
{
    m_timer += deltaTime;
    if (m_timer < m_checkInterval)
        return;
    m_timer = 0.0f;

    for (auto& entry : m_entries)
    {
        if (entry.shader)
            CheckAndReload(entry);
    }
}

#else   // ------------------ Release : 감시 코드 제외 ------------------

void ShaderManager::Register(const std::string& name, const std::shared_ptr<Shader>& shader)
{
    if (!shader)
        return;

    Entry entry;
    entry.name = name;
    entry.shader = shader;
    m_entries.push_back(std::move(entry));
}

void ShaderManager::Update(float /*deltaTime*/)
{
    // Release 빌드에서는 파일 시스템을 전혀 건드리지 않는다. (S26)
}

#endif
