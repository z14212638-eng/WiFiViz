#include "ppdu_detail_window.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace
{
QLabel *
CreateValueLabel(const QString &text = "N/A")
{
    auto *label = new QLabel(text);
    label->setStyleSheet("color: #152033; font-size: 14px; font-weight: 600;");
    label->setWordWrap(true);
    return label;
}

QWidget *
CreateMetricCard(const QString &title, QLabel **valueOut)
{
    auto *card = new QFrame;
    card->setStyleSheet(
        "QFrame {"
        "  background: #F8FBFF;"
        "  border: 1px solid #D7E3F1;"
        "  border-radius: 14px;"
        "}"
    );

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("color: #6B7A90; font-size: 11px; letter-spacing: 0.6px;");
    layout->addWidget(titleLabel);

    *valueOut = CreateValueLabel();
    layout->addWidget(*valueOut);
    return card;
}
} // namespace

PpduDetailWindow::PpduDetailWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumWidth(320);
    setMinimumHeight(640);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(
        "QWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "    stop:0 #FFFFFF, stop:1 #F4F8FC);"
        "}"
    );

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(14);

    auto *header = new QFrame(this);
    header->setStyleSheet(
        "QFrame {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D6E2EE;"
        "  border-radius: 18px;"
        "}"
    );
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(18, 16, 18, 16);
    headerLayout->setSpacing(10);

    auto *title = new QLabel("PPDU Inspector", header);
    title->setStyleSheet("color: #122033; font-size: 22px; font-weight: 700;");
    headerLayout->addWidget(title);

    auto *subtitle = new QLabel("Click any PPDU rectangle to refresh this window.", header);
    subtitle->setStyleSheet("color: #6C7B8D; font-size: 12px;");
    headerLayout->addWidget(subtitle);

    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(16);

    auto *frameTypeColumn = new QVBoxLayout;
    frameTypeColumn->setSpacing(4);
    auto *frameTypeTitle = new QLabel("Frame Type", header);
    frameTypeTitle->setStyleSheet("color: #6C7B8D; font-size: 11px; font-weight: 700;");
    frameTypeColumn->addWidget(frameTypeTitle);

    m_frameType = new QLabel("No PPDU Selected", header);
    m_frameType->setStyleSheet("color: #213145; font-size: 15px; font-weight: 600;");
    frameTypeColumn->addWidget(m_frameType);
    statusRow->addLayout(frameTypeColumn, 1);

    auto *statusColumn = new QVBoxLayout;
    statusColumn->setSpacing(4);
    auto *statusTitle = new QLabel("Phy State", header);
    statusTitle->setStyleSheet("color: #6C7B8D; font-size: 11px; font-weight: 700;");
    statusTitle->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusColumn->addWidget(statusTitle);

    m_statusBadge = new QLabel("IDLE", header);
    m_statusBadge->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  background: #5C6D7E;"
        "  border-radius: 10px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "}"
    );
    m_statusBadge->setAlignment(Qt::AlignCenter);
    statusColumn->addWidget(m_statusBadge, 0, Qt::AlignRight);
    statusRow->addLayout(statusColumn, 0);
    headerLayout->addLayout(statusRow);
    root->addWidget(header);

    auto *endpointCard = new QFrame(this);
    endpointCard->setStyleSheet(
        "QFrame {"
        "  background: #FFFFFF;"
        "  border: 1px solid #D6E2EE;"
        "  border-radius: 18px;"
        "}"
    );
    auto *endpointLayout = new QGridLayout(endpointCard);
    endpointLayout->setContentsMargins(18, 16, 18, 16);
    endpointLayout->setHorizontalSpacing(20);
    endpointLayout->setVerticalSpacing(8);

    m_senderTitle = new QLabel("Sender", endpointCard);
    m_senderTitle->setStyleSheet("color: #7FB9FF; font-size: 12px; font-weight: 700;");
    endpointLayout->addWidget(m_senderTitle, 0, 0);

    m_receiverTitle = new QLabel("Receiver", endpointCard);
    m_receiverTitle->setStyleSheet("color: #F8C87A; font-size: 12px; font-weight: 700;");
    endpointLayout->addWidget(m_receiverTitle, 0, 1);

    m_senderMac = CreateValueLabel();
    m_senderMac->setStyleSheet("color: #152033; font-size: 15px; font-weight: 600;");
    endpointLayout->addWidget(m_senderMac, 1, 0);

    m_receiverMac = CreateValueLabel();
    m_receiverMac->setStyleSheet("color: #152033; font-size: 15px; font-weight: 600;");
    endpointLayout->addWidget(m_receiverMac, 1, 1);
    root->addWidget(endpointCard);

    auto *metricsGrid = new QGridLayout;
    metricsGrid->setHorizontalSpacing(12);
    metricsGrid->setVerticalSpacing(12);
    metricsGrid->addWidget(CreateMetricCard("Channel", &m_channelValue), 0, 0);
    metricsGrid->addWidget(CreateMetricCard("Node ID", &m_nodeValue), 0, 1);
    metricsGrid->addWidget(CreateMetricCard("Start", &m_startValue), 1, 0);
    metricsGrid->addWidget(CreateMetricCard("End", &m_endValue), 1, 1);
    metricsGrid->addWidget(CreateMetricCard("Duration", &m_durationValue), 2, 0);
    metricsGrid->addWidget(CreateMetricCard("Size", &m_sizeValue), 2, 1);
    metricsGrid->addWidget(CreateMetricCard("MPDU Aggregate", &m_mpduValue), 3, 0);
    metricsGrid->addWidget(CreateMetricCard("MCS", &m_mcsValue), 3, 1);
    metricsGrid->addWidget(CreateMetricCard("Throughput", &m_throughputValue), 4, 0);
    metricsGrid->addWidget(CreateMetricCard("SNR", &m_snrValue), 4, 1);
    metricsGrid->addWidget(CreateMetricCard("Collision", &m_collisionValue), 5, 0, 1, 2);
    root->addLayout(metricsGrid);
    root->addStretch(1);

    clearDetails();
}

void
PpduDetailWindow::clearDetails()
{
    showDetails("Sender", "N/A",
                "Receiver", "N/A",
                "No PPDU Selected",
                "N/A", "#5C6D7E",
                "N/A", "N/A", "N/A", "N/A", "N/A",
                "N/A", "N/A", "N/A", "N/A", "N/A", "N/A");
}

void
PpduDetailWindow::showDetails(const QString &senderTitle,
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
                              const QString &collisionText)
{
    m_senderTitle->setText(senderTitle);
    m_senderMac->setText(senderMac);
    m_receiverTitle->setText(receiverTitle);
    m_receiverMac->setText(receiverMac);
    m_frameType->setText(frameType);
    m_statusBadge->setText(statusText);
    m_statusBadge->setStyleSheet(
        QString(
            "QLabel {"
            "  color: white;"
            "  background: %1;"
            "  border-radius: 10px;"
            "  padding: 4px 10px;"
            "  font-size: 11px;"
            "  font-weight: 700;"
            "}")
            .arg(statusAccent));

    m_channelValue->setText(channelText);
    m_nodeValue->setText(nodeText);
    m_startValue->setText(startText);
    m_endValue->setText(endText);
    m_durationValue->setText(durationText);
    m_sizeValue->setText(sizeText);
    m_mpduValue->setText(mpduText);
    m_mcsValue->setText(mcsText);
    m_throughputValue->setText(throughputText);
    m_snrValue->setText(snrText);
    m_collisionValue->setText(collisionText);
}
