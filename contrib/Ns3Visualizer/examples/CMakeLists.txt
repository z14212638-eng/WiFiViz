build_lib_example(
    NAME QNs3-example
    SOURCE_FILES QNs3-example.cc
    LIBRARIES_TO_LINK ${libNs3Visualizer}
)
build_lib_example(
    NAME SniffUtils-example
    SOURCE_FILES SniffUtils-example.cc
    LIBRARIES_TO_LINK ${libNs3Visualizer}
)
