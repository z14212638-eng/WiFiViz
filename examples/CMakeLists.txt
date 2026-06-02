build_lib_example(
    NAME wifiviz-basic-example
    SOURCE_FILES wifiviz-basic-example.cc
    LIBRARIES_TO_LINK ${libwifiviz}
)

build_lib_example(
    NAME wifiviz-overhead-benchmark
    SOURCE_FILES wifiviz-overhead-benchmark.cc
    LIBRARIES_TO_LINK ${libwifiviz}
)

build_lib_example(
    NAME wifiviz-visualizer
    SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../tools/visualizer.cc
    LIBRARIES_TO_LINK ${libwifiviz} ${libcore}
)
