#include "node_config.h"
#include "ui_node_config.h"
#include "flow_dialog.h"

node_config::node_config(QWidget *parent)
    : QWidget(parent), ui(new Ui::node_config)
{
    ui->setupUi(this);

    centerWindow(this);

    
    ui->tabWidget->setCurrentIndex(0);
    if (!ui->checkBox_2->isChecked())
    {
        pos_set = true;
    }

    if (!ui->checkBox->isChecked())
    {
        ui->comboBox->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
        ui->doubleSpinBox_8->setEnabled(false);
        ui->doubleSpinBox_9->setEnabled(false);
        ui->doubleSpinBox_10->setEnabled(false);
        ui->doubleSpinBox_11->setEnabled(false);
        ui->doubleSpinBox_4->setEnabled(false);
        ui->doubleSpinBox_5->setEnabled(false);
    }

    if (phymac_set != true)
    {
        ui->tabWidget->setTabEnabled(3, false);
    }
    ui->tableWidget->setRowCount(0);

    // deduce the GI from the selected protocol
    ui->comboBox_5->setCurrentIndex(5);
    ui->comboBox_15->setEnabled(false);

    Restrict_channel();
}

void node_config::Restrict_channel()
{
    auto apply_rate_control = [this]()
    {
        auto standard = get_standard_from_string(
            ui->comboBox_5->currentText().toStdString());

        auto is_allowed = [standard](const QString &name)
        {
            if (standard == Standard_MAP::k80211b)
            {
                return name == "Aarf" || name == "Amrr" || name == "Arf" ||
                       name == "Cara" || name == "Onoe" || name == "Rraa" ||
                       name == "Ideal" || name == "Constant" || name == "MinstrelHt";
            }
            if (standard == Standard_MAP::k80211a || standard == Standard_MAP::k80211g)
            {
                return name == "Aarf" || name == "Amrr" || name == "Arf" ||
                       name == "Cara" || name == "Onoe" || name == "Rraa" ||
                       name == "Minstrel" || name == "Ideal" || name == "Constant" || name == "MinstrelHt";
            }
            // 802.11n/ac/ax
            return name == "MinstrelHt" || name == "Ideal" || name == "Constant" ||
                   name == "ThomsonSampling";
        };

        int firstAllowed = -1;
        for (int i = 0; i < ui->comboBox_7->count(); ++i)
        {
            const QString item = ui->comboBox_7->itemText(i);
            const bool allowed = is_allowed(item);
            ui->comboBox_7->setItemData(i, allowed ? QVariant() : QVariant(0), Qt::UserRole - 1);
            if (allowed && firstAllowed == -1)
            {
                firstAllowed = i;
            }
        }

        if (firstAllowed != -1 &&
            !is_allowed(ui->comboBox_7->currentText()))
        {
            ui->comboBox_7->setCurrentIndex(firstAllowed);
        }
    };

    connect(ui->comboBox_5,
            &QComboBox::currentTextChanged,
            this,
            [this, apply_rate_control](const QString &text)
            {
                auto standard = get_standard_from_string(text.toStdString());

                for (int i = 0; i < ui->comboBox_15->count(); ++i)
                {
                    ui->comboBox_15->setItemData(
                        i, QVariant(), Qt::UserRole - 1);
                }

                switch (standard)
                {
                case Standard_MAP::k80211a:
                case Standard_MAP::k80211b:
                case Standard_MAP::k80211g:
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setEnabled(false);
                    break;

                case Standard_MAP::k80211n:
                case Standard_MAP::k80211ac:
                    ui->comboBox_15->setEnabled(true);
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setItemData(
                        2, QVariant(0), Qt::UserRole - 1);
                    ui->comboBox_15->setItemData(
                        3, QVariant(1), Qt::UserRole - 1);
                    break;

                case Standard_MAP::k80211ax:
                    ui->comboBox_15->setEnabled(true);
                    ui->comboBox_15->setCurrentIndex(1);
                    ui->comboBox_15->setItemData(
                        0, QVariant(0), Qt::UserRole - 1);
                    break;

                default:
                    break;
                }

                apply_rate_control();
            });

    connect(ui->comboBox_3,
            &QComboBox::currentTextChanged,
            this,
            [this](const QString &text)
            {
                ui->comboBox_16->clear();

                if (text == "2.4G")
                {
                    ui->comboBox_16->clear();
                    ui->comboBox_16->addItems({"20", "40"});
                    ui->comboBox_16->setCurrentText("20");

                    ui->spinBox->setRange(1, 13);
                    ui->spinBox->setSingleStep(1);
                    ui->spinBox->setValue(1);
                }
                else if (text == "5G")
                {
                    ui->comboBox_16->clear();
                    ui->comboBox_16->addItems({"20", "40", "80", "160"});
                    ui->comboBox_16->setCurrentText("40");

                    ui->spinBox->setRange(36, 165);
                    ui->spinBox->setSingleStep(4);
                    ui->spinBox->setValue(36);
                }
            });

    connect(ui->comboBox_16,
            &QComboBox::currentTextChanged,
            this,
            [this](const QString &bwText)
            {
                int bw = bwText.toInt();

                if (ui->comboBox_3->currentText() != "5G")
                    return;

                if (bw == 20 || bw == 40)
                {
                    ui->spinBox->setRange(36, 165);
                    ui->spinBox->setSingleStep(4);
                    ui->spinBox->setValue(36);
                }
                else if (bw == 80)
                {
                    ui->spinBox->setRange(42, 155);
                    ui->spinBox->setSingleStep(16);
                    ui->spinBox->setValue(42);
                }
                else if (bw == 160)
                {
                    ui->spinBox->setRange(50, 114);
                    ui->spinBox->setSingleStep(64);
                    ui->spinBox->setValue(50);
                }
            });

    apply_rate_control();
}

