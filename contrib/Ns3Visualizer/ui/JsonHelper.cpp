#include "JsonHelper.h"
#include <QDateTime>
#include <QSet>


double get_true_random_double(double min, double max)
{
    std::random_device rd;
    std::uniform_real_distribution<> dis(min, max);
    return dis(rd);
}

void JsonHelper::UpsertNodePositionEntry(QJsonArray &entries, int id, const QJsonArray &position)
{
    for (int i = 0; i < entries.size(); ++i)
    {
        QJsonObject obj = entries[i].toObject();
        if (obj["id"].toInt() == id)
        {
            obj["pos"] = position;
            entries[i] = obj;
            return;
        }
    }

    QJsonObject entry;
    entry["id"] = id;
    entry["pos"] = position;
    entries.append(entry);
}

void JsonHelper::UpsertNodeConfig(QVector<QJsonObject> &configs, const QJsonObject &jsonObj)
{
    const int id = jsonObj["Id"].toInt(-1);
    for (int i = 0; i < configs.size(); ++i)
    {
        if (configs[i]["Id"].toInt(-1) == id)
        {
            configs[i] = jsonObj;
            return;
        }
    }
    configs.append(jsonObj);
}

Standard_MAP get_standard_from_string(const std::string &gi_str)
{
    auto it = standard_map.find(gi_str);
    if (it == standard_map.end())
    {
        return Standard_MAP::k80211ax;
    }
    return it->second;
}

