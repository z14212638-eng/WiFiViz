#include "config_ui_style.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLCDNumber>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTextBrowser>
#include <QToolBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDebug>

namespace
{
QWidget* Find(QWidget* root, const char* name)
{
    if (!root)
    {
        return nullptr;
    }

    return root->findChild<QWidget*>(QString::fromLatin1(name));
}

template <typename T>
T* FindAs(QWidget* root, const char* name)
{
    if (!root)
    {
        return nullptr;
    }

    return root->findChild<T*>(QString::fromLatin1(name));
}

QWidget* Use(QWidget* root, const char* name)
{
    auto* widget = Find(root, name);
    if (widget)
    {
        widget->setParent(nullptr);
    }
    return widget;
}

template <typename T>
T* UseAs(QWidget* root, const char* name)
{
    auto* widget = FindAs<T>(root, name);
    if (widget)
    {
        widget->setParent(nullptr);
    }
    return widget;
}

void ClearLayout(QLayout* layout)
{
    if (!layout)
    {
        return;
    }

    while (auto* item = layout->takeAt(0))
    {
        if (auto* childLayout = item->layout())
        {
            ClearLayout(childLayout);
            delete childLayout;
        }
        delete item;
    }
}

QLabel* TextLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("fieldLabel");
    label->setWordWrap(true);
    return label;
}

void AddField(QGridLayout* grid, int row, int column, const QString& label, QWidget* field)
{
    if (!grid)
    {
        return;
    }

    grid->addWidget(TextLabel(label, grid->parentWidget()), row * 2, column);
    if (field)
    {
        grid->addWidget(field, row * 2 + 1, column);
    }
    else
    {
        qWarning() << "ConfigUiStyle missing field widget:" << label;
    }
}

QFrame* Section(QWidget* parent, const QString& title)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName("configSection");
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);

    auto* label = new QLabel(title, frame);
    label->setObjectName("sectionTitle");
    layout->addWidget(label);
    return frame;
}

QGridLayout* SectionGrid(QFrame* section, int columns = 2)
{
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(10);
    for (int i = 0; i < columns; ++i)
    {
        grid->setColumnStretch(i, 1);
    }
    qobject_cast<QVBoxLayout*>(section->layout())->addLayout(grid);
    return grid;
}

