check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

# The wifiviz library depends on nlohmann/json and Boost headers that are not
# bundled by ns-3. Skip the module at configure time when any required library
# header is missing instead of letting the build fail later at compile time.
find_package(nlohmann_json 3.2.0 QUIET)
find_package(Boost 1.71 QUIET)

set(WIFIVIZ_CMAKE_REQUIRED_INCLUDES_SAVED ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
set(WIFIVIZ_REQUIRED_BOOST_HEADERS
    boost/interprocess/allocators/allocator.hpp
    boost/interprocess/containers/vector.hpp
    boost/interprocess/managed_shared_memory.hpp
    boost/interprocess/shared_memory_object.hpp
    boost/interprocess/sync/interprocess_condition.hpp
    boost/interprocess/sync/interprocess_mutex.hpp
    boost/interprocess/sync/scoped_lock.hpp
    boost/date_time/posix_time/posix_time.hpp
)
set(WIFIVIZ_MISSING_BOOST_HEADERS)
foreach(WIFIVIZ_BOOST_HEADER ${WIFIVIZ_REQUIRED_BOOST_HEADERS})
    string(MAKE_C_IDENTIFIER "WIFIVIZ_HAVE_${WIFIVIZ_BOOST_HEADER}" WIFIVIZ_BOOST_HEADER_VAR)
    check_include_file_cxx("${WIFIVIZ_BOOST_HEADER}" ${WIFIVIZ_BOOST_HEADER_VAR})
    if(NOT ${WIFIVIZ_BOOST_HEADER_VAR})
        list(APPEND WIFIVIZ_MISSING_BOOST_HEADERS "${WIFIVIZ_BOOST_HEADER}")
    endif()
endforeach()
set(CMAKE_REQUIRED_INCLUDES ${WIFIVIZ_CMAKE_REQUIRED_INCLUDES_SAVED})

if(NOT nlohmann_json_FOUND)
    message(STATUS "Skipping contrib module 'wifiviz': nlohmann_json (>= 3.2.0) not found")
    return()
endif()

if(NOT Boost_FOUND)
    message(STATUS "Skipping contrib module 'wifiviz': Boost (>= 1.71) not found")
    return()
endif()

if(WIFIVIZ_MISSING_BOOST_HEADERS)
    string(REPLACE ";" ", " WIFIVIZ_MISSING_BOOST_HEADERS_MSG "${WIFIVIZ_MISSING_BOOST_HEADERS}")
    message(STATUS "Skipping contrib module 'wifiviz': missing Boost headers: ${WIFIVIZ_MISSING_BOOST_HEADERS_MSG}")
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

# The Qt GUI front-end is optional. Skip only WiFiVizApp when Qt or one of its
# platform dependencies is missing instead of failing configuration for the whole
# ns-3 tree.
find_package(OpenGL QUIET)
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 COMPONENTS Core Gui Widgets QUIET)
endif()

if(Qt6_FOUND OR Qt5_FOUND)
    add_subdirectory(ui)
else()
    set(WIFIVIZ_QT_SKIP_REASON "Qt 6 or Qt 5.15+ Core/Gui/Widgets not found")
    if(NOT OPENGL_FOUND)
        set(WIFIVIZ_QT_SKIP_REASON "${WIFIVIZ_QT_SKIP_REASON}; OpenGL/GLX development libraries not found")
    endif()
    if(Qt6_NOT_FOUND_MESSAGE)
        set(WIFIVIZ_QT_SKIP_REASON "${WIFIVIZ_QT_SKIP_REASON}; Qt6: ${Qt6_NOT_FOUND_MESSAGE}")
    endif()
    if(Qt5_NOT_FOUND_MESSAGE)
        set(WIFIVIZ_QT_SKIP_REASON "${WIFIVIZ_QT_SKIP_REASON}; Qt5: ${Qt5_NOT_FOUND_MESSAGE}")
    endif()
    message(STATUS "wifiviz: ${WIFIVIZ_QT_SKIP_REASON}; skipping the WiFiVizApp GUI front-end")
endif()
