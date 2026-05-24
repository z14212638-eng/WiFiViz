#pragma once

class QTableWidget;
class QTabWidget;
class QWidget;

namespace ConfigUiStyle
{
void ApplyConfigTheme(QWidget* root, QTabWidget* tabWidget = nullptr);
void RebuildSceneChooser(QWidget* root);
void RebuildSimulationConfig(QWidget* root, QTabWidget* tabWidget);
void RebuildNodeConfig(QWidget* root, QTabWidget* tabWidget);
void RebuildApConfig(QWidget* root, QTabWidget* tabWidget);
void RebuildEdcaConfig(QWidget* root, QTabWidget* tabWidget);
void RebuildAntennaConfig(QWidget* root);
void RebuildFlowDialog(QWidget* root, QTabWidget* tabWidget);
void PolishNodeTrafficPanel(QWidget* root);
}