QWidget* Page(QTabWidget* tabs, int index)
{
    auto* page = tabs->widget(index);
    if (!page)
    {
        return nullptr;
    }

    ClearLayout(page->layout());
    delete page->layout();

    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(page);
    scroll->setObjectName("configScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("configPage");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(14);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    outer->addWidget(scroll);
    return content;
}

void InsertSection(QWidget* page, QFrame* section)
{
    if (!page || !section)
    {
        return;
    }

    auto* layout = qobject_cast<QVBoxLayout*>(page->layout());
    if (!layout)
    {
        return;
    }

    layout->insertWidget(layout->count() - 1, section);
}

void AddRootFooter(QWidget* root, QWidget* widget)
{
    if (!root || !widget)
    {
        return;
    }

    auto* layout = root->layout();
    if (!layout)
    {
        return;
    }

    if (auto* boxLayout = qobject_cast<QBoxLayout*>(layout))
    {
        boxLayout->addWidget(widget);
        return;
    }

    if (auto* gridLayout = qobject_cast<QGridLayout*>(layout))
    {
        gridLayout->addWidget(widget, gridLayout->rowCount(), 0, 1, gridLayout->columnCount());
        return;
    }

    layout->addWidget(widget);
}

QWidget* PlainPage(QWidget* root)
{
    ClearLayout(root->layout());
    delete root->layout();

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(0);

    auto* scroll = new QScrollArea(root);
    scroll->setObjectName("configScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    content->setObjectName("configPage");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 18, 18, 18);
    contentLayout->setSpacing(14);
    contentLayout->addStretch(1);

    scroll->setWidget(content);
    layout->addWidget(scroll);
    return content;
}

void AddActionRow(QFrame* section, std::initializer_list<QPushButton*> buttons)
{
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 4, 0, 0);
    row->setSpacing(10);
    row->addStretch(1);
    for (auto* button : buttons)
    {
        if (button)
        {
            row->addWidget(button);
        }
    }
    qobject_cast<QVBoxLayout*>(section->layout())->addLayout(row);
}

void PolishCommon(QWidget* root, QTabWidget* tabs, bool polishLayout = true)
{
    root->setAttribute(Qt::WA_StyledBackground, true);
    if (polishLayout)
    {
        root->setMinimumSize(860, 760);

        if (auto* rootLayout = root->layout())
        {
            rootLayout->setContentsMargins(14, 14, 14, 14);
            rootLayout->setSpacing(0);
        }

        if (tabs)
        {
            tabs->setDocumentMode(true);
            tabs->setUsesScrollButtons(true);
        }

        const auto buttons = root->findChildren<QPushButton*>();
        for (auto* button : buttons)
        {
            button->setMinimumHeight(36);
            button->setCursor(Qt::PointingHandCursor);
        }

        const auto spinBoxes = root->findChildren<QAbstractSpinBox*>();
        for (auto* spinBox : spinBoxes)
        {
            spinBox->setMinimumHeight(34);
        }

        const auto combos = root->findChildren<QComboBox*>();
        for (auto* combo : combos)
        {
            combo->setMinimumHeight(34);
        }

        const auto edits = root->findChildren<QLineEdit*>();
        for (auto* edit : edits)
        {
            edit->setMinimumHeight(34);
        }

        const auto tables = root->findChildren<QTableWidget*>();
        for (auto* table : tables)
        {
            table->setAlternatingRowColors(true);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->setShowGrid(false);
            table->setMinimumHeight(320);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
            table->horizontalHeader()->setMinimumSectionSize(92);
        }
    }

    root->setStyleSheet(R"(
        QWidget#node_config, QWidget#Ap_config, QWidget#Simu_Config,
        QWidget#Page1_model_chose, QDialog#Edca_config, QDialog#Antenna,
        QDialog#flow_dialog, QDialog#RandomVariableDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #FFFFFF, stop:1 #F4F8FC);
            color: #152033;
            font-family: "Inter", "Segoe UI", "Noto Sans", Arial, sans-serif;
            font-size: 13px;
        }
        QTabWidget::pane {
            background: #FFFFFF;
            border: 1px solid #D6E2EE;
            border-radius: 14px;
            top: -1px;
        }
        QTabWidget > QWidget, QTabWidget QWidget::pane, QWidget#tab, QWidget#tab_2,
        QWidget#tab_3, QWidget#tab_4, QWidget#tab_5, QWidget#tab_6 {
            background: #FFFFFF;
        }
        QTabBar::tab {
            background: #F4F8FC;
            color: #6C7B8D;
            border: 1px solid #D6E2EE;
            border-bottom: none;
            padding: 10px 20px;
            margin-right: 6px;
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
            font-weight: 700;
        }
        QTabBar::tab:selected {
            background: #FFFFFF;
            color: #2E72D2;
        }
        QTabBar::tab:disabled {
            color: #A9B6C5;
            background: #EEF3F8;
        }
        QScrollArea#configScrollArea, QWidget#configPage {
            background: transparent;
            border: none;
        }
        QFrame#configSection, QGroupBox, QFrame#frame, QFrame#frame_2, QFrame#frame_3 {
            background: #FFFFFF;
            border: 1px solid #D6E2EE;
            border-radius: 18px;
        }
        QLabel#sectionTitle {
            color: #122033;
            font-size: 16px;
            font-weight: 800;
        }
        QLabel#fieldLabel {
            color: #6C7B8D;
            font-weight: 700;
        }
        QLabel {
            color: #243447;
        }
        QCheckBox {
            spacing: 8px;
            color: #243447;
            font-weight: 700;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 5px;
            border: 1px solid #B9C9D9;
            background: #FFFFFF;
        }
        QCheckBox::indicator:checked {
            background: #7A838C;
            border-color: #69727B;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF;
            border: 1px solid #C8D7E6;
            border-radius: 8px;
            padding: 4px 8px;
            color: #152033;
            selection-background-color: #DDEBFF;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #7FB9FF;
            background: #FFFFFF;
        }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
            background: #EEF3F8;
            color: #8A98A8;
            border-color: #D6E2EE;
        }
        QPushButton {
            background: #DCE1E6;
            border: 1px solid #B8C2CC;
            border-radius: 10px;
            padding: 7px 16px;
            color: #243447;
            font-weight: 800;
        }
        QPushButton:hover {
            background: #E8ECEF;
            border-color: #7FB9FF;
        }
        QPushButton:pressed {
            background: #C9D0D6;
        }
        QPushButton#pushButton, QPushButton#pushButton_4,
        QPushButton#pushButton_6, QPushButton#pushButton_7,
        QPushButton#pushButton_5, QPushButton#saveButton {
            background: #C9D0D6;
            border-color: #AEB8C2;
            color: #243447;
        }
        QPushButton#pushButton:hover, QPushButton#pushButton_4:hover,
        QPushButton#pushButton_6:hover, QPushButton#pushButton_7:hover,
        QPushButton#pushButton_5:hover, QPushButton#saveButton:hover {
            background: #DCE1E6;
            border-color: #7FB9FF;
        }
        QTableWidget {
            background: #FFFFFF;
            alternate-background-color: #F4F8FC;
            border: 1px solid #D6E2EE;
            border-radius: 14px;
            gridline-color: transparent;
            color: #152033;
        }
        QTableWidget::item {
            padding: 8px;
            border-bottom: 1px solid #EDF3F9;
        }
        QTableWidget::item:selected {
            background: #DDEBFF;
            color: #152033;
        }
        QHeaderView::section {
            background: #F4F8FC;
            color: #243447;
            border: none;
            border-bottom: 1px solid #D6E2EE;
            padding: 8px;
            font-weight: 800;
        }
        QListWidget, QTextBrowser, QTextEdit, QToolBox {
            background: #FFFFFF;
            border: 1px solid #D6E2EE;
            border-radius: 14px;
            color: #152033;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #EDF3F9;
        }
        QListWidget::item:selected {
            background: #DDEBFF;
            color: #152033;
        }
        QToolBox::tab {
            background: #F4F8FC;
            border: 1px solid #D6E2EE;
            border-radius: 12px;
            padding: 8px;
            font-weight: 800;
            color: #243447;
        }
    )");
}