node_config::~node_config()
{
    delete ui;
}

void node_config::setPosition(Sta &sta)
{
    if (!ui->checkBox_2->isChecked())
    {

        if (ui->doubleSpinBox->value() != 0.00 || ui->doubleSpinBox_2->value() != 0.00 || ui->doubleSpinBox_3->value() != 0.00)
        {

            ui->tabWidget->setCurrentIndex(1);
            sta.m_position = {ui->doubleSpinBox->value(),
                              ui->doubleSpinBox_2->value(),
                              ui->doubleSpinBox_3->value()};
            pos_set = true;
        }
        else
        {
            QMessageBox::critical(
                this,
                "Error",
                "The position can not be (0,0,0)",
                QMessageBox::Ok);
        }
    }
    else
    {
        pos_set = true;

        // Random position
        double x_max = this->Building_range[0];
        double y_max = this->Building_range[1];
        double z_max = this->Building_range[2];
        double x_rand = get_true_random_double(0, x_max);
        double y_rand = get_true_random_double(0, y_max);
        double z_rand = get_true_random_double(0, z_max);

        sta.m_position = {x_rand, y_rand, z_rand};
        std::cout << "The position of AP is: " << x_rand << " " << y_rand << " " << z_rand << std::endl;
        ui->tabWidget->setCurrentIndex(1);
    }
}

