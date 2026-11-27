add_library(DasCesium "")
configure_cesium_library(DasCesium)

cesium_glob_files(DAS_CESIUM_SOURCES src/*.cpp)
cesium_glob_files(DAS_CESIUM_PUBLIC_HEADERS include/DasCesium/*.h)

target_sources(DasCesium PRIVATE ${DAS_CESIUM_SOURCES} ${DAS_CESIUM_PUBLIC_HEADERS})

cesium_target_include_directories(
    TARGET DasCesium
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(DasCesium PUBLIC Cesium3DTilesSelection)