void BuildPositionPage(QWidget* root, QTabWidget* tabs, bool ap)
{
    auto* page = Page(tabs, 0);
    auto* section = Section(page, "Placement");
    auto* grid = SectionGrid(section, 3);

    grid->addWidget(Use(root, ap ? "checkBox_4" : "checkBox_2"), 0, 0, 1, 3);
    AddField(grid, 1, 0, "X coordinate", Use(root, "doubleSpinBox"));
    AddField(grid, 1, 1, "Y coordinate", Use(root, "doubleSpinBox_2"));
    AddField(grid, 1, 2, "Z coordinate", Use(root, "doubleSpinBox_3"));
    AddActionRow(section, {UseAs<QPushButton>(root, "pushButton")});
    InsertSection(page, section);
}

void BuildNodeMobilityPage(QWidget* root, QTabWidget* tabs)
{
    auto* page = Page(tabs, 1);
    auto* model = Section(page, "Mobility Model");
    auto* modelGrid = SectionGrid(model, 2);
    modelGrid->addWidget(Use(root, "checkBox"), 0, 0, 1, 2);
    AddField(modelGrid, 1, 0, "Route model", Use(root, "comboBox"));
    AddField(modelGrid, 1, 1, "Change mode", Use(root, "comboBox_2"));
    InsertSection(page, model);

    auto* bounds = Section(page, "Movement Boundaries");
    auto* boundsGrid = SectionGrid(bounds, 4);
    AddField(boundsGrid, 0, 0, "min X", Use(root, "doubleSpinBox_8"));
    AddField(boundsGrid, 0, 1, "max X", Use(root, "doubleSpinBox_9"));
    AddField(boundsGrid, 0, 2, "min Y", Use(root, "doubleSpinBox_10"));
    AddField(boundsGrid, 0, 3, "max Y", Use(root, "doubleSpinBox_11"));
    InsertSection(page, bounds);

    auto* timing = Section(page, "Motion Parameters");
    auto* timingGrid = SectionGrid(timing, 3);
    AddField(timingGrid, 0, 0, "Time interval", Use(root, "doubleSpinBox_4"));
    AddField(timingGrid, 0, 1, "Distance interval", Use(root, "doubleSpinBox_5"));
    AddField(timingGrid, 0, 2, "Random velocity", Use(root, "doubleSpinBox_7"));
    AddActionRow(timing, {UseAs<QPushButton>(root, "pushButton_4")});
    InsertSection(page, timing);
}