void node_config::setMobility(Sta &sta)
{
    // If mobility is not set
    if (!ui->checkBox->isChecked())
    {
        ui->tabWidget->setCurrentIndex(2);
        mobility_set = true;
        sta.Mobility = false;
        return;
    }

    // If mobility is set and "random" mode is selected
    else if (ui->checkBox->isChecked() && ui->comboBox->currentText() == "Random Mobility Model")
    {
        sta.Mobility = true;
        sta.mode = "Random Mobility Model";
        double x_max = this->Building_range[0];
        double y_max = this->Building_range[1];
        double x_rand_1 = get_true_random_double(0, x_max);
        double y_rand_1 = get_true_random_double(0, y_max);
        double x_rand_2 = get_true_random_double(0, x_max);
        double y_rand_2 = get_true_random_double(0, y_max);

        ui->doubleSpinBox_8->setValue((x_rand_1 > x_rand_2) ? x_rand_2 : x_rand_1);
        ui->doubleSpinBox_9->setValue((x_rand_1 > x_rand_2) ? x_rand_1 : x_rand_2);
        ui->doubleSpinBox_10->setValue((y_rand_1 > y_rand_2) ? y_rand_2 : y_rand_1);
        ui->doubleSpinBox_11->setValue((y_rand_1 > y_rand_2) ? y_rand_1 : y_rand_2);

        // set the boundaries of mobility
        sta.boundaries = {ui->doubleSpinBox_8->value(), ui->doubleSpinBox_9->value(), ui->doubleSpinBox_10->value(), ui->doubleSpinBox_11->value()};
        sta.time_change_interval = ui->doubleSpinBox_4->value();
        sta.mode = ui->comboBox_2->currentText();
        sta.distance_change_interval = ui->doubleSpinBox_5->value();
        sta.random_velocity = ui->doubleSpinBox_7->value();

        std::cout << "The boundaries of AP is: " << sta.boundaries[0] << " " << sta.boundaries[1] << " " << sta.boundaries[2] << " " << sta.boundaries[3] << std::endl;
        std::cout << "The time change interval of AP is: " << sta.time_change_interval << std::endl;
        std::cout << "The distance change interval of AP is: " << sta.distance_change_interval << std::endl;
        std::cout << "The random velocity of AP is: " << sta.random_velocity << std::endl;
        std::cout << "The mode of AP is: " << sta.mode.toStdString() << std::endl;

        ui->tabWidget->setCurrentIndex(2);
        sta.Mobility = true;
        return;
    }
    // If not choosing the "random" mode,check if the conditions are valid
    else if ((ui->doubleSpinBox_9->value() > ui->doubleSpinBox_8->value()) &&
             (ui->doubleSpinBox_11->value() > ui->doubleSpinBox_10->value()) &&
             (ui->doubleSpinBox_4->value() != 0.00 || ui->doubleSpinBox_5->value() != 0.00) &&
             (ui->comboBox->currentText() == "Draw the Route Manually"))
    {
        mobility_set = true;
    }
    else
    {
        mobility_set = false;
    }
    if (!pos_set)
    {
        ui->tabWidget->setCurrentIndex(0);
        QMessageBox::critical(
            this,
            "Error",
            "Please set the position",
            QMessageBox::Ok);
    }

    else
    {
        if (mobility_set)
        {
            ui->tabWidget->setCurrentIndex(2);
            sta.Mobility = true;
        }

        else
        {
            QMessageBox::critical(
                this,
                "Error",
                "Please set the mobility correctly",
                QMessageBox::Ok);
        }
    }
}

