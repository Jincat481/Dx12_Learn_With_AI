#pragma once
#include "Core/stdafx.h"
#include "Terrain/TerrainChunk.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

// =============================================================
// ChunkBuildWorker (S65)
//  청크의 정점 계산을 작업 스레드로 넘긴다.
//
//  무한 지형(스텝 10)은 카메라가 청크 경계를 넘을 때마다 새 청크를 만든다.
//  이 계산(노이즈 수천 번)을 메인 스레드에서 하면 그 프레임이 늦어져 화면이 끊긴다.
//
//  나누는 기준
//   - 작업 스레드 : 높이 샘플링 · 법선 · 모프 타깃 · 인덱스   → 순수 CPU 계산
//   - 메인 스레드 : ID3D11Buffer 생성과 슬롯 교체             → GPU 자원과 공유 상태
//
//  스레드끼리 주고받는 것은 "요청 줄" 과 "결과 줄" 두 개뿐이고, 뮤텍스 하나로 지킨다.
//  작업 스레드는 청크 배열을 절대 직접 만지지 않는다.
// =============================================================
namespace terrain
{
    struct ChunkBuildRequest
    {
        int      worldX = 0;
        int      worldZ = 0;
        uint32_t generation = 0;   // 지형 설정이 바뀔 때마다 올라간다. 옛 결과를 버리는 데 쓴다.

        float originX = 0.0f;
        float originZ = 0.0f;
        int   cells = 16;
        float cellSize = 1.0f;
        bool  skirt = true;

        // 같은 좌표의 청크를 새 높이로 바꿔 끼운다 (편집 · 침식처럼 제자리에서 높이가 바뀐 경우, S74)
        bool  replace = false;

        // 높이 함수의 사본. 메인 스레드가 원본을 바꿔도(N 키) 계산 중인 작업은 영향을 받지 않는다.
        std::shared_ptr<const HeightField> height;
    };

    struct ChunkBuildResult
    {
        int      worldX = 0;
        int      worldZ = 0;
        uint32_t generation = 0;
        bool     ok = false;
        bool     replace = false;
        ChunkMeshData data;
    };

    class ChunkBuildWorker
    {
    public:
        ChunkBuildWorker() = default;
        ~ChunkBuildWorker();

        ChunkBuildWorker(const ChunkBuildWorker&) = delete;
        ChunkBuildWorker& operator=(const ChunkBuildWorker&) = delete;

        void Start(int threadCount);
        void Stop();                       // 남은 요청을 버리고 스레드를 모두 합류(join)시킨다
        bool IsRunning() const { return !m_threads.empty(); }
        int  GetThreadCount() const { return static_cast<int>(m_threads.size()); }

        void Submit(ChunkBuildRequest request);

        // 아직 시작하지 않은 요청을 모두 빼서 돌려준다. 이미 계산 중인 것은 끝까지 간다.
        std::vector<ChunkBuildRequest> CancelPending();

        // 끝난 결과를 최대 maxCount 개 꺼내 out 뒤에 붙인다.
        void TakeCompleted(std::vector<ChunkBuildResult>& out, size_t maxCount);

        // 줄 서 있거나 계산 중인 요청 수
        size_t GetOutstandingCount() const;

    private:
        void WorkerLoop();

        mutable std::mutex            m_mutex;
        std::condition_variable       m_wake;
        std::deque<ChunkBuildRequest> m_pending;
        std::deque<ChunkBuildResult>  m_completed;
        std::vector<std::thread>      m_threads;
        int  m_busy = 0;
        bool m_stopping = false;
    };
}