void BuildApMobilityPage(QWidget* root, QTabWidget* tabs)
{
    auto* page = Page(tabs, 1);
    auto* model = Section(page, "Mobility Model");
    auto* modelGrid = SectionGrid(model, 2);
    modelGrid->addWidget(Use(root, "checkBox_3"), 0, 0, 1, 2);
    AddField(modelGrid, 1, 0, "Route model", Use(root, "comboBox"));
    AddField(modelGrid, 1, 1, "Change mode", Use(root, "comboBox_2"));
    InsertSection(page, model);

    auto* bounds = Section(page, "Movement Boundaries");
    auto* boundsGrid = SectionGrid(bounds, 4);
    AddField(boundsGrid, 0, 0, "min X", Use(root, "doubleSpinBox_14"));
    AddField(boundsGrid, 0, 1, "max X", Use(root, "doubleSpinBox_15"));
    AddField(boundsGrid, 0, 2, "min Y", Use(root, "doubleSpinBox_16"));
    AddField(boundsGrid, 0, 3, "max Y", Use(root, "doubleSpinBox_17"));
    InsertSection(page, bounds);

    auto* timing = Section(page, "Motion Parameters");
    auto* timingGrid = SectionGrid(timing, 3);
    AddField(timingGrid, 0, 0, "Time interval", Use(root, "doubleSpinBox_4"));
    AddField(timingGrid, 0, 1, "Distance interval", Use(root, "doubleSpinBox_5"));
    AddField(timingGrid, 0, 2, "Random velocity", Use(root, "doubleSpinBox_18"));
    AddActionRow(timing, {UseAs<QPushButton>(root, "pushButton_4")});
    InsertSection(page, timing);
}

void BuildCommonPhySections(QWidget* root, QWidget* page, bool ap)
{
    auto* identity = Section(page, "Identity & Wireless Mode");
    auto* identityGrid = SectionGrid(identity, 3);
    AddField(identityGrid, 0, 0, "SSID", Use(root, "lineEdit"));
    AddField(identityGrid, 0, 1, "Standard", Use(root, "comboBox_5"));
    AddField(identityGrid, 0, 2, "PHY model", Use(root, "comboBox_4"));
    AddField(identityGrid, 1, 0, "Band", Use(root, "comboBox_3"));
    AddField(identityGrid, 1, 1, "Channel", Use(root, "spinBox"));
    AddField(identityGrid, 1, 2, "Bandwidth", Use(root, "comboBox_16"));
    AddField(identityGrid, 2, 0, "Guard interval", Use(root, "comboBox_15"));
    AddField(identityGrid, 2, 1, "Tx power", Use(root, "doubleSpinBox_6"));
    AddField(identityGrid, 2, 2, "Rate control", Use(root, "comboBox_7"));
    InsertSection(page, identity);

    auto* mac = Section(page, "MAC Features");
    auto* macGrid = SectionGrid(mac, 3);
    macGrid->addWidget(Use(root, "checkBox_5"), 0, 0);
    AddField(macGrid, 1, 0, "RTS/CTS threshold", Use(root, "spinBox_3"));
    AddField(macGrid, 1, 1, "Tx queue", Use(root, "comboBox_8"));
    macGrid->addWidget(Use(root, "checkBox_6"), 0, 1);
    AddField(macGrid, 2, 0, "EDCA profile", Use(root, "comboBox_6"));
    macGrid->addWidget(Use(root, "pushButton_8"), 5, 1);
    macGrid->addWidget(Use(root, "pushButton_9"), 5, 2);
    if (!ap)
    {
        macGrid->addWidget(Use(root, "checkBox_7"), 0, 2);
        macGrid->addWidget(Use(root, "checkBox_12"), 2, 1);
        AddField(macGrid, 2, 2, "Max missed beacons", Use(root, "spinBox_4"));
        AddField(macGrid, 3, 0, "Probe timeout", Use(root, "spinBox_5"));
    }
    InsertSection(page, mac);
}

