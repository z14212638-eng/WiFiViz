cmake_minimum_required(VERSION 3.16)

project(WiFiViz VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# Source files
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    JsonHelper.cpp
    JsonHelper.h
    antennas.cpp
    antennas.h
    antennas.ui
    ap_config.cpp
    ap_config.h
    ap_config.ui
    config_ui_style.cpp
    config_ui_style.h
    configgraphicsview.cpp
    configgraphicsview.h
    edca_config.cpp
    edca_config.h
    edca_config.ui
    greeting.cpp
    greeting.h
    greeting.ui
    node_config.cpp
    node_config.h
    node_config.ui
    page1_model_chose.cpp
    page1_model_chose.h
    page1_model_chose.ui
    simu_config.cpp
    simu_config.h
    simu_config.ui
    ppdu_timeline_view.cpp
    ppdu_timeline_view.h
    ppdu_adapter.cpp
    ppdu_adapter.h
    ppdu_visual_item.cpp
    ppdu_visual_item.h
    ppdu_detail_window.cpp
    ppdu_detail_window.h
    qt_ppdu_reader.cpp
    qt_ppdu_reader.h
    shm.cpp
    shm.h
    ppdu_info_overlay.cpp
    ppdu_info_overlay.h
    legend_overlay.cpp
    legend_overlay.h
    timeline_display.cpp
    timeline_display.h
    timeline_display.ui
    visualizer_config.cpp
    visualizer_config.h
    utils.cpp
    utils.h
    indus_widget.cpp
    indus_widget.h
    image_viewer.cpp
    image_viewer.h
    throughput_chart.cpp
    throughput_chart.h
    latency_chart.cpp
    latency_chart.h
    ppdu_composition_chart.cpp
    ppdu_composition_chart.h
    node_throughput_chart.cpp
    node_throughput_chart.h
    rx_outcome_chart.cpp
    rx_outcome_chart.h
    phy_state_pie_chart.cpp
    phy_state_pie_chart.h
    mcs_distribution_chart.cpp
    mcs_distribution_chart.h
    process_terminal.cpp
    process_terminal.h
    node_traffic_panel.cpp
    node_traffic_panel.h
    timeline_window.cpp
    timeline_window.h
    visualizer_mode.h
    chart_filter.h
)

# Resource files
set(PROJECT_RESOURCES
    resources.qrc
)

# Create executable
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(WiFiVizApp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
        flow_dialog.h flow_dialog.cpp flow_dialog.ui
        random_variable_dialog.h random_variable_dialog.cpp
    )
else()
    add_executable(WiFiVizApp
        ${PROJECT_SOURCES}
        ${PROJECT_RESOURCES}
        flow_dialog.h flow_dialog.cpp flow_dialog.ui
        random_variable_dialog.h random_variable_dialog.cpp
    )
endif()

# Link Qt libraries
target_link_libraries(WiFiVizApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Include directories - needed for custom widgets in UI files
target_include_directories(WiFiVizApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Expose the ns-3 source root at compile time
target_compile_definitions(WiFiVizApp PRIVATE
    NS3_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Set target properties
set_target_properties(WiFiVizApp PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Qt6 specific finalization
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(WiFiVizApp)
endif()

# Build wifiviz-script-generator utility together with the main app
set(NS3_ROOT "${CMAKE_SOURCE_DIR}" CACHE PATH "Path to ns-3 root directory" FORCE)
add_subdirectory(utils)
add_dependencies(WiFiVizApp wifiviz-script-generator)

# Installation rules
install(TARGETS WiFiVizApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Copy sample JSON files to build directory
file(COPY
    Sample_Ap_private.json
    Sample_Sta_private.json
    Sample_public.json
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)
