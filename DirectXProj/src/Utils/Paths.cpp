#include "Core/stdafx.h"
#include "Utils/Paths.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace Paths
{
    std::wstring GetExecutableDirectory()
    {
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length == 0)
            return std::wstring();

        return fs::path(buffer).parent_path().wstring();
    }

    static std::vector<fs::path> BuildSearchRoots()
    {
        std::vector<fs::path> roots;

        std::error_code ec;
        roots.push_back(fs::current_path(ec));

        const std::wstring exeDir = GetExecutableDirectory();
        if (!exeDir.empty())
        {
            fs::path dir(exeDir);
            for (int i = 0; i < 4 && !dir.empty(); ++i)
            {
                roots.push_back(dir);
                if (!dir.has_parent_path() || dir.parent_path() == dir)
                    break;
                dir = dir.parent_path();
            }
        }
        return roots;
    }

    std::wstring Resolve(const std::wstring& relative)
    {
        fs::path rel(relative);
        std::error_code ec;

        if (rel.is_absolute())
            return relative;

        for (const auto& root : BuildSearchRoots())
        {
            const fs::path candidate = root / rel;
            if (fs::exists(candidate, ec))
                return candidate.wstring();
        }

        // 못 찾으면 작업 폴더 기준 경로를 돌려준다(오류 메시지에 경로가 남게 된다).
        return (fs::current_path(ec) / rel).wstring();
    }

    std::wstring ResolveForWrite(const std::wstring& relative)
    {
        fs::path rel(relative);
        std::error_code ec;

        fs::path base = rel.is_absolute() ? rel : (fs::current_path(ec) / rel);

        // 같은 이름의 폴더가 이미 다른 위치에 있으면 그쪽을 우선한다.
        if (!rel.is_absolute() && rel.has_parent_path())
        {
            for (const auto& root : BuildSearchRoots())
            {
                if (fs::exists(root / rel.parent_path(), ec))
                {
                    base = root / rel;
                    break;
                }
            }
        }

        if (base.has_parent_path())
            fs::create_directories(base.parent_path(), ec);

        return base.wstring();
    }
}