void BuildNodePhyPage(QWidget* root, QTabWidget* tabs)
{
    auto* page = Page(tabs, 2);
    BuildCommonPhySections(root, page, false);
    auto* actions = Section(page, "Apply Configuration");
    AddActionRow(actions, {UseAs<QPushButton>(root, "pushButton_6")});
    InsertSection(page, actions);
}

void BuildApPhyPage(QWidget* root, QTabWidget* tabs)
{
    auto* page = Page(tabs, 2);
    BuildCommonPhySections(root, page, true);

    auto* timing = Section(page, "PHY Timing & Thresholds");
    auto* timingGrid = SectionGrid(timing, 3);
    AddField(timingGrid, 0, 0, "Slot", Use(root, "doubleSpinBox_7"));
    AddField(timingGrid, 0, 1, "SIFS", Use(root, "doubleSpinBox_8"));
    AddField(timingGrid, 0, 2, "Rx sensitivity", Use(root, "doubleSpinBox_9"));
    AddField(timingGrid, 1, 0, "CCA threshold", Use(root, "doubleSpinBox_10"));
    AddField(timingGrid, 1, 1, "CCA sensitivity", Use(root, "doubleSpinBox_11"));
    InsertSection(page, timing);

    auto* beacon = Section(page, "Beacon");
    auto* beaconGrid = SectionGrid(beacon, 3);
    beaconGrid->addWidget(Use(root, "checkBox"), 0, 0);
    beaconGrid->addWidget(Use(root, "checkBox_2"), 0, 1);
    AddField(beaconGrid, 1, 0, "Beacon interval", Use(root, "doubleSpinBox_12"));
    AddField(beaconGrid, 1, 1, "Beacon rate", Use(root, "doubleSpinBox_13"));
    InsertSection(page, beacon);

    auto* actions = Section(page, "Apply Configuration");
    AddActionRow(actions, {UseAs<QPushButton>(root, "pushButton_6")});
    InsertSection(page, actions);
}

void BuildTrafficPage(QWidget* root, QTabWidget* tabs)
{
    auto* page = Page(tabs, 3);
    auto* section = Section(page, "Configured Flows");
    auto* layout = qobject_cast<QVBoxLayout*>(section->layout());
    layout->addWidget(Use(root, "tableWidget"));
    AddActionRow(section,
                 {UseAs<QPushButton>(root, "pushButton_3"),
                  UseAs<QPushButton>(root, "pushButton_2"),
                  UseAs<QPushButton>(root, "pushButton_7")});
    InsertSection(page, section);
}

void BuildEdcaAccessCategory(QWidget* root,
                             QTabWidget* tabs,
                             int index,
                             const QString& title,
                             const char* cwMin,
                             const char* cwMax,
                             const char* aifsn,
                             const char* txop)
{
    auto* page = Page(tabs, index);
    auto* section = Section(page, title);
    auto* grid = SectionGrid(section, 4);
    AddField(grid, 0, 0, "CW min", Use(root, cwMin));
    AddField(grid, 0, 1, "CW max", Use(root, cwMax));
    AddField(grid, 0, 2, "AIFSN", Use(root, aifsn));
    AddField(grid, 0, 3, "TXOP limit", Use(root, txop));
    InsertSection(page, section);
}