void node_config::Load_One_Config(Sta &one_sta)
{
    // set the channel number
    one_sta.channel_number = ui->spinBox->value();
    qint16 channel_number = ui->spinBox->value();

    // set the frequency
    if (ui->comboBox_3->currentText() == "5G")
    {
        one_sta.Frequency = 5000 + channel_number * 5;
    }
    else if (ui->comboBox_3->currentText() == "2.4G")
    {
        one_sta.Frequency = 2400 + (channel_number - 1) * 5;
    }

    // set the GI
    QString GI_str = ui->comboBox_15->currentText();
    if (ui->comboBox_3->currentText() == "5G")
    {
        one_sta.Frequency = 5000 + channel_number * 5;
    }
    else if (ui->comboBox_3->currentText() == "2.4G")
    {
        one_sta.Frequency = 2400 + (channel_number - 1) * 5;
    }

    if (ui->comboBox_15->currentText() == "400ns")
    {
        one_sta.GI = 400;
    }
    else if (ui->comboBox_15->currentText() == "800ns")
    {
        one_sta.GI = 800;
    }
    else if (ui->comboBox_15->currentText() == "1600ns")
    {
        one_sta.GI = 1600;
    }
    else if (ui->comboBox_15->currentText() == "3200ns")
    {
        one_sta.GI = 3200;
    }

    // set the bandwidth
    one_sta.bandwidth = ui->comboBox_16->currentText().toShort();
    // set the Tx power
    one_sta.TxPower = ui->doubleSpinBox_6->value();
    // set the Ssid
    one_sta.Ssid = ui->lineEdit->text();
    // set the phy_model
    one_sta.Phy_model = ui->comboBox_4->currentText();
    // set the Standard
    one_sta.Standard = ui->comboBox_5->currentText();

    // set the MaxMissedBeacons
    one_sta.MaxMissedBeacons = ui->spinBox_4->value();
    // set the ProbeRequestTimeout
    one_sta.ProbeRequestTimeout = ui->spinBox_5->value();
    // set the EnableAssocFailRetry
    one_sta.EnableAssocFailRetry = ui->checkBox_7->isChecked();
    // set the ActiveProbing
    one_sta.active_probing = ui->checkBox_12->isChecked();
    // set the Rts_Cts
    one_sta.RtsCts = ui->checkBox_5->isChecked();
    one_sta.RtsCts_Threshold = ui->spinBox_3->value();
    // set the Rate_ctr_algo
    one_sta.Rate_ctr_algo = ui->comboBox_7->currentText();
    // set the TxQueue
    one_sta.TxQueue = ui->comboBox_8->currentText();

    // set the Qos
    one_sta.Qos = ui->checkBox_6->isChecked();
    one_sta.Edca = ui->comboBox_6->currentText();
}

void node_config::Get_Edca_Config(Sta &sta, Edca_config &edca_config)
{
    sta.AC_VO_cwmin = edca_config.AC_VO_cwmin;
    sta.AC_VO_cwmax = edca_config.AC_VO_cwmax;
    sta.AC_VO_AIFSN = edca_config.AC_VO_AIFSN;
    sta.AC_VO_TXOP_LIMIT = edca_config.AC_VO_TXOP_LIMIT;

    sta.AC_VI_cwmin = edca_config.AC_VI_cwmin;
    sta.AC_VI_cwmax = edca_config.AC_VI_cwmax;
    sta.AC_VI_AIFSN = edca_config.AC_VI_AIFSN;
    sta.AC_VI_TXOP_LIMIT = edca_config.AC_VI_TXOP_LIMIT;

    sta.AC_BE_cwmin = edca_config.AC_BE_cwmin;
    sta.AC_BE_cwmax = edca_config.AC_BE_cwmax;
    sta.AC_BE_AIFSN = edca_config.AC_BE_AIFSN;
    sta.AC_BE_TXOP_LIMIT = edca_config.AC_BE_TXOP_LIMIT;

    sta.AC_BK_cwmin = edca_config.AC_BK_cwmin;
    sta.AC_BK_cwmax = edca_config.AC_BK_cwmax;
    sta.AC_BK_AIFSN = edca_config.AC_BK_AIFSN;
    sta.AC_BK_TXOP_LIMIT = edca_config.AC_BK_TXOP_LIMIT;

    sta.Msdu_aggregation = edca_config.Msdu_aggregation;
    sta.AMsdu_type = edca_config.AMsdu_type;
    sta.MaxAMsduSize = edca_config.MaxAMsduSize;

    sta.Mpdu_aggregation = edca_config.Mpdu_aggregation;
    sta.AMpdu_type = edca_config.AMpdu_type;
    sta.MaxAMpduSize = edca_config.MaxAMpduSize;
    sta.Density = edca_config.Density;
}

