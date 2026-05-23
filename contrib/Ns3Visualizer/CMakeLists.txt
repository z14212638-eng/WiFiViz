check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

set(examples_as_tests_sources)
if(${ENABLE_EXAMPLES})
    set(examples_as_tests_sources
        #test/QNs3-examples-test-suite.cc
        )
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) 

build_lib(
    LIBNAME Ns3Visualizer
    SOURCE_FILES model/QNs3.cc
                 model/SniffUtils.cc
                 helper/QNs3-helper.cc
    HEADER_FILES model/QNs3.h
                 model/SniffUtils.h
                 helper/QNs3-helper.h
    LIBRARIES_TO_LINK ${libcore}  
                      ${libinternet}
                      ${libmobility}
                      ${libaodv}
                      ${libwifi}
    TEST_SOURCES test/QNs3-test-suite.cc
                 ${examples_as_tests_sources}
)

# Include the Qt UI frontend application
add_subdirectory(ui)