void BuildFlowTab(QWidget* root,
                  QTabWidget* tabs,
                  int index,
                  const QString& title,
                  const QVector<QPair<QString, const char*>>& fields)
{
    auto* page = Page(tabs, index);
    auto* section = Section(page, title);
    auto* grid = SectionGrid(section, 3);
    int row = 0;
    int col = 0;
    for (const auto& field : fields)
    {
        AddField(grid, row, col, field.first, Use(root, field.second));
        ++col;
        if (col == 3)
        {
            col = 0;
            ++row;
        }
    }
    InsertSection(page, section);
}
}

void
ConfigUiStyle::ApplyConfigTheme(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget, false);
}

void
ConfigUiStyle::RebuildSceneChooser(QWidget* root)
{
    PolishCommon(root, FindAs<QTabWidget>(root, "tabWidget"));
    auto* page = PlainPage(root);

    auto* scenes = Section(page, "Simulation Library");
    auto* scenesLayout = qobject_cast<QVBoxLayout*>(scenes->layout());
    scenesLayout->addWidget(Use(root, "toolBox"), 1);
    AddActionRow(scenes,
                 {UseAs<QPushButton>(root, "pushButton_5"),
                  UseAs<QPushButton>(root, "pushButton_3"),
                  UseAs<QPushButton>(root, "pushButton"),
                  UseAs<QPushButton>(root, "load_button"),
                  UseAs<QPushButton>(root, "pushButton_4")});
    InsertSection(page, scenes);

    auto* preview = Section(page, "Scene Preview");
    auto* previewGrid = SectionGrid(preview, 2);
    previewGrid->addWidget(Use(root, "label_3"), 0, 0);
    previewGrid->addWidget(Use(root, "textBrowser_2"), 0, 1);
    InsertSection(page, preview);

    auto* selected = Section(page, "Selected Scene");
    qobject_cast<QVBoxLayout*>(selected->layout())->addWidget(Use(root, "textBrowser"));
    InsertSection(page, selected);
}

void
ConfigUiStyle::RebuildSimulationConfig(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget);
    tabWidget->setTabText(0, "Environment");
    tabWidget->setTabText(1, "Access Points");
    tabWidget->setTabText(2, "Stations");

    auto* envPage = Page(tabWidget, 0);
    auto* building = Section(envPage, "Building & Simulation");
    auto* buildingGrid = SectionGrid(building, 4);
    AddField(buildingGrid, 0, 0, "Width", Use(root, "doubleSpinBox"));
    AddField(buildingGrid, 0, 1, "Depth", Use(root, "doubleSpinBox_2"));
    AddField(buildingGrid, 0, 2, "Height", Use(root, "doubleSpinBox_3"));
    AddField(buildingGrid, 0, 3, "Simulation time", Use(root, "doubleSpinBox_4"));
    AddField(buildingGrid, 1, 0, "Building type", Use(root, "comboBox"));
    AddField(buildingGrid, 1, 1, "Wall type", Use(root, "comboBox_2"));
    AddActionRow(building, {UseAs<QPushButton>(root, "pushButton_7")});
    InsertSection(envPage, building);

    auto* map = Section(envPage, "Topology Map");
    auto* mapLayout = qobject_cast<QVBoxLayout*>(map->layout());
    mapLayout->addWidget(Use(root, "graphicsView"), 1);
    auto* counters = new QHBoxLayout();
    counters->setSpacing(12);
    counters->addWidget(TextLabel("AP count", map));
    counters->addWidget(Use(root, "lcdNumber"));
    counters->addWidget(TextLabel("STA count", map));
    counters->addWidget(Use(root, "lcdNumber_2"));
    counters->addStretch(1);
    mapLayout->addLayout(counters);
    AddActionRow(map,
                 {UseAs<QPushButton>(root, "pushButton_8"),
                  UseAs<QPushButton>(root, "saveButton"),
                  UseAs<QPushButton>(root, "pushButton_9")});
    InsertSection(envPage, map);

    auto* apPage = Page(tabWidget, 1);
    auto* apSection = Section(apPage, "Access Points");
    qobject_cast<QVBoxLayout*>(apSection->layout())->addWidget(Use(root, "tableWidget_2"));
    AddActionRow(apSection,
                 {UseAs<QPushButton>(root, "pushButton_4"),
                  UseAs<QPushButton>(root, "pushButton_5"),
                  UseAs<QPushButton>(root, "pushButton_6")});
    InsertSection(apPage, apSection);

    auto* staPage = Page(tabWidget, 2);
    auto* staSection = Section(staPage, "Stations");
    qobject_cast<QVBoxLayout*>(staSection->layout())->addWidget(Use(root, "tableWidget"));
    AddActionRow(staSection,
                 {UseAs<QPushButton>(root, "pushButton"),
                  UseAs<QPushButton>(root, "pushButton_2"),
                  UseAs<QPushButton>(root, "pushButton_3")});
    InsertSection(staPage, staSection);
}

