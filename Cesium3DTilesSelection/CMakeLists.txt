add_library(Cesium3DTilesSelection "")

configure_cesium_library(Cesium3DTilesSelection)

cesium_glob_files(CESIUM_3DTILES_SELECTION_SOURCES src/*.cpp)
cesium_glob_files(CESIUM_3DTILES_SELECTION_HEADERS src/*.h)
cesium_glob_files(CESIUM_3DTILES_SELECTION_PUBLIC_HEADERS include/Cesium3DTilesSelection/*.h)
cesium_glob_files(CESIUM_3DTILES_SELECTION_TEST_SOURCES test/*.cpp)
cesium_glob_files(CESIUM_3DTILES_SELECTION_TEST_HEADERS test/*.h)

set_target_properties(Cesium3DTilesSelection
    PROPERTIES
        TEST_SOURCES "${CESIUM_3DTILES_SELECTION_TEST_SOURCES}"
        TEST_HEADERS "${CESIUM_3DTILES_SELECTION_TEST_HEADERS}"
        TEST_DATA_DIR ${CMAKE_CURRENT_LIST_DIR}/test/data
)

set_target_properties(Cesium3DTilesSelection
    PROPERTIES
    #add Das 将src目录的头文件添加到PUBLIC_HEADER，使其在安装时被导出
    PUBLIC_HEADER "${CESIUM_3DTILES_SELECTION_PUBLIC_HEADERS};${CESIUM_3DTILES_SELECTION_HEADERS}"
)

target_sources(
    Cesium3DTilesSelection
    PRIVATE
        ${CESIUM_3DTILES_SELECTION_SOURCES}
        ${CESIUM_3DTILES_SELECTION_HEADERS}
    PUBLIC
        ${CESIUM_3DTILES_SELECTION_PUBLIC_HEADERS}
)

cesium_target_include_directories(
    TARGET 
        Cesium3DTilesSelection
    PUBLIC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
        #add Das 将src目录添加到PUBLIC包含路径，允许外部模块访问src目录的头文件
        ${CMAKE_CURRENT_LIST_DIR}/src
    PRIVATE
)

target_link_libraries(Cesium3DTilesSelection
    PUBLIC
        Cesium3DTiles
        Cesium3DTilesContent
        Cesium3DTilesReader
        CesiumAsync
        CesiumGeospatial
        CesiumGeometry
        CesiumGltf
        CesiumGltfReader
        CesiumQuantizedMeshTerrain
        CesiumRasterOverlays
        CesiumUtility
        spdlog::spdlog spdlog::spdlog_header_only
# PRIVATE
        libmorton::libmorton
        draco::draco
        nonstd::expected-lite
PRIVATE
        tinyxml2::tinyxml2
)
