#include "qt_ppdu_reader.h"
#include "ppdu_adapter.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <QThread>
#include <QDebug>

using namespace boost::interprocess;
constexpr const char *SHM_NAME = "Ns3PpduSharedMemory";

QtPpduReader::QtPpduReader(QObject *parent)
    : QObject(parent), m_running(true), m_shm(nullptr), m_ring(nullptr)
{
}

// 主循环
void QtPpduReader::run()
{
    m_running = true;
    m_shm = nullptr;
    m_ring = nullptr;

    QThread::msleep(300);

    // 等待共享内存可用
    while (m_running && !m_shm)
    {
        try
        {
            m_shm = new managed_shared_memory(open_only, SHM_NAME);
            m_ring = m_shm->find<RingBuffer>("PpduRing").first;
        }
        catch (...)
        {
            QThread::msleep(50);
        }
    }

    if (!m_running || !m_ring)
    {
        if (m_shm)
        {
            delete m_shm;
            m_shm = nullptr;
            m_ring = nullptr;
        }
        emit finished();
        return;
    }

    // Do not drain here to avoid dropping early PPDU data

    while (m_running)
    {
        try
        {
            scoped_lock<interprocess_mutex> lock(m_ring->mutex);

            // 等待数据或退出信号，加入超时防止挂死
            bool hasData = m_ring->cond.wait_for(
                lock, boost::posix_time::milliseconds(100),
                [&] { return m_ring->read_index != m_ring->write_index || !m_running; });

            if (!hasData)
            {
                // 超时且没有数据，检查 ns-3 是否已退出
                if (!m_running && m_ring->read_index == m_ring->write_index)
                {
                    qDebug() << "[PPDU] ns-3 ended and ring buffer empty, exiting reader";
                    break;
                }

                // 没有新数据，继续循环
                continue;
            }

            uint32_t idx = m_ring->read_index % MAX_PPDU_NUM;
            PPDU_Meta shmPpdu = m_ring->records[idx];
            m_ring->read_index++;

            lock.unlock();

            auto visual = PpduAdapter::FromShm(shmPpdu);
            emit ppduReady(visual);
        }
        catch (boost::interprocess::lock_exception &e)
        {
            qDebug() << "PPDU lock_exception, exiting thread...";
            break;
        }
        catch (...)
        {
            qDebug() << "PPDU unknown exception, exiting thread...";
            break;
        }
    }

    // 清理共享内存
    if (m_shm)
    {
        delete m_shm;
        m_shm = nullptr;
        m_ring = nullptr;
    }

    emit finished();
}


void QtPpduReader::clearBuffer()
{
    if (m_ring)
    {
        try
        {
            scoped_lock<interprocess_mutex> lock(m_ring->mutex);
            m_ring->read_index = m_ring->write_index;
            qDebug() << "[PPDU] Ring buffer drained";
        }
        catch (...)
        {
            qDebug() << "[PPDU] Failed to drain ring buffer";
        }
    }
}


// 安全 stop
void QtPpduReader::stop()
{
    m_running = false;

    if (m_ring)
    {
        try
        {
            // 尝试非阻塞锁
            if (m_ring->mutex.try_lock())
            {
                // 成功获得锁才通知
                m_ring->cond.notify_all();
                m_ring->mutex.unlock();
                qDebug() << "PPDU stop: notified ring buffer";
            }
            else
            {
                qDebug() << "PPDU stop: mutex busy, skipping notify";
            }
        }
        catch (boost::interprocess::lock_exception &e)
        {
            qDebug() << "PPDU stop: lock exception, maybe already destroyed";
        }
        catch (...)
        {
            qDebug() << "PPDU stop: unknown exception";
        }
    }
}
