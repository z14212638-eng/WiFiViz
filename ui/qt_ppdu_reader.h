// qt_ppdu_reader.h
#pragma once

#include <QObject>
#include <atomic>
#include <cstdint>
#include "shm.h"
#include "ppdu_visual_item.h"

class QtPpduReader : public QObject
{
    Q_OBJECT
public:
    explicit QtPpduReader(QObject* parent = nullptr);

public slots:
    void run(); 
    void stop(); 
    void clearBuffer();

signals:
    void ppduReady(const PpduVisualItem& ppdu);
    void finished();

private:
    std::atomic_bool m_running{true};
    uint64_t m_ppduCount = 0;      


    boost::interprocess::managed_shared_memory* m_shm = nullptr;
    RingBuffer* m_ring = nullptr;
};
