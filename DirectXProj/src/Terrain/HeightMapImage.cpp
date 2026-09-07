#include "Core/stdafx.h"
#include "Terrain/HeightMapImage.h"
#include "Graphics/WicLoader.h"

#include <cmath>

namespace terrain
{
    bool HeightMapImage::LoadFromFile(const std::wstring& path)
    {
        std::vector<uint8_t> pixels;
        UINT width = 0;
        UINT height = 0;

        if (!wic::LoadPixelsRGBA(path, pixels, width, height))
        {
            dxutil::DebugLog(L"[HeightMap] 이미지를 읽지 못했다 : %s", path.c_str());
            return false;
        }

        m_width = width;
        m_height = height;
        m_heights.resize(static_cast<size_t>(width) * height);

        // RGBA → 밝기(luminance) → 0~1 높이
        //  회색조 이미지면 R = G = B 라 어느 채널을 써도 같지만,
        //  컬러 이미지를 넣어도 동작하도록 표준 밝기 공식을 쓴다.
        for (size_t i = 0; i < m_heights.size(); ++i)
        {
            const uint8_t r = pixels[i * 4 + 0];
            const uint8_t g = pixels[i * 4 + 1];
            const uint8_t b = pixels[i * 4 + 2];

            const float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
            m_heights[i] = luminance;
        }

        m_path = path;
        dxutil::DebugLog(L"[HeightMap] 로드 완료 : %s (%ux%u, 표본 %zu개)",
                         path.c_str(), m_width, m_height, m_heights.size());
        return true;
    }

    void HeightMapImage::Clear()
    {
        m_heights.clear();
        m_width = 0;
        m_height = 0;
        m_path.clear();
    }

    float HeightMapImage::TexelAt(int x, int y) const
    {
        if (m_heights.empty())
            return 0.0f;

        // 가장자리를 넘어가면 잘라 쓴다(clamp). 반복(wrap)하면 지형이 이어지는 듯한
        // 이상한 이음매가 생긴다.
        x = (std::max)(0, (std::min)(static_cast<int>(m_width) - 1, x));
        y = (std::max)(0, (std::min)(static_cast<int>(m_height) - 1, y));

        return m_heights[static_cast<size_t>(y) * m_width + x];
    }

    // -------------------------------------------------------------
    // 이중선형 보간 (S42)
    //  격자 정점이 이미지 픽셀보다 촘촘하면 픽셀 사이 값을 만들어 내야 한다.
    //  가로로 두 번 섞고, 그 둘을 세로로 한 번 더 섞는다.
    //
    //      t00 ---- t10        u 방향으로 섞어 a, b 를 만들고
    //       |        |         v 방향으로 a, b 를 섞는다
    //      t01 ---- t11
    //
    //  최근접 이웃으로 뽑으면 이미지 픽셀 경계가 그대로 계단이 되어 보인다.
    // -------------------------------------------------------------
    float HeightMapImage::SampleBilinear(float u, float v) const
    {
        if (m_heights.empty() || m_width == 0 || m_height == 0)
            return 0.0f;

        u = (std::max)(0.0f, (std::min)(1.0f, u));
        v = (std::max)(0.0f, (std::min)(1.0f, v));

        // 텍셀 중심 기준 좌표로 옮긴다.
        const float fx = u * static_cast<float>(m_width - 1);
        const float fy = v * static_cast<float>(m_height - 1);

        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;

        const float tx = fx - static_cast<float>(x0);
        const float ty = fy - static_cast<float>(y0);

        const float t00 = TexelAt(x0, y0);
        const float t10 = TexelAt(x1, y0);
        const float t01 = TexelAt(x0, y1);
        const float t11 = TexelAt(x1, y1);

        const float a = t00 + (t10 - t00) * tx;
        const float b = t01 + (t11 - t01) * tx;

        return a + (b - a) * ty;
    }
}
