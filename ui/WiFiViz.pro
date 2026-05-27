QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    JsonHelper.cpp \
    antennas.cpp \
    ap_config.cpp \
    config_ui_style.cpp \
    configgraphicsview.cpp \
    edca_config.cpp \
    greeting.cpp \
    main.cpp \
    mainwindow.cpp \
    node_config.cpp \
    node_traffic_panel.cpp \
    page1_model_chose.cpp \
    simu_config.cpp \
    ppdu_timeline_view.cpp\
    ppdu_adapter.cpp\
    ppdu_visual_item.cpp\
    ppdu_detail_window.cpp\
    qt_ppdu_reader.cpp\
    shm.cpp\
    ppdu_info_overlay.cpp\
    legend_overlay.cpp\
    timeline_display.cpp \
    timeline_window.cpp \
    visualizer_config.cpp\
    utils.cpp\
    indus_widget.cpp\
    image_viewer.cpp \
    throughput_chart.cpp \
    latency_chart.cpp \
    ppdu_composition_chart.cpp \
    node_throughput_chart.cpp \
    rx_outcome_chart.cpp \
    phy_state_pie_chart.cpp \
    mcs_distribution_chart.cpp \
    process_terminal.cpp

HEADERS += \
    JsonHelper.h \
    antennas.h \
    ap_config.h \
    config_ui_style.h \
    configgraphicsview.h \
    edca_config.h \
    greeting.h \
    mainwindow.h \
    node_config.h \
    node_traffic_panel.h \
    page1_model_chose.h \
    simu_config.h \
    ppdu_timeline_view.h\
    ppdu_adapter.h\
    ppdu_visual_item.h\
    ppdu_detail_window.h\
    qt_ppdu_reader.h\
    shm.h\
    ppdu_info_overlay.h\
    legend_overlay.h\
    timeline_display.h \
    timeline_window.h \
    visualizer_mode.h \
    chart_filter.h \
    visualizer_config.h\
    utils.h\
    indus_widget.h\
    image_viewer.h \
    throughput_chart.h \
    latency_chart.h \
    ppdu_composition_chart.h \
    node_throughput_chart.h \
    rx_outcome_chart.h \
    phy_state_pie_chart.h \
    mcs_distribution_chart.h \
    process_terminal.h

FORMS += \
    antennas.ui \
    ap_config.ui \
    edca_config.ui \
    greeting.ui \
    mainwindow.ui \
    node_config.ui \
    page1_model_chose.ui \
    simu_config.ui\
    timeline_display.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
