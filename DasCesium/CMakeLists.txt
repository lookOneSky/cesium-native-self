add_library(DasCesium "")

configure_cesium_library(DasCesium)

cesium_glob_files(DasCesium_Sources
src/*.cpp
)

cesium_glob_files(DasCesium_Headers include/*.h)
cesium_glob_files(DasCesium_PUBLIC_HEADERS
include/DasCesium/*.h
)

#vs2022编译问题
add_compile_definitions(_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)

set_target_properties(DasCesium
    PROPERTIES
    PUBLIC_HEADER "${DasCesium_PUBLIC_HEADERS}"
)

target_sources(
    DasCesium
    PRIVATE
        ${DasCesium_Sources}
        ${DasCesium_Headers}
    PUBLIC
        ${DasCesium_PUBLIC_HEADERS}
)

target_include_directories(
    DasCesium
    SYSTEM PUBLIC
        ${CESIUM_NATIVE_RAPIDJSON_INCLUDE_DIR}
        ${CESIUM_NATIVE_DRACO_INCLUDE_DIR}
        ${CESIUM_NATIVE_LIBMORTON_INCUDE_DIR}
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(DasCesium
    PUBLIC
        spdlog
        Cesium3DTilesSelection
        tinyxml2
        CesiumUtility
        CesiumGltf
)

install(TARGETS DasCesium
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ./include/DasCesium
DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
