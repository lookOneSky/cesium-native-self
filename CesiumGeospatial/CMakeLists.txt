add_library(CesiumGeospatial "")

configure_cesium_library(CesiumGeospatial)

cesium_glob_files(CESIUM_GEOSPATIAL_SOURCES src/*.cpp)
cesium_glob_files(CESIUM_GEOSPATIAL_HEADERS src/*.h)
cesium_glob_files(CESIUM_GEOSPATIAL_PUBLIC_HEADERS include/CesiumGeospatial/*.h)

# Das: Add Das folder sources and headers
cesium_glob_files(CESIUM_GEOSPATIAL_DAS_SOURCES src/Das/*.cpp)
cesium_glob_files(CESIUM_GEOSPATIAL_DAS_HEADERS src/Das/*.h)
cesium_glob_files(CESIUM_GEOSPATIAL_DAS_PUBLIC_HEADERS include/CesiumGeospatial/Das/*.h)
cesium_glob_files(CESIUM_GEOSPATIAL_TEST_SOURCES test/*.cpp)
cesium_glob_files(CESIUM_GEOSPATIAL_TEST_HEADERS test/*.h)

set_target_properties(CesiumGeospatial
    PROPERTIES
        TEST_SOURCES "${CESIUM_GEOSPATIAL_TEST_SOURCES}"
        TEST_HEADERS "${CESIUM_GEOSPATIAL_TEST_HEADERS}"
        TEST_DATA_DIR ${CMAKE_CURRENT_LIST_DIR}/test/data
)

set_target_properties(CesiumGeospatial
    PROPERTIES
    # Das: Add Das public headers
    PUBLIC_HEADER "${CESIUM_GEOSPATIAL_PUBLIC_HEADERS};${CESIUM_GEOSPATIAL_DAS_PUBLIC_HEADERS}" 
)

target_sources(
    CesiumGeospatial
    PRIVATE
        ${CESIUM_GEOSPATIAL_SOURCES}
        ${CESIUM_GEOSPATIAL_HEADERS}
        ${CESIUM_GEOSPATIAL_DAS_SOURCES} # Das: Add Das sources
        ${CESIUM_GEOSPATIAL_DAS_HEADERS} # Das: Add Das headers
    PUBLIC
        ${CESIUM_GEOSPATIAL_PUBLIC_HEADERS}
        ${CESIUM_GEOSPATIAL_DAS_PUBLIC_HEADERS} # Das: Add Das public headers
)

cesium_target_include_directories(
    TARGET
        CesiumGeospatial
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(CesiumGeospatial
PUBLIC
    CesiumGeometry
    CesiumUtility
    earcut
PRIVATE
    s2::s2
)

if(CESIUM_DISABLE_DEFAULT_ELLIPSOID)
  target_compile_definitions(CesiumGeospatial PUBLIC CESIUM_DISABLE_DEFAULT_ELLIPSOID=1)
endif()