void node_config::Get_Antenna_Config(Sta &sta, Antenna &antenna_config)
{
    // set the Antenna
    std::cout << "the number of antenna is: " << antenna_config.antenna_list.size() << std::endl;

    if (antenna_config.AntennaCount() == 0)
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the antenna",
            QMessageBox::Ok);
        return;
    }

    std::cout << antenna_config.antenna_list[0]->Antenna_type.toStdString() << std::endl;
    for (auto item : antenna_config.antenna_list)
    {
        sta.Antenna_list.push_back(std::move(item));
    }
}

// Page1_Finished
void node_config::on_pushButton_clicked()
{
    emit Page1();
}

// Page2_Finished
void node_config::on_pushButton_4_clicked()
{
    emit Page2();
}

// Page1_Random_Position
void node_config::on_checkBox_2_clicked(bool checked)
{
    if (checked)
    {
        ui->doubleSpinBox->setEnabled(false);
        ui->doubleSpinBox_2->setEnabled(false);
        ui->doubleSpinBox_3->setEnabled(false);
        pos_set = true;
    }
    else
    {
        ui->doubleSpinBox->setEnabled(true);
        ui->doubleSpinBox_2->setEnabled(true);
        ui->doubleSpinBox_3->setEnabled(true);
    }
}

// Page2_Mobility
void node_config::on_checkBox_clicked(bool checked)
{
    if (checked)
    {
        ui->comboBox->setEnabled(true);
        ui->comboBox_2->setEnabled(true);
        ui->doubleSpinBox_8->setEnabled(true);
        ui->doubleSpinBox_9->setEnabled(true);
        ui->doubleSpinBox_10->setEnabled(true);
        ui->doubleSpinBox_11->setEnabled(true);
        ui->doubleSpinBox_4->setEnabled(true);
        ui->doubleSpinBox_5->setEnabled(true);
        ui->doubleSpinBox_7->setEnabled(true);
        mobility_set = false;
    }
    else
    {
        ui->comboBox->setEnabled(false);
        ui->comboBox_2->setEnabled(false);
        ui->doubleSpinBox_8->setEnabled(false);
        ui->doubleSpinBox_9->setEnabled(false);
        ui->doubleSpinBox_10->setEnabled(false);
        ui->doubleSpinBox_11->setEnabled(false);
        ui->doubleSpinBox_4->setEnabled(false);
        ui->doubleSpinBox_5->setEnabled(false);
        ui->doubleSpinBox_7->setEnabled(false);
        mobility_set = true;
    }
}

// Page3_Rts_Cts

void node_config::on_checkBox_5_clicked(bool checked)
{
    if (checked)
    {
        ui->spinBox_3->setEnabled(true);
        Rts_Cts_set = true;
    }
    else
    {
        ui->spinBox_3->setEnabled(false);
        Rts_Cts_set = false;
    }
}

// Page3_Qos

void node_config::on_checkBox_6_clicked(bool checked)
{
    if (checked)
    {
        ui->comboBox_6->setEnabled(true);
        ui->pushButton_8->setEnabled(true);
        Qos_set = true;
    }
    else
    {
        ui->comboBox_6->setEnabled(false);
        ui->pushButton_8->setEnabled(false);
        Qos_set = false;
    }
}

// Edca_Config
void node_config::on_pushButton_8_clicked()
{
    emit Edca_to_config();
}

// Antenna_Config
void node_config::on_pushButton_9_clicked()
{
    emit Antenna_to_config();
}

// Page3_Finished
void node_config::on_pushButton_6_clicked()
{
    if (ui->lineEdit->text().isEmpty())
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the Ssid",
            QMessageBox::Ok);
        return;
    }

    phymac_set = true;
    ui->tabWidget->setTabEnabled(3, true);
    ui->tabWidget->setCurrentIndex(3);
}

bool node_config::Integrity_Check()
{
    return true;
}

