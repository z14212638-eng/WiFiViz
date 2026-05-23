#pragma once

#include <QWidget>

class QLabel;
class QMouseEvent;

struct PpduVisualItem;

class PpduDetailWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PpduDetailWindow(QWidget *parent = nullptr);

    void clearDetails();
    void showDetails(const QString &senderTitle,
                     const QString &senderMac,
                     const QString &receiverTitle,
                     const QString &receiverMac,
                     const QString &frameType,
                     const QString &statusText,
                     const QString &statusAccent,
                     const QString &channelText,
                     const QString &nodeText,
                     const QString &startText,
                     const QString &endText,
                     const QString &durationText,
                     const QString &sizeText,
                     const QString &mpduText,
                     const QString &mcsText,
                     const QString &throughputText,
                     const QString &snrText,
                     const QString &collisionText);

    QLabel *m_senderTitle = nullptr;
    QLabel *m_senderMac = nullptr;
    QLabel *m_receiverTitle = nullptr;
    QLabel *m_receiverMac = nullptr;
    QLabel *m_frameType = nullptr;
    QLabel *m_statusBadge = nullptr;
    QLabel *m_channelValue = nullptr;
    QLabel *m_nodeValue = nullptr;
    QLabel *m_startValue = nullptr;
    QLabel *m_endValue = nullptr;
    QLabel *m_durationValue = nullptr;
    QLabel *m_sizeValue = nullptr;
    QLabel *m_mpduValue = nullptr;
    QLabel *m_mcsValue = nullptr;
    QLabel *m_throughputValue = nullptr;
    QLabel *m_snrValue = nullptr;
    QLabel *m_collisionValue = nullptr;
};
