check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

# The wifiviz library depends on nlohmann/json and Boost.Interprocess, which
# are not bundled by ns-3. Skip the module at configure time when either one is
# missing instead of letting the build fail later at compile time.
find_package(nlohmann_json 3.2.0 QUIET)
find_package(Boost 1.71 QUIET)

set(WIFIVIZ_CMAKE_REQUIRED_INCLUDES_SAVED ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
check_include_file_cxx("boost/interprocess/managed_shared_memory.hpp"
                       WIFIVIZ_HAVE_BOOST_IPC)
set(CMAKE_REQUIRED_INCLUDES ${WIFIVIZ_CMAKE_REQUIRED_INCLUDES_SAVED})

if(NOT nlohmann_json_FOUND)
    message(STATUS "Skipping contrib module 'wifiviz': nlohmann_json (>= 3.2.0) not found")
    return()
endif()

if(NOT Boost_FOUND)
    message(STATUS "Skipping contrib module 'wifiviz': Boost (>= 1.71) not found")
    return()
endif()

if(NOT WIFIVIZ_HAVE_BOOST_IPC)
    message(STATUS "Skipping contrib module 'wifiviz': Boost.Interprocess headers not found")
    return()
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/wifiviz-examples-test-suite.cc
        )
endif()

# Inherit ns-3's C++ standard instead of forcing C++23 for this module.

build_lib(
    LIBNAME wifiviz
    SOURCE_FILES model/QNs3.cc
                 model/SniffUtils.cc
                 helper/QNs3-helper.cc
    HEADER_FILES model/QNs3.h
                 model/SniffUtils.h
                 model/wifiviz.h
                 helper/QNs3-helper.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libinternet}
                      ${libmobility}
                      ${libaodv}
                      ${libwifi}
                      nlohmann_json::nlohmann_json
    TEST_SOURCES test/wifiviz-test-suite.cc
                 ${examples_as_tests_sources}
)

if(Boost_INCLUDE_DIRS)
    target_include_directories(${libwifiviz} PRIVATE ${Boost_INCLUDE_DIRS})
endif()

# The Qt GUI front-end is optional. Skip only WiFiVizApp when Qt is missing
# instead of failing configuration for the whole ns-3 tree.
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Core Gui Widgets QUIET)
endif()

if(Qt6_FOUND OR Qt5_FOUND)
    add_subdirectory(ui)
else()
    message(STATUS "wifiviz: Qt (6 or 5.15+) not found; skipping the WiFiVizApp GUI front-end")
endif()