// Add one traffic
void node_config::Get_Traffic_Config(Sta &sta)
{
    // 遍历所有保存的流量配置
    for (const FlowConfig &config : m_flowConfigs)
    {
        TrafficConfig new_traffic;
        new_traffic.Flow_id = config.flowId;
        new_traffic.Protocol = config.protocol;
        new_traffic.Destination = config.destination;
        new_traffic.StartTime = config.startTime;
        new_traffic.EndTime = config.endTime;
        new_traffic.Tos = config.tos;
        
        // 根据流量类型设置不同的参数
        new_traffic.flowType = static_cast<TrafficConfig::FlowType>(config.flowType);
        
        switch (config.flowType) {
            case FlowConfig::OnOff:
                new_traffic.dataRate = config.dataRate;
                new_traffic.packetSize = config.packetSize;
                new_traffic.ontimeType = config.ontimeType;
                new_traffic.ontimeParams = config.ontimeParams;
                new_traffic.offtimeType = config.offtimeType;
                new_traffic.offtimeParams = config.offtimeParams;
                new_traffic.maxBytes = config.maxBytes;
                break;
                
            case FlowConfig::CBR:
                new_traffic.packetSize = config.packetSize;
                new_traffic.interval = config.interval;
                new_traffic.maxPackets = config.maxPackets;
                break;
                
            case FlowConfig::Bulk:
                new_traffic.maxBytes = config.maxBytes;
                break;
        }
        
        // Insert the data structure into the vector
        sta.Traffic_list.push_back(new_traffic);
    }
}
void node_config::on_pushButton_2_clicked()
{
    flow_dialog dialog(this, false);
    
    if (dialog.exec() == QDialog::Accepted) {
        FlowConfig config = dialog.getFlowConfig();
        
        // 保存配置
        m_flowConfigs.push_back(config);
        
        // 在表格中显示
        if (ui->tableWidget->rowCount() == 1 &&
            ui->tableWidget->item(0, 0) &&
            ui->tableWidget->item(0, 0)->text().isEmpty())
        {
            ui->tableWidget->removeRow(0);
        }
        
        int rowCount = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(rowCount);
        ui->tableWidget->setItem(rowCount, 0, new QTableWidgetItem(config.flowId));
        ui->tableWidget->setItem(rowCount, 1, new QTableWidgetItem(QString::number(config.startTime)));
        ui->tableWidget->setItem(rowCount, 2, new QTableWidgetItem(QString::number(config.endTime)));
        ui->tableWidget->setItem(rowCount, 3, new QTableWidgetItem(config.protocol));
        ui->tableWidget->setItem(rowCount, 4, new QTableWidgetItem(config.destination));
        
        // 根据流量类型显示额外信息
        QString typeStr;
        switch(config.flowType) {
            case FlowConfig::OnOff:
                typeStr = "OnOff";
                break;
            case FlowConfig::CBR:
                typeStr = "CBR";
                break;
            case FlowConfig::Bulk:
                typeStr = "Bulk";
                break;
        }
        ui->tableWidget->setItem(rowCount, 5, new QTableWidgetItem(typeStr));
    }
}

// Delete one traffic
void node_config::on_pushButton_3_clicked()
{
    if (!m_flowConfigs.isEmpty()) {
        m_flowConfigs.removeLast();
        
        int row = ui->tableWidget->rowCount() - 1;
        if (row >= 0) {
            ui->tableWidget->removeRow(row);
        }
    }
}

// Page4_Traffic_Finished
void node_config::on_pushButton_7_clicked()
{
    // check if the flow id is set
    if (ui->lineEdit->text().isEmpty())
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the FlowId",
            QMessageBox::Ok);
        return;
    }

    // load the settings of the traffic
    if (ui->tableWidget->rowCount() == 0)
    {
        QMessageBox::critical(
            this,
            "Error",
            "Please set the traffic",
            QMessageBox::Ok);
        return;
    }

    emit LoadOneStaConfig();
    StaIndex++;
    m_flowConfigs.clear();  // 清空流量配置，为下一个STA做准备
    ui->tableWidget->setRowCount(0);  // 清空表格，准备配置下一个STA
    emit Finish_setting_sta();
}

