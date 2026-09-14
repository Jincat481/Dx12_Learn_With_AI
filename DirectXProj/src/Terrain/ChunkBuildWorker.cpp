#include "Core/stdafx.h"
#include "Terrain/ChunkBuildWorker.h"

#include <iterator>

namespace terrain
{
    ChunkBuildWorker::~ChunkBuildWorker()
    {
        // 스레드가 살아 있는 채로 std::thread 가 소멸하면 std::terminate 가 불린다.
        Stop();
    }

    void ChunkBuildWorker::Start(int threadCount)
    {
        if (IsRunning())
            return;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = false;
        }

        threadCount = (std::max)(1, threadCount);
        m_threads.reserve(static_cast<size_t>(threadCount));

        for (int i = 0; i < threadCount; ++i)
            m_threads.emplace_back(&ChunkBuildWorker::WorkerLoop, this);

        dxutil::DebugLog(L"[ChunkBuild] 작업 스레드 %d개 시작", threadCount);
    }

    void ChunkBuildWorker::Stop()
    {
        if (!IsRunning())
            return;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
            m_pending.clear();
        }

        // 잠들어 있는 스레드를 모두 깨워 루프를 빠져나오게 한다.
        m_wake.notify_all();

        for (std::thread& thread : m_threads)
        {
            if (thread.joinable())
                thread.join();
        }
        m_threads.clear();

        std::lock_guard<std::mutex> lock(m_mutex);
        m_completed.clear();
        m_busy = 0;
    }

    void ChunkBuildWorker::Submit(ChunkBuildRequest request)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending.push_back(std::move(request));
        }
        m_wake.notify_one();
    }

    std::vector<ChunkBuildRequest> ChunkBuildWorker::CancelPending()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<ChunkBuildRequest> canceled(std::make_move_iterator(m_pending.begin()),
                                                std::make_move_iterator(m_pending.end()));
        m_pending.clear();
        return canceled;
    }

    void ChunkBuildWorker::TakeCompleted(std::vector<ChunkBuildResult>& out, size_t maxCount)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        size_t taken = 0;
        while (!m_completed.empty() && taken < maxCount)
        {
            out.push_back(std::move(m_completed.front()));
            m_completed.pop_front();
            ++taken;
        }
    }

    size_t ChunkBuildWorker::GetOutstandingCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pending.size() + static_cast<size_t>(m_busy);
    }

    // -------------------------------------------------------------
    // 작업 스레드 본체
    //  요청이 올 때까지 condition_variable 로 잠든다(바쁜 대기로 CPU 를 태우지 않는다).
    //  무거운 계산은 반드시 잠금 밖에서 한다. 잠근 채 계산하면
    //  메인 스레드가 결과를 꺼내려다 그 시간만큼 기다리게 되어 스레드를 쓴 의미가 없다.
    // -------------------------------------------------------------
    void ChunkBuildWorker::WorkerLoop()
    {
        for (;;)
        {
            ChunkBuildRequest request;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [this] { return m_stopping || !m_pending.empty(); });

                if (m_stopping)
                    return;

                request = std::move(m_pending.front());
                m_pending.pop_front();
                ++m_busy;
            }

            ChunkBuildResult result;
            result.worldX = request.worldX;
            result.worldZ = request.worldZ;
            result.generation = request.generation;
            result.replace = request.replace;
            result.ok = TerrainChunk::BuildMeshData(request.originX, request.originZ,
                                                    request.cells, request.cellSize,
                                                    request.skirt, request.height.get(),
                                                    result.data);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_busy;
                if (!m_stopping)
                    m_completed.push_back(std::move(result));
            }
        }
    }
}
