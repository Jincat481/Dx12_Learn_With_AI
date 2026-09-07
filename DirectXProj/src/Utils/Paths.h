#pragma once
#include <string>

// =============================================================
// Paths
//  Shaders / Assets 같은 상대 경로를 실행 환경에 상관없이 찾는다.
//  탐색 순서 : 현재 작업 폴더 → 실행 파일 폴더 → 그 상위 폴더들
//  (VS 에서 실행할 때와 exe 를 직접 실행할 때 모두 동작하게 한다.)
// =============================================================
namespace Paths
{
    std::wstring GetExecutableDirectory();

    // 존재하는 파일/폴더를 찾으면 그 절대 경로를, 못 찾으면 작업 폴더 기준 경로를 돌려준다.
    std::wstring Resolve(const std::wstring& relative);

    // 저장 파일처럼 "아직 없는" 파일의 경로를 만들 때 사용한다(폴더는 만들어 둔다).
    std::wstring ResolveForWrite(const std::wstring& relative);
}
