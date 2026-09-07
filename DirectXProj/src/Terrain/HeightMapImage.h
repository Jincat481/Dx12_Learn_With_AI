#pragma once
#include "Core/stdafx.h"

// =============================================================
// HeightMapImage (스텝 3 / S41, S42)
//  회색조 이미지를 CPU 메모리로 읽어 높이 값으로 쓴다.
//
//  절차적 노이즈와의 차이
//   - 노이즈 : 함수. 아무 좌표나 물어봐도 값이 나오고 해상도 제한이 없다.
//   - 이미지 : 표본. 픽셀 사이 값은 보간해서 만들어야 하고,
//              8비트라 높이가 256 단계로 양자화되어 있다.
//
//  격자 정점이 이미지 픽셀보다 촘촘하면 이중선형 보간이 필수다.
//  최근접 이웃으로 뽑으면 계단이 그대로 지형에 나타난다.
// =============================================================
namespace terrain
{
    class HeightMapImage
    {
    public:
        bool LoadFromFile(const std::wstring& path);
        void Clear();

        bool IsValid() const { return !m_heights.empty(); }
        UINT GetWidth()  const { return m_width; }
        UINT GetHeight() const { return m_height; }
        const std::wstring& GetPath() const { return m_path; }

        // u, v 는 0~1. 결과는 0~1 로 정규화된 높이.
        //  범위를 벗어나면 가장자리 값으로 잘라 쓴다(clamp).
        float SampleBilinear(float u, float v) const;

    private:
        float TexelAt(int x, int y) const;

        std::vector<float> m_heights;   // 0~1 로 정규화한 회색값
        UINT m_width = 0;
        UINT m_height = 0;
        std::wstring m_path;
    };
}