void node_config::resetPage()
{
    qDebug() << "[node_config] resetPage()";

    // ===============================
    // 1️⃣ 内部状态变量
    // ===============================
    Building_range = {0, 0, 0};
    m_position     = {0, 0, 0};
    StaIndex       = 0;

    pos_set      = false;
    mobility_set = false;
    phymac_set   = false;
    beacon_set   = false;
    Rts_Cts_set  = false;
    Qos_set      = false;
    Ssid_set     = false;

    // ===============================
    // 2️⃣ Tab 状态
    // ===============================
    ui->tabWidget->setCurrentIndex(0);
    ui->tabWidget->setTabEnabled(3, false); // Page3 需要 phy/mac 完成

    // ===============================
    // 3️⃣ Position（Page1）
    // ===============================
    ui->checkBox_2->setChecked(true);
    ui->doubleSpinBox->setEnabled(true);
    ui->doubleSpinBox_2->setEnabled(true);
    ui->doubleSpinBox_3->setEnabled(true);

    ui->doubleSpinBox->setValue(0.0);
    ui->doubleSpinBox_2->setValue(0.0);
    ui->doubleSpinBox_3->setValue(0.0);

    // ===============================
    // 4️⃣ Mobility（Page2）
    // ===============================
    ui->checkBox->setChecked(false);

    ui->comboBox->setEnabled(false);
    ui->comboBox_2->setEnabled(false);

    ui->doubleSpinBox_8->setEnabled(false);
    ui->doubleSpinBox_9->setEnabled(false);
    ui->doubleSpinBox_10->setEnabled(false);
    ui->doubleSpinBox_11->setEnabled(false);
    ui->doubleSpinBox_4->setEnabled(false);
    ui->doubleSpinBox_5->setEnabled(false);
    ui->doubleSpinBox_7->setEnabled(false);

    ui->doubleSpinBox_8->setValue(0);
    ui->doubleSpinBox_9->setValue(0);
    ui->doubleSpinBox_10->setValue(0);
    ui->doubleSpinBox_11->setValue(0);
    ui->doubleSpinBox_4->setValue(0);
    ui->doubleSpinBox_5->setValue(0);
    ui->doubleSpinBox_7->setValue(0);

    // ===============================
    // 5️⃣ PHY / MAC（Page3）
    // ===============================
    // ui->lineEdit->clear();              // SSID
    ui->comboBox_3->setCurrentText("5G"); // band
    ui->comboBox_4->setCurrentIndex(0);   // phy model
    ui->comboBox_5->setCurrentText("802.11ax"); // standard
    ui->comboBox_7->setCurrentIndex(0);
    ui->comboBox_8->setCurrentIndex(0);

    ui->spinBox->setValue(36);
    ui->spinBox_3->setValue(0);
    ui->spinBox_4->setValue(0);
    ui->spinBox_5->setValue(0);

    ui->doubleSpinBox_6->setValue(0.0);

    ui->checkBox_5->setChecked(false); // RTS/CTS
    ui->checkBox_6->setChecked(false); // QoS

    ui->spinBox_3->setEnabled(false);
    ui->comboBox_6->setEnabled(false);
    ui->pushButton_8->setEnabled(false);

    // ===============================
    // 6️⃣ EDCA / Traffic（Page4）
    // ===============================
    ui->tableWidget->setRowCount(0);
    
    // 清空流量配置
    m_flowConfigs.clear();

    // ===============================
    // 7️⃣ Channel / Band 约束重新生效
    // ===============================
    Restrict_channel();

    qDebug() << "[node_config] resetPage() done";
}