void
ConfigUiStyle::RebuildNodeConfig(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget);
    tabWidget->setTabText(0, "Position");
    tabWidget->setTabText(1, "Mobility (2D)");
    tabWidget->setTabText(2, "PHY & MAC");
    tabWidget->setTabText(3, "Traffic");
    BuildPositionPage(root, tabWidget, false);
    BuildNodeMobilityPage(root, tabWidget);
    BuildNodePhyPage(root, tabWidget);
    BuildTrafficPage(root, tabWidget);
}

void
ConfigUiStyle::RebuildApConfig(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget);
    tabWidget->setTabText(0, "Position");
    tabWidget->setTabText(1, "Mobility (2D)");
    tabWidget->setTabText(2, "PHY & MAC");
    tabWidget->setTabText(3, "Traffic");
    BuildPositionPage(root, tabWidget, true);
    BuildApMobilityPage(root, tabWidget);
    BuildApPhyPage(root, tabWidget);
    BuildTrafficPage(root, tabWidget);
}

void
ConfigUiStyle::RebuildEdcaConfig(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget);
    tabWidget->setTabText(0, "AC VO");
    tabWidget->setTabText(1, "AC VI");
    tabWidget->setTabText(2, "AC BE");
    tabWidget->setTabText(3, "AC BK");
    tabWidget->setTabText(4, "A-MSDU");
    tabWidget->setTabText(5, "A-MPDU");

    BuildEdcaAccessCategory(root, tabWidget, 0, "Voice Access Category", "spinBox", "spinBox_2", "spinBox_3", "spinBox_4");
    BuildEdcaAccessCategory(root, tabWidget, 1, "Video Access Category", "spinBox_5", "spinBox_7", "spinBox_9", "spinBox_8");
    BuildEdcaAccessCategory(root, tabWidget, 2, "Best Effort Access Category", "spinBox_6", "spinBox_10", "spinBox_11", "spinBox_12");
    BuildEdcaAccessCategory(root, tabWidget, 3, "Background Access Category", "spinBox_13", "spinBox_14", "spinBox_15", "spinBox_16");

    auto* msduPage = Page(tabWidget, 4);
    auto* msdu = Section(msduPage, "A-MSDU Aggregation");
    auto* msduGrid = SectionGrid(msdu, 3);
    msduGrid->addWidget(Use(root, "checkBox"), 0, 0);
    AddField(msduGrid, 1, 0, "Access category", Use(root, "comboBox"));
    AddField(msduGrid, 1, 1, "Max A-MSDU size", Use(root, "spinBox_17"));
    InsertSection(msduPage, msdu);

    auto* mpduPage = Page(tabWidget, 5);
    auto* mpdu = Section(mpduPage, "A-MPDU Aggregation");
    auto* mpduGrid = SectionGrid(mpdu, 3);
    mpduGrid->addWidget(Use(root, "checkBox_2"), 0, 0);
    AddField(mpduGrid, 1, 0, "Access category", Use(root, "comboBox_2"));
    AddField(mpduGrid, 1, 1, "Max A-MPDU size", Use(root, "spinBox_18"));
    AddField(mpduGrid, 1, 2, "Density", Use(root, "spinBox_19"));
    InsertSection(mpduPage, mpdu);

    if (auto* buttonBox = Use(root, "buttonBox"))
    {
        AddRootFooter(root, buttonBox);
    }
}

