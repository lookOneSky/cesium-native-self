add_library(DasCesiumExtra "")

configure_cesium_library(DasCesiumExtra)

cesium_glob_files(DasCesiumExtra_Sources
src/*.cpp
)

cesium_glob_files(DasCesiumExtra_Headers include/*.h)
cesium_glob_files(DasCesiumExtra_PUBLIC_HEADERS
include/DasCesiumExtra/*.h
)

#vs2022编译问题
add_compile_definitions(_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)

set_target_properties(DasCesiumExtra
    PROPERTIES
    PUBLIC_HEADER "${DasCesiumExtra_PUBLIC_HEADERS}"
)

target_sources(
    DasCesiumExtra
    PRIVATE
        ${DasCesiumExtra_Sources}
        ${DasCesiumExtra_Headers}
    PUBLIC
        ${DasCesiumExtra_PUBLIC_HEADERS}
)

target_include_directories(
    DasCesiumExtra
    SYSTEM PUBLIC
        ${CESIUM_NATIVE_RAPIDJSON_INCLUDE_DIR}
        ${CESIUM_NATIVE_DRACO_INCLUDE_DIR}
        ${CESIUM_NATIVE_LIBMORTON_INCUDE_DIR}
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(DasCesiumExtra
    PUBLIC
        spdlog
        Cesium3DTilesSelection
        tinyxml2
        CesiumUtility
        CesiumGltf
)

install(TARGETS DasCesiumExtra
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ./include/DasCesiumExtra
DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