bool JsonHelper::SaveJsonObjToRoute(const QJsonObject jsonObj, const QString filePath)
{

    QJsonDocument jsonDoc(jsonObj);

    QFileInfo fileInfo(filePath);
    qDebug() << "Absolute file path:" << fileInfo.absoluteFilePath();
    QDir parentDir = fileInfo.absoluteDir();
    if (!parentDir.exists())
    {
        if (!parentDir.mkpath("."))
        {
            qWarning() << "Failed to create directory:" << parentDir.path();
            return false;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }

    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

void JsonHelper::ensureRunDirectories()
{
    if (run_dir_initialized)
        return;

    const QString timestamp = QDateTime::currentDateTime().toString("yy_M_d_HHmm");
    Run_dir = Base_dir + "Designed_" + timestamp + "/";

    const QString apDir = Run_dir + "ApConfigJson";
    const QString staDir = Run_dir + "StaConfigJson";
    const QString generalDir = Run_dir + "GeneralJson";

    QDir().mkpath(apDir);
    QDir().mkpath(staDir);
    QDir().mkpath(generalDir);

    Ap_file_path = apDir + "/Ap_";
    Sta_file_path = staDir + "/Sta_";
    General_file_path = generalDir + "/";

    run_dir_initialized = true;
}

bool JsonHelper::SetApToJson(const Ap *ap, qint8 id)
{
    QJsonObject jsonObj;

    // Position
    jsonObj["Position"] = QJsonArray{ap->m_position[0],
                                     ap->m_position[1],
                                     ap->m_position[2]};
    jsonObj["Id"] = id;

    QJsonArray apPosList;
    if (m_building_config.contains("Ap_pos_list") && m_building_config["Ap_pos_list"].isArray())
    {
        apPosList = m_building_config["Ap_pos_list"].toArray();
    }

    QJsonObject apEntry;
    apEntry["id"] = id;

    QJsonArray position;
    position.append(ap->m_position[0]);
    position.append(ap->m_position[1]);
    position.append(ap->m_position[2]);
    apEntry["pos"] = position;
    UpsertNodePositionEntry(apPosList, id, position);
    m_building_config["Ap_pos_list"] = apPosList;

    // Mobility configuration
    QJsonObject mobilityObj;
    mobilityObj["set"] = ap->Mobility;
    mobilityObj["model_type"] = ap->Mobility_model;
    mobilityObj["boundaries"] = QJsonArray{ap->boundaries[0],
                                           ap->boundaries[1],
                                           ap->boundaries[2],
                                           ap->boundaries[3]};
    mobilityObj["mode"] = ap->mode;
    mobilityObj["time_change_interval"] = ap->time_change_interval;
    mobilityObj["distance_change_interval"] = ap->distance_change_interval;
    mobilityObj["velocity"] = ap->random_velocity;
    jsonObj["Mobility"] = mobilityObj;

    // Basic network configuration
    jsonObj["Channel_number"] = ap->channel_number;
    jsonObj["Frequency"] = ap->Frequency;
    jsonObj["Bandwidth"] = ap->bandwidth;
    jsonObj["Tx_power"] = ap->TxPower;
    jsonObj["Ssid"] = ap->Ssid;
    jsonObj["Phy_model"] = ap->Phy_model;
    jsonObj["Standard"] = ap->Standard;
    jsonObj["Guard_interval"] = ap->GI;
    jsonObj["Slot"] = ap->Slot;
    jsonObj["Sifs"] = ap->Sifs;
    jsonObj["RxSensitivity"] = ap->RxSensitivity;
    jsonObj["CcaEdThreshold"] = ap->CcaThreshold;
    jsonObj["CcaSensitivity"] = ap->CcaSensitivity;

    // Beacon configuration
    QJsonObject beaconObj;
    beaconObj["set"] = ap->Beacon;
    beaconObj["BeaconInterval"] = ap->Beacon_interval;
    beaconObj["Beacon_Rate"] = ap->Beacon_Rate;
    beaconObj["EnableBeaconJitter"] = ap->EnableBeaconJitter;
    jsonObj["Beacon"] = beaconObj;

    // RTS/CTS configuration
    QJsonObject rtsCtsObj;
    rtsCtsObj["set"] = ap->RtsCts;
    rtsCtsObj["Threshold"] = ap->RtsCts_Threshold;
    jsonObj["Rts_Cts"] = rtsCtsObj;

    jsonObj["Rate_ctrl_algo"] = ap->Rate_ctr_algo;
    jsonObj["TxQueue"] = ap->TxQueue;

    // QoS configuration
    QJsonObject qosObj;
    qosObj["Qos_support"] = ap->Qos;
    qosObj["Edca_choose"] = ap->Edca;

    // EDCA parameters
    QJsonObject edcaParamsObj;
    QJsonObject acVoObj;
    acVoObj["CWmin"] = ap->AC_VO_cwmin;
    acVoObj["CWmax"] = ap->AC_VO_cwmax;
    acVoObj["AIFSN"] = ap->AC_VO_AIFSN;
    acVoObj["TXOPLimit"] = ap->AC_VO_TXOP_LIMIT;
    edcaParamsObj["AC_VO"] = acVoObj;

    QJsonObject acViObj;
    acViObj["CWmin"] = ap->AC_VI_cwmin;
    acViObj["CWmax"] = ap->AC_VI_cwmax;
    acViObj["AIFSN"] = ap->AC_VI_AIFSN;
    acViObj["TXOPLimit"] = ap->AC_VI_TXOP_LIMIT;
    edcaParamsObj["AC_VI"] = acViObj;

    QJsonObject acBeObj;
    acBeObj["CWmin"] = ap->AC_BE_cwmin;
    acBeObj["CWmax"] = ap->AC_BE_cwmax;
    acBeObj["AIFSN"] = ap->AC_BE_AIFSN;
    acBeObj["TXOPLimit"] = ap->AC_BE_TXOP_LIMIT;
    edcaParamsObj["AC_BE"] = acBeObj;

    QJsonObject acBkObj;
    acBkObj["CWmin"] = ap->AC_BK_cwmin;
    acBkObj["CWmax"] = ap->AC_BK_cwmax;
    acBkObj["AIFSN"] = ap->AC_BK_AIFSN;
    acBkObj["TXOPLimit"] = ap->AC_BK_TXOP_LIMIT;
    edcaParamsObj["AC_BK"] = acBkObj;

    qosObj["Edca_params"] = edcaParamsObj;

    // Traffic Configuration
    QJsonObject Traffic;
    QJsonArray Flows;
    qDebug() << "ap->Traffic_list.size():" << ap->Traffic_list.size();
    for (const auto &flow : ap->Traffic_list)
    {
        QJsonObject Flow;
        Flow["FlowId"] = flow.Flow_id;
        Flow["Protocol"] = flow.Protocol;
        Flow["Destination"] = flow.Destination;
        Flow["StartTime"] = flow.StartTime;
        Flow["EndTime"] = flow.EndTime;
        Flow["Tos"] = flow.Tos;
        
        // 流量类型
        QString flowTypeStr;
        switch (flow.flowType) {
            case TrafficConfig::OnOff:
            {
                flowTypeStr = "OnOff";
                Flow["DataRate"] = flow.dataRate;
                Flow["PacketSize"] = flow.packetSize;
                Flow["OntimeType"] = flow.ontimeType;
                Flow["OfftimeType"] = flow.offtimeType;
                Flow["MaxBytes"] = flow.maxBytes;
                
                // OnTime 参数
                QJsonObject ontimeParams;
                for (auto it = flow.ontimeParams.begin(); it != flow.ontimeParams.end(); ++it) {
                    ontimeParams[it.key()] = it.value();
                }
                Flow["OntimeParams"] = ontimeParams;
                
                // OffTime 参数
                QJsonObject offtimeParams;
                for (auto it = flow.offtimeParams.begin(); it != flow.offtimeParams.end(); ++it) {
                    offtimeParams[it.key()] = it.value();
                }
                Flow["OfftimeParams"] = offtimeParams;
                break;
            }
                
            case TrafficConfig::CBR:
            {
                flowTypeStr = "CBR";
                Flow["PacketSize"] = flow.packetSize;
                Flow["Interval"] = flow.interval;
                Flow["MaxPackets"] = flow.maxPackets;
                break;
            }
                
            case TrafficConfig::Bulk:
            {
                flowTypeStr = "Bulk";
                Flow["MaxBytes"] = flow.maxBytes;
                break;
            }
        }
        Flow["FlowType"] = flowTypeStr;
        
        Flows.append(Flow);
    }
    Traffic["Flows"] = Flows;
    jsonObj["Traffic"] = Traffic;

    // MSDU Aggregation
    QJsonObject msduAggObj;
    msduAggObj["set"] = ap->Msdu_aggregation;
    msduAggObj["type"] = ap->AMsdu_type;
    msduAggObj["MaxAmsduSize"] = ap->MaxAMsduSize;
    qosObj["Msdu_Aggregation"] = msduAggObj;

    // MPDU Aggregation
    QJsonObject mpduAggObj;
    mpduAggObj["set"] = ap->Mpdu_aggregation;
    mpduAggObj["type"] = ap->AMpdu_type;
    mpduAggObj["MaxAmpduSize"] = ap->MaxAMpduSize;
    mpduAggObj["Density"] = ap->Density;
    qosObj["Mpdu_Aggregation"] = mpduAggObj;

    jsonObj["Qos"] = qosObj;

    // Antenna configuration
    QJsonObject antennaObj;
    antennaObj["set"] = !ap->Antenna_list.isEmpty();
    QJsonArray antennasArray;
    for (const auto &antenna : ap->Antenna_list)
    {
        if (antenna)
        {
            QJsonObject antennaItem;
            antennaItem["type"] = antenna->Antenna_type;
            antennaItem["MaxGain"] = antenna->MaxGain;
            antennaItem["Beamwidth"] = antenna->Beamwidth;
            antennasArray.append(antennaItem);
        }
    }
    antennaObj["Antennas"] = antennasArray;
    jsonObj["Antenna"] = antennaObj;

    // Store the configuration
    if (m_ap_config)
    {
        delete m_ap_config;
    }
    m_ap_config = new QJsonObject(jsonObj);
    UpsertNodeConfig(m_ap_config_list, jsonObj);
    m_building_config["Ap_num"] = m_ap_config_list.size();
    return true;
}

bool JsonHelper::SetStaToJson(const Sta *sta, qint8 id)
{
    QJsonObject jsonObj;

    // Position
    jsonObj["Position"] = QJsonArray{sta->m_position[0],
                                     sta->m_position[1],
                                     sta->m_position[2]};
    jsonObj["Id"] = id;

    QJsonArray staPosList;
    if (m_building_config.contains("Sta_pos_list") && m_building_config["Sta_pos_list"].isArray())
    {
        staPosList = m_building_config["Sta_pos_list"].toArray();
    }

    QJsonObject staEntry;
    staEntry["id"] = id;

    QJsonArray position;
    position.append(sta->m_position[0]);
    position.append(sta->m_position[1]);
    position.append(sta->m_position[2]);
    staEntry["pos"] = position;
    UpsertNodePositionEntry(staPosList, id, position);
    m_building_config["Sta_pos_list"] = staPosList;

    // Mobility configuration
    QJsonObject mobilityObj;
    mobilityObj["set"] = sta->Mobility;
    mobilityObj["model_type"] = sta->Mobility_model;
    mobilityObj["boundaries"] = QJsonArray{sta->boundaries[0],
                                           sta->boundaries[1],
                                           sta->boundaries[2],
                                           sta->boundaries[3]};
    mobilityObj["mode"] = sta->mode;
    mobilityObj["time_change_interval"] = sta->time_change_interval;
    mobilityObj["distance_change_interval"] = sta->distance_change_interval;
    mobilityObj["velocity"] = sta->random_velocity;
    jsonObj["Mobility"] = mobilityObj;

    // Basic network configuration
    jsonObj["Channel_number"] = sta->channel_number;
    jsonObj["Frequency"] = sta->Frequency;
    jsonObj["Bandwidth"] = sta->bandwidth;
    jsonObj["Tx_power"] = sta->TxPower;
    jsonObj["Ssid"] = sta->Ssid;
    jsonObj["Phy_model"] = sta->Phy_model;
    jsonObj["Standard"] = sta->Standard;
    jsonObj["Guard_interval"] = sta->GI;
    jsonObj["MaxMissedBeacons"] = sta->MaxMissedBeacons;
    jsonObj["ProbeRequestTimeout"] = sta->ProbeRequestTimeout;
    jsonObj["EnableAssocFailRetry"] = sta->EnableAssocFailRetry;
    jsonObj["ActiveProbing"] = sta->active_probing;  

    // RTS/CTS configuration
    QJsonObject rtsCtsObj;
    rtsCtsObj["set"] = sta->RtsCts;
    rtsCtsObj["Threshold"] = sta->RtsCts_Threshold;
    jsonObj["Rts_Cts"] = rtsCtsObj;

    jsonObj["Rate_ctrl_algo"] = sta->Rate_ctr_algo;
    jsonObj["TxQueue"] = sta->TxQueue;

    // QoS configuration
    QJsonObject qosObj;
    qosObj["Qos_support"] = sta->Qos;
    qosObj["Edca_choose"] = sta->Edca;

    // EDCA parameters
    QJsonObject edcaParamsObj;
    QJsonObject acVoObj;
    acVoObj["CWmin"] = sta->AC_VO_cwmin;
    acVoObj["CWmax"] = sta->AC_VO_cwmax;
    acVoObj["AIFSN"] = sta->AC_VO_AIFSN;
    acVoObj["TXOPLimit"] = sta->AC_VO_TXOP_LIMIT;
    edcaParamsObj["AC_VO"] = acVoObj;

    QJsonObject acViObj;
    acViObj["CWmin"] = sta->AC_VI_cwmin;
    acViObj["CWmax"] = sta->AC_VI_cwmax;
    acViObj["AIFSN"] = sta->AC_VI_AIFSN;
    acViObj["TXOPLimit"] = sta->AC_VI_TXOP_LIMIT;
    edcaParamsObj["AC_VI"] = acViObj;

    QJsonObject acBeObj;
    acBeObj["CWmin"] = sta->AC_BE_cwmin;
    acBeObj["CWmax"] = sta->AC_BE_cwmax;
    acBeObj["AIFSN"] = sta->AC_BE_AIFSN;
    acBeObj["TXOPLimit"] = sta->AC_BE_TXOP_LIMIT;
    edcaParamsObj["AC_BE"] = acBeObj;

    QJsonObject acBkObj;
    acBkObj["CWmin"] = sta->AC_BK_cwmin;
    acBkObj["CWmax"] = sta->AC_BK_cwmax;
    acBkObj["AIFSN"] = sta->AC_BK_AIFSN;
    acBkObj["TXOPLimit"] = sta->AC_BK_TXOP_LIMIT;
    edcaParamsObj["AC_BK"] = acBkObj;

    qosObj["Edca_params"] = edcaParamsObj;

    // Traffic Configuration
    QJsonObject Traffic;
    QJsonArray Flows;
    qDebug() << "sta->Traffic_list.size():" << sta->Traffic_list.size();
    for (const auto &flow : sta->Traffic_list)
    {
        QJsonObject Flow;
        Flow["FlowId"] = flow.Flow_id;
        Flow["Protocol"] = flow.Protocol;
        Flow["Destination"] = flow.Destination;
        Flow["StartTime"] = flow.StartTime;
        Flow["EndTime"] = flow.EndTime;
        Flow["Tos"] = flow.Tos;
        
        // 流量类型
        QString flowTypeStr;
        switch (flow.flowType) {
            case TrafficConfig::OnOff:
            {
                flowTypeStr = "OnOff";
                Flow["DataRate"] = flow.dataRate;
                Flow["PacketSize"] = flow.packetSize;
                Flow["OntimeType"] = flow.ontimeType;
                Flow["OfftimeType"] = flow.offtimeType;
                Flow["MaxBytes"] = flow.maxBytes;
                
                // OnTime 参数
                QJsonObject ontimeParams;
                for (auto it = flow.ontimeParams.begin(); it != flow.ontimeParams.end(); ++it) {
                    ontimeParams[it.key()] = it.value();
                }
                Flow["OntimeParams"] = ontimeParams;
                
                // OffTime 参数
                QJsonObject offtimeParams;
                for (auto it = flow.offtimeParams.begin(); it != flow.offtimeParams.end(); ++it) {
                    offtimeParams[it.key()] = it.value();
                }
                Flow["OfftimeParams"] = offtimeParams;
                break;
            }
                
            case TrafficConfig::CBR:
            {
                flowTypeStr = "CBR";
                Flow["PacketSize"] = flow.packetSize;
                Flow["Interval"] = flow.interval;
                Flow["MaxPackets"] = flow.maxPackets;
                break;
            }
                
            case TrafficConfig::Bulk:
            {
                flowTypeStr = "Bulk";
                Flow["MaxBytes"] = flow.maxBytes;
                break;
            }
        }
        Flow["FlowType"] = flowTypeStr;
        
        Flows.append(Flow);
    }
    Traffic["Flows"] = Flows;
    jsonObj["Traffic"] = Traffic;

    // MSDU Aggregation
    QJsonObject msduAggObj;
    msduAggObj["set"] = sta->Msdu_aggregation;
    msduAggObj["type"] = sta->AMsdu_type;
    msduAggObj["MaxAmsduSize"] = sta->MaxAMsduSize;
    qosObj["Msdu_Aggregation"] = msduAggObj;

    // MPDU Aggregation
    QJsonObject mpduAggObj;
    mpduAggObj["set"] = sta->Mpdu_aggregation;
    mpduAggObj["type"] = sta->AMpdu_type;
    mpduAggObj["MaxAmpduSize"] = sta->MaxAMpduSize;
    mpduAggObj["Density"] = sta->Density;
    qosObj["Mpdu_Aggregation"] = mpduAggObj;
    

    jsonObj["Qos"] = qosObj;

    // Antenna configuration
    QJsonObject antennaObj;
    antennaObj["set"] = !sta->Antenna_list.isEmpty();
    QJsonArray antennasArray;
    for (const auto &antenna : sta->Antenna_list)
    {
        if (antenna)
        {
            QJsonObject antennaItem;
            antennaItem["type"] = antenna->Antenna_type;
            antennaItem["MaxGain"] = antenna->MaxGain;
            antennaItem["Beamwidth"] = antenna->Beamwidth;
            antennasArray.append(antennaItem);
        }
    }
    antennaObj["Antennas"] = antennasArray;
    jsonObj["Antenna"] = antennaObj;

    // Store the configuration
    if (m_sta_config)
    {
        delete m_sta_config;
    }
    m_sta_config = new QJsonObject(jsonObj);
    UpsertNodeConfig(m_sta_config_list, jsonObj);
    m_building_config["Sta_num"] = m_sta_config_list.size();
    return true;
}

void JsonHelper::reset()
{
    qDebug() << "[JsonHelper] reset()";

    // 1️⃣ 清空 building json
    m_building_config = QJsonObject();

    // 2️⃣ 释放动态分配的 json（避免泄漏）
    if (m_sta_config)
    {
        delete m_sta_config;
        m_sta_config = nullptr;
    }

    if (m_ap_config)
    {
        delete m_ap_config;
        m_ap_config = nullptr;
    }

    // 3️⃣ 清空配置列表
    m_sta_config_list.clear();
    m_ap_config_list.clear();

    // 4️⃣ 重建 building 配置（而不是 clear）
    m_building.reset();
    m_building = std::make_shared<BuildingConfig>();

    // 5️⃣ （可选）初始化 building json 的基础字段
    m_building_config["Sta_num"] = 0;
    m_building_config["Ap_num"]  = 0;
    m_building_config["Sta_pos_list"] = QJsonArray();
    m_building_config["Ap_pos_list"]  = QJsonArray();

    // 6️⃣ reset run directory (next run will create a new folder)
    run_dir_initialized = false;
    Run_dir.clear();
    Ap_file_path.clear();
    Sta_file_path.clear();
    General_file_path.clear();
    
    // 7️⃣ reset project config
    m_project_config = ProjectConfig();
}

bool JsonHelper::SaveProjectConfig(const QString &projectFilePath, const QString &projectName)
{
    QJsonObject projectObj;
    
    // Basic project info
    projectObj["project_name"] = projectName.isEmpty() ? "Untitled_Project" : projectName;
    projectObj["ns3_directory"] = Base_dir;
    projectObj["run_directory"] = Run_dir;
    
    // Config folders
    projectObj["ap_config_folder"] = Run_dir + "ApConfigJson";
    projectObj["sta_config_folder"] = Run_dir + "StaConfigJson";
    projectObj["general_config_folder"] = Run_dir + "GeneralJson";
    
    // Config files list
    QJsonArray apFiles;
    for (int i = 0; i < m_ap_config_list.size(); ++i) {
        apFiles.append(QString("Ap_%1.json").arg(i));
    }
    projectObj["ap_config_files"] = apFiles;
    
    QJsonArray staFiles;
    for (int i = 0; i < m_sta_config_list.size(); ++i) {
        staFiles.append(QString("Sta_%1.json").arg(i));
    }
    projectObj["sta_config_files"] = staFiles;
    
    projectObj["general_config_file"] = "general.json";
    
    // Timestamps
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    projectObj["created_time"] = currentTime;
    projectObj["last_modified"] = currentTime;
    
    // Statistics
    projectObj["ap_count"] = m_ap_config_list.size();
    projectObj["sta_count"] = m_sta_config_list.size();
    
    // Save to file
    bool success = SaveJsonObjToRoute(projectObj, projectFilePath);
    
    if (success) {
        qDebug() << "Project configuration saved to:" << projectFilePath;
        
        // Update internal project config
        m_project_config.project_name = projectName;
        m_project_config.ns3_directory = Base_dir;
        m_project_config.run_directory = Run_dir;
        m_project_config.ap_config_folder = Run_dir + "ApConfigJson";
        m_project_config.sta_config_folder = Run_dir + "StaConfigJson";
        m_project_config.general_config_folder = Run_dir + "GeneralJson";
        m_project_config.created_time = currentTime;
        m_project_config.last_modified = currentTime;
    }
    
    return success;
}

bool JsonHelper::LoadProjectConfig(const QString &projectFilePath)
{
    QFile file(projectFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open project file:" << projectFilePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse project file:" << parseError.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qWarning() << "Invalid project file format";
        return false;
    }
    
    QJsonObject projectObj = doc.object();
    
    // Get the project directory (where the .nsproj file is located)
    QFileInfo fileInfo(projectFilePath);
    QString projectDir = fileInfo.absolutePath();
    
    // Load project configuration
    m_project_config.project_name = projectObj["project_name"].toString();
    m_project_config.work_directory = projectObj["work_directory"].toString();
    m_project_config.ns3_directory = projectObj["ns3_directory"].toString();
    m_project_config.run_directory = projectObj["run_directory"].toString();
    
    // Config folders (stored as relative paths, convert to absolute)
    QString apConfigFolderRel = projectObj["ap_config_folder"].toString();
    QString staConfigFolderRel = projectObj["sta_config_folder"].toString();
    QString generalConfigFolderRel = projectObj["general_config_folder"].toString();
    
    m_project_config.ap_config_folder = projectDir + "/" + apConfigFolderRel;
    m_project_config.sta_config_folder = projectDir + "/" + staConfigFolderRel;
    m_project_config.general_config_folder = projectDir + "/" + generalConfigFolderRel;
    m_project_config.general_config_file = projectObj["general_config_file"].toString();
    m_project_config.created_time = projectObj["created_time"].toString();
    m_project_config.last_modified = projectObj["last_modified"].toString();
    
    // Load file lists
    m_project_config.ap_config_files.clear();
    QJsonArray apFiles = projectObj["ap_config_files"].toArray();
    for (const auto &file : apFiles) {
        m_project_config.ap_config_files.append(file.toString());
    }
    
    m_project_config.sta_config_files.clear();
    QJsonArray staFiles = projectObj["sta_config_files"].toArray();
    for (const auto &file : staFiles) {
        m_project_config.sta_config_files.append(file.toString());
    }
    
    // Update internal paths
    Base_dir = m_project_config.ns3_directory;
    Run_dir = projectDir + "/";  // Use project directory as run directory
    Ap_file_path = m_project_config.ap_config_folder + "/Ap_";
    Sta_file_path = m_project_config.sta_config_folder + "/Sta_";
    General_file_path = m_project_config.general_config_folder + "/";
    run_dir_initialized = true;
    
    qDebug() << "Project configuration loaded from:" << projectFilePath;
    qDebug() << "Project directory:" << projectDir;
    qDebug() << "Project name:" << m_project_config.project_name;
    qDebug() << "AP count:" << m_project_config.ap_config_files.size();
    qDebug() << "STA count:" << m_project_config.sta_config_files.size();
    
    // Now load the actual configuration files
    m_ap_config_list.clear();
    m_sta_config_list.clear();
    
    // Load AP configurations
    for (const QString &apFile : m_project_config.ap_config_files) {
        QString fullPath = m_project_config.ap_config_folder + "/" + apFile;
        QFile apFileObj(fullPath);
        if (apFileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument apDoc = QJsonDocument::fromJson(apFileObj.readAll());
            if (apDoc.isObject()) {
                m_ap_config_list.append(apDoc.object());
                qDebug() << "Loaded AP config:" << fullPath;
            }
            apFileObj.close();
        } else {
            qWarning() << "Failed to open AP config file:" << fullPath;
        }
    }
    
    // Load STA configurations
    for (const QString &staFile : m_project_config.sta_config_files) {
        QString fullPath = m_project_config.sta_config_folder + "/" + staFile;
        QFile staFileObj(fullPath);
        if (staFileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument staDoc = QJsonDocument::fromJson(staFileObj.readAll());
            if (staDoc.isObject()) {
                m_sta_config_list.append(staDoc.object());
                qDebug() << "Loaded STA config:" << fullPath;
            }
            staFileObj.close();
        } else {
            qWarning() << "Failed to open STA config file:" << fullPath;
        }
    }
    
    // Load general configuration
    QString generalPath = m_project_config.general_config_folder + "/" + m_project_config.general_config_file;
    QFile generalFile(generalPath);
    if (generalFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument generalDoc = QJsonDocument::fromJson(generalFile.readAll());
        if (generalDoc.isObject()) {
            m_building_config = generalDoc.object();
            qDebug() << "Loaded general config:" << generalPath;
        }
        generalFile.close();
    } else {
        qWarning() << "Failed to open general config file:" << generalPath;
    }
    
    return true;
}

ProjectConfig JsonHelper::GetCurrentProjectConfig() const
{
    return m_project_config;
}

void JsonHelper::SanitizeNodeConfigs()
{
    auto dedupeConfigs = [](const QVector<QJsonObject> &configs) {
        QVector<QJsonObject> sanitized;
        QSet<int> seenIds;
        for (int i = configs.size() - 1; i >= 0; --i)
        {
            const int id = configs[i]["Id"].toInt(-1);
            if (id < 0 || seenIds.contains(id))
            {
                continue;
            }
            seenIds.insert(id);
            sanitized.prepend(configs[i]);
        }
        return sanitized;
    };

    auto rebuildPositions = [](const QVector<QJsonObject> &configs, const char *positionKey) {
        QJsonArray entries;
        for (const auto &cfg : configs)
        {
            const int id = cfg["Id"].toInt(-1);
            const QJsonArray position = cfg.value(positionKey).toArray();
            if (id < 0 || position.size() < 3)
            {
                continue;
            }
            QJsonObject entry;
            entry["id"] = id;
            entry["pos"] = position;
            entries.append(entry);
        }
        return entries;
    };

    m_ap_config_list = dedupeConfigs(m_ap_config_list);
    m_sta_config_list = dedupeConfigs(m_sta_config_list);
    m_building_config["Ap_pos_list"] = rebuildPositions(m_ap_config_list, "Position");
    m_building_config["Sta_pos_list"] = rebuildPositions(m_sta_config_list, "Position");
    m_building_config["Ap_num"] = m_ap_config_list.size();
    m_building_config["Sta_num"] = m_sta_config_list.size();
}

QVector<QPair<QString, QString>> JsonHelper::GetAvailableNodeTargets() const
{
    QVector<QPair<QString, QString>> targets;

    for (const auto &cfg : m_ap_config_list)
    {
        const int id = cfg["Id"].toInt();
        const QString value = QString("AP_%1").arg(id);
        const QString ssid = cfg["Ssid"].toString().trimmed();
        const QString label = ssid.isEmpty()
                                  ? QString("AP %1").arg(id)
                                  : QString("AP %1 (%2)").arg(id).arg(ssid);
        targets.append(qMakePair(label, value));
    }

    for (const auto &cfg : m_sta_config_list)
    {
        const int id = cfg["Id"].toInt();
        const QString value = QString("STA_%1").arg(id);
        const QString ssid = cfg["Ssid"].toString().trimmed();
        const QString label = ssid.isEmpty()
                                  ? QString("STA %1").arg(id)
                                  : QString("STA %1 (%2)").arg(id).arg(ssid);
        targets.append(qMakePair(label, value));
    }

    return targets;
}

bool JsonHelper::PersistAllJson()
{
    if (General_file_path.isEmpty())
    {
        return false;
    }

    SanitizeNodeConfigs();

    const QString generalPath = General_file_path + "General.json";
    if (!SaveJsonObjToRoute(m_building_config, generalPath))
    {
        return false;
    }

    auto cleanupNodeJsonFiles = [](const QString &folderPath, const QString &prefix) {
        if (folderPath.isEmpty())
        {
            return;
        }

        QDir dir(folderPath);
        if (!dir.exists())
        {
            return;
        }

        const QStringList files = dir.entryList(QStringList() << (prefix + "*.json"), QDir::Files);
        for (const QString &fileName : files)
        {
            dir.remove(fileName);
        }
    };

    const QString apFolder = !m_project_config.ap_config_folder.isEmpty()
                                 ? m_project_config.ap_config_folder
                                 : QFileInfo(Ap_file_path).path();
    const QString staFolder = !m_project_config.sta_config_folder.isEmpty()
                                  ? m_project_config.sta_config_folder
                                  : QFileInfo(Sta_file_path).path();

    cleanupNodeJsonFiles(apFolder, "Ap_");
    cleanupNodeJsonFiles(staFolder, "Sta_");

    for (int i = 0; i < m_ap_config_list.size(); ++i)
    {
        QString path;
        if (!m_project_config.ap_config_folder.isEmpty() &&
            i < m_project_config.ap_config_files.size())
        {
            path = m_project_config.ap_config_folder + "/" + m_project_config.ap_config_files[i];
        }
        else
        {
            path = Ap_file_path + QString::number(i) + ".json";
        }

        if (!SaveJsonObjToRoute(m_ap_config_list[i], path))
        {
            return false;
        }
    }

    for (int i = 0; i < m_sta_config_list.size(); ++i)
    {
        QString path;
        if (!m_project_config.sta_config_folder.isEmpty() &&
            i < m_project_config.sta_config_files.size())
        {
            path = m_project_config.sta_config_folder + "/" + m_project_config.sta_config_files[i];
        }
        else
        {
            path = Sta_file_path + QString::number(i) + ".json";
        }

        if (!SaveJsonObjToRoute(m_sta_config_list[i], path))
        {
            return false;
        }
    }

    return true;
}

bool JsonHelper::RemoveNodeConfig(const QString &nodeType, int id)
{
    const bool isAp = nodeType.compare("AP", Qt::CaseInsensitive) == 0;
    const bool isSta = nodeType.compare("STA", Qt::CaseInsensitive) == 0;
    if (!isAp && !isSta)
    {
        return false;
    }

    auto removeConfigById = [id](QVector<QJsonObject> &configs) {
        for (int i = 0; i < configs.size(); ++i)
        {
            if (configs[i]["Id"].toInt() == id)
            {
                configs.removeAt(i);
                return true;
            }
        }
        return false;
    };

    auto removePosById = [id](QJsonArray &entries) {
        for (int i = 0; i < entries.size(); ++i)
        {
            if (entries[i].toObject()["id"].toInt() == id)
            {
                entries.removeAt(i);
                return true;
            }
        }
        return false;
    };

    bool removed = false;
    if (isAp)
    {
        removed = removeConfigById(m_ap_config_list);
        QJsonArray apList = m_building_config["Ap_pos_list"].toArray();
        removePosById(apList);
        m_building_config["Ap_pos_list"] = apList;
        m_building_config["Ap_num"] = m_ap_config_list.size();
    }
    else
    {
        removed = removeConfigById(m_sta_config_list);
        QJsonArray staList = m_building_config["Sta_pos_list"].toArray();
        removePosById(staList);
        m_building_config["Sta_pos_list"] = staList;
        m_building_config["Sta_num"] = m_sta_config_list.size();
    }

    if (!removed)
    {
        return false;
    }

    return PersistAllJson();
}

void JsonHelper::ClearNodeConfigs(const QString &nodeType)
{
    const bool isAp = nodeType.compare("AP", Qt::CaseInsensitive) == 0;
    const bool isSta = nodeType.compare("STA", Qt::CaseInsensitive) == 0;
    if (!isAp && !isSta)
    {
        return;
    }

    if (isAp)
    {
        m_ap_config_list.clear();
        m_building_config["Ap_pos_list"] = QJsonArray();
        m_building_config["Ap_num"] = 0;
    }
    else
    {
        m_sta_config_list.clear();
        m_building_config["Sta_pos_list"] = QJsonArray();
        m_building_config["Sta_num"] = 0;
    }

    PersistAllJson();
}

QVector<TrafficConfig> JsonHelper::GetNodeTrafficConfigs(const QString &nodeType, int id) const
{
    const QVector<QJsonObject> *configs = nullptr;
    if (nodeType.compare("AP", Qt::CaseInsensitive) == 0)
    {
        configs = &m_ap_config_list;
    }
    else if (nodeType.compare("STA", Qt::CaseInsensitive) == 0)
    {
        configs = &m_sta_config_list;
    }
    else
    {
        return {};
    }

    for (const auto &cfg : *configs)
    {
        if (cfg["Id"].toInt() != id)
        {
            continue;
        }

        QVector<TrafficConfig> flows;
        const QJsonArray flowArray =
            cfg.value("Traffic").toObject().value("Flows").toArray();
        for (const auto &flowValue : flowArray)
        {
            const QJsonObject flowObj = flowValue.toObject();
            TrafficConfig flow;
            flow.Flow_id = flowObj["FlowId"].toString();
            flow.Protocol = flowObj["Protocol"].toString("UDP");
            flow.Destination = flowObj["Destination"].toString();
            flow.StartTime = flowObj["StartTime"].toDouble();
            flow.EndTime = flowObj["EndTime"].toDouble();
            flow.Tos = flowObj["Tos"].toInt();

            const QString flowType = flowObj["FlowType"].toString("OnOff");
            if (flowType == "CBR")
            {
                flow.flowType = TrafficConfig::CBR;
                flow.packetSize = flowObj["PacketSize"].toInt(512);
                flow.interval = flowObj["Interval"].toDouble(0.001);
                flow.maxPackets = flowObj["MaxPackets"].toInt(0);
            }
            else if (flowType == "Bulk")
            {
                flow.flowType = TrafficConfig::Bulk;
                flow.maxBytes = flowObj["MaxBytes"].toInt(0);
                flow.packetSize = flowObj["PacketSize"].toInt(512);
            }
            else
            {
                flow.flowType = TrafficConfig::OnOff;
                flow.dataRate = flowObj["DataRate"].toDouble(1.0);
                flow.packetSize = flowObj["PacketSize"].toInt(512);
                flow.ontimeType = flowObj["OntimeType"].toString("Constant");
                flow.offtimeType = flowObj["OfftimeType"].toString("Constant");
                flow.maxBytes = flowObj["MaxBytes"].toInt(0);

                const QJsonObject onObj = flowObj["OntimeParams"].toObject();
                for (auto it = onObj.begin(); it != onObj.end(); ++it)
                {
                    flow.ontimeParams[it.key()] = it.value().toDouble();
                }

                const QJsonObject offObj = flowObj["OfftimeParams"].toObject();
                for (auto it = offObj.begin(); it != offObj.end(); ++it)
                {
                    flow.offtimeParams[it.key()] = it.value().toDouble();
                }
            }

            flows.push_back(flow);
        }
        return flows;
    }

    return {};
}

bool JsonHelper::SetNodeTrafficConfigs(const QString &nodeType,
                                       int id,
                                       const QVector<TrafficConfig> &flows)
{
    QVector<QJsonObject> *configs = nullptr;
    if (nodeType.compare("AP", Qt::CaseInsensitive) == 0)
    {
        configs = &m_ap_config_list;
    }
    else if (nodeType.compare("STA", Qt::CaseInsensitive) == 0)
    {
        configs = &m_sta_config_list;
    }
    else
    {
        return false;
    }

    for (auto &cfg : *configs)
    {
        if (cfg["Id"].toInt() != id)
        {
            continue;
        }

        QJsonArray flowArray;
        for (const auto &flow : flows)
        {
            QJsonObject flowObj;
            flowObj["FlowId"] = flow.Flow_id;
            flowObj["Protocol"] = flow.Protocol;
            flowObj["Destination"] = flow.Destination;
            flowObj["StartTime"] = flow.StartTime;
            flowObj["EndTime"] = flow.EndTime;
            flowObj["Tos"] = flow.Tos;

            if (flow.flowType == TrafficConfig::CBR)
            {
                flowObj["FlowType"] = "CBR";
                flowObj["PacketSize"] = flow.packetSize;
                flowObj["Interval"] = flow.interval;
                flowObj["MaxPackets"] = flow.maxPackets;
            }
            else if (flow.flowType == TrafficConfig::Bulk)
            {
                flowObj["FlowType"] = "Bulk";
                flowObj["MaxBytes"] = flow.maxBytes;
                flowObj["PacketSize"] = flow.packetSize;
            }
            else
            {
                flowObj["FlowType"] = "OnOff";
                flowObj["DataRate"] = flow.dataRate;
                flowObj["PacketSize"] = flow.packetSize;
                flowObj["OntimeType"] = flow.ontimeType;
                flowObj["OfftimeType"] = flow.offtimeType;
                flowObj["MaxBytes"] = flow.maxBytes;

                QJsonObject onObj;
                for (auto it = flow.ontimeParams.begin(); it != flow.ontimeParams.end(); ++it)
                {
                    onObj[it.key()] = it.value();
                }
                flowObj["OntimeParams"] = onObj;

                QJsonObject offObj;
                for (auto it = flow.offtimeParams.begin(); it != flow.offtimeParams.end(); ++it)
                {
                    offObj[it.key()] = it.value();
                }
                flowObj["OfftimeParams"] = offObj;
            }

            flowArray.append(flowObj);
        }

        QJsonObject trafficObj = cfg.value("Traffic").toObject();
        trafficObj["Flows"] = flowArray;
        cfg["Traffic"] = trafficObj;
        return PersistAllJson();
    }

    return false;
}