void
ConfigUiStyle::RebuildAntennaConfig(QWidget* root)
{
    PolishCommon(root, nullptr);
    auto* page = PlainPage(root);
    auto* list = Section(page, "Antenna List");
    qobject_cast<QVBoxLayout*>(list->layout())->addWidget(Use(root, "tableWidget"));
    AddActionRow(list,
                 {UseAs<QPushButton>(root, "pushButton"),
                  UseAs<QPushButton>(root, "pushButton_2")});
    InsertSection(page, list);

    auto* editor = Section(page, "Antenna Editor");
    auto* editorGrid = SectionGrid(editor, 3);
    AddField(editorGrid, 0, 0, "Type", Use(root, "comboBox"));
    AddField(editorGrid, 0, 1, "Max gain", Use(root, "spinBox"));
    AddField(editorGrid, 0, 2, "Beam width", Use(root, "spinBox_2"));
    AddActionRow(editor, {UseAs<QPushButton>(root, "pushButton_3")});
    InsertSection(page, editor);

    if (auto* buttonBox = Use(root, "buttonBox"))
    {
        AddRootFooter(root, buttonBox);
    }
}

void
ConfigUiStyle::RebuildFlowDialog(QWidget* root, QTabWidget* tabWidget)
{
    PolishCommon(root, tabWidget);
    tabWidget->setTabText(0, "OnOff");
    tabWidget->setTabText(1, "CBR");
    tabWidget->setTabText(2, "Bulk");

    BuildFlowTab(root, tabWidget, 0, "OnOff Application",
                 {{"Flow ID", "lineEdit_flowid_onoff"},
                  {"Data rate", "doubleSpinBox_datarate"},
                  {"Packet size", "spinBox_packetsize"},
                  {"On time type", "comboBox_ontime_type"},
                  {"On time value", "lineEdit_ontime_value"},
                  {"Off time type", "comboBox_offtime_type"},
                  {"Off time value", "lineEdit_offtime_value"},
                  {"Max bytes", "spinBox_maxbytes"},
                  {"Protocol", "comboBox_protocol"},
                  {"TOS", "spinBox_tos"},
                  {"Start time", "doubleSpinBox_starttime_onoff"},
                  {"End time", "doubleSpinBox_endtime_onoff"},
                  {"Destination", "comboBox_direction_onoff"}});

    BuildFlowTab(root, tabWidget, 1, "CBR Application",
                 {{"Flow ID", "lineEdit_flowid_cbr"},
                  {"Packet size", "spinBox_packetsize_cbr"},
                  {"Interval", "doubleSpinBox_interval_cbr"},
                  {"Max packets", "spinBox_maxpackets_cbr"},
                  {"Protocol", "comboBox_protocol_cbr"},
                  {"TOS", "spinBox_tos_cbr"},
                  {"Start time", "doubleSpinBox_starttime_cbr"},
                  {"End time", "doubleSpinBox_endtime_cbr"},
                  {"Destination", "comboBox_direction_cbr"}});

    BuildFlowTab(root, tabWidget, 2, "Bulk Application",
                 {{"Flow ID", "lineEdit_flowid_bulk"},
                  {"Max bytes", "spinBox_maxbytes_bulk"},
                  {"Protocol", "comboBox_protocol_bulk"},
                  {"TOS", "spinBox_tos_bulk"},
                  {"Start time", "doubleSpinBox_starttime_bulk"},
                  {"End time", "doubleSpinBox_endtime_bulk"},
                  {"Destination", "comboBox_direction_bulk"}});

    if (auto* buttonBox = Use(root, "buttonBox"))
    {
        AddRootFooter(root, buttonBox);
    }
}

void
ConfigUiStyle::PolishNodeTrafficPanel(QWidget* root)
{
    PolishCommon(root, nullptr);
}
