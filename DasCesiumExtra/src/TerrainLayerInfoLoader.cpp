#include "EVScene/TerrainLayerInfoLoader.h"

#include "EVScene/HeightMapMeshLoader.h"
#include "EVUtility/strings_conversion.h"
// #include "WebServerConfigLoader/WebMetadata.h"
#include "CesiumGeospatial/calcQuadtreeMaxGeometricError.h"

#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>

#include <libmorton/morton.h>
// #include <CesiumAsync/ITickTaskDispatcher.h>
#include "EVScene/TilesetUtils.h"

#include <CesiumGeometry/QuadtreeRectangleAvailability.h>
#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumGltf/Buffer.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/RasterOverlayUtilities.h>
using namespace Cesium3DTilesSelection;
using namespace CesiumAsync;
using namespace CesiumGeometry;
using namespace CesiumGeospatial;

namespace EVScene {
static const std::string EV_REQUEST =
    "EV_WebService/get/globeservice/10000/GetDEMData/";

TerrainLayerInfoLoader::TerrainLayerInfoLoader(
    const CesiumGeometry::QuadtreeTilingScheme& tilingScheme,
    const CesiumGeospatial::Projection& projection,
    std::vector<Cesium3DTilesSelection::LayerJsonTerrainLoader::Layer>&& layers,
    std::vector<TerrainLayerOptions>& options,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions)
    : LayerJsonTerrainLoader(tilingScheme, projection, std::move(layers)),
      _LayerOptions(options) /*,
       bSampleMostDetailed(tilesetOptions.bSampleTerrain),
       _editPolygons(tilesetOptions.contentOptions.editPolygons)*/
{}

/**
 * @brief Creates a default {@link BoundingRegionWithLooseFittingHeights} for
 * the given rectangle.
 *
 * The heights of this bounding volume will have unspecified default values
 * that are suitable for the use on earth.
 *
 * @param globeRectangle The {@link CesiumGeospatial::GlobeRectangle}
 * @return The {@link BoundingRegionWithLooseFittingHeights}
 */
BoundingVolume createDefaultLooseEarthBoundingVolume(
    const CesiumGeospatial::GlobeRectangle& globeRectangle) {
  return BoundingRegionWithLooseFittingHeights(
      BoundingRegion(globeRectangle, -1000.0, 9000.0));
}

void subtreeHash(
    const CesiumGeometry::QuadtreeTileID& subtreeID,
    const LayerJsonTerrainLoader::Layer& layer,
    uint32_t& subtreeLevelIdx,
    uint64_t& subtreeMortonIdx) {
  subtreeLevelIdx = subtreeID.level / uint32_t(layer.availabilityLevels);
  subtreeMortonIdx = libmorton::morton2D_64_encode(subtreeID.x, subtreeID.y);
}
bool parseDemHeadInfo(const std::string& path, DemlayerHeadinfo& headinfo) {
  std::string datasetname;
  std::string datasetfolder;
  std::string outExt;
  FILE* fp = NULL;
  long filesize;
  char* data = NULL;
  fp = fopen(path.c_str(), "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = (char*)new char[filesize + 1];
    memset(data, 0, filesize + 1);
    size_t size = fread(data, 1, filesize, fp);
    fclose(fp);
    fp = NULL;
  }
  if (!data)
    return false;
  char* temp = data;

  if (!temp)
    return false;

  char chartmp20[21];
  // QString qs;
  short m_Version;
  int m_ByteOrder;
  int m_HeadFileSize;
  // 20
  int m_StorageType;
  int m_DataType;
  int m_DataStorageFormat;
  std::string m_FirstFile;
  std::string m_SecondFile;
  std::string m_Prefix;
  std::string m_Postfix;
  signed char m_CoorectPath;
  // 52_93
  int m_MinLevel;
  int m_MaxLevel;
  double m_Span;
  int m_Prejection;
  double m_StartX;
  double m_StartY;
  int m_OrigPostion;
  int m_TileWidth;
  int m_TileHeigth;
  // 48
  int m_LayerCount;
  // LAYERS m_Layers;

  std::string m_LayerName;
  // 50
  double m_Xmin;
  double m_Xmax;
  double m_Ymin;
  double m_Ymax;
  // 32
  int m_DataFormat;
  double m_Precision;
  std::string m_LayerTime;
  int m_FieldCount;
  // FIELDS m_Fields;
  int m_LevelMin;
  int m_LevelMax;
  int m_FieldLevelMin;
  int m_FieldLevelMax;
  std::string m_SplitTime;
  std::string m_AdditionInfo;

  memcpy(&m_Version, temp, 2);
  temp += 2;
  memcpy(&m_ByteOrder, temp, 4);
  temp += 14;
  memcpy(&m_HeadFileSize, temp, 4);
  temp += 4;
  memcpy(&m_StorageType, temp, 4);
  temp += 4;
  memcpy(&m_DataType, temp, 4);
  temp += 4;
  memcpy(&m_DataStorageFormat, temp, 4);
  temp += 4;
  memset(chartmp20, 0, 21);
  memcpy(chartmp20, temp, 20);
  // std::string qs = std::string(chartmp20);
  std::string qs = std::string(chartmp20);
  m_FirstFile.assign(qs.c_str());
  temp += 20;
  memset(chartmp20, 0, 21);
  memcpy(chartmp20, temp, 20);
  // qs = std::string(chartmp20);
  qs = std::string(chartmp20);
  m_SecondFile.assign(qs.c_str());
  temp += 20;
  if (m_Version == 1010) {
    memset(chartmp20, 0, 21);
    memcpy(chartmp20, temp, 20);
    // qs = std::string(chartmp20);
    qs = std::string(chartmp20);
    m_Prefix.assign(qs.c_str());
    temp += 20;
    memset(chartmp20, 0, 21);
    memcpy(chartmp20, temp, 20);
    // qs = std::string(chartmp20);
    qs = std::string(chartmp20);
    m_Postfix.assign(qs.c_str());
    temp += 20;
    memcpy(&m_CoorectPath, temp, 1);
    temp += 1;
  }
  memcpy(&m_MinLevel, temp, 4);
  temp += 4;
  memcpy(&m_MaxLevel, temp, 4);
  temp += 4;
  memcpy(&m_Span, temp, 8);
  temp += 8;
  memcpy(&m_Prejection, temp, 4);
  temp += 4;
  memcpy(&m_StartX, temp, 8);
  temp += 8;
  memcpy(&m_StartY, temp, 8);
  temp += 8;
  memcpy(&m_OrigPostion, temp, 4);
  temp += 4;
  memcpy(&m_TileWidth, temp, 4);
  temp += 4;
  memcpy(&m_TileHeigth, temp, 4);
  temp += 4;
  memcpy(&m_LayerCount, temp, 4);
  temp += 4;
  char chartmp50[51];
  // int index;
  memset(chartmp50, 0, 51);
  memcpy(chartmp50, temp, 50);
  m_LayerName = chartmp50;
  temp += 50;
  int32_t layerOffset;
  memcpy(&layerOffset, temp, 4);
  temp += 4;

  memcpy(&m_Xmin, temp, 8);
  temp += 8;
  memcpy(&m_Xmax, temp, 8);
  temp += 8;
  memcpy(&m_Ymin, temp, 8);
  temp += 8;
  memcpy(&m_Ymax, temp, 8);
  temp += 8;
  memcpy(&m_DataFormat, temp, 4);
  temp += 4;
  memcpy(&m_Precision, temp, 8);
  temp += 8;

  char chartmp10[11];

  memset(chartmp10, 0, 11);
  memcpy(chartmp10, temp, 10);
  qs = chartmp10;
  m_LayerTime.assign(qs.c_str());
  temp += 10;
  memcpy(&m_FieldCount, temp, 4);
  temp += 4;

  // char chartmp50[51];
  for (int i = 0; i < m_FieldCount; ++i) {
    // CLocalTileLayerFieldMeta field;
    /*memset(chartmp50,0,51);
    memcpy(chartmp50,temp,50);
    qs=QString::fromUtf8(chartmp50);
    field.m_Name.assign(qs.toLocal8Bit().constData());*/
    temp += 50;
    // memcpy(&(field.m_Type),temp,4);
    temp += 4;
    // memcpy(&(field.m_Size),temp,4);
    temp += 4;
    // m_Fields.push_back(field);
  }
  memcpy(&m_LevelMin, temp, 4);
  temp += 4;
  memcpy(&m_LevelMax, temp, 4);
  temp += 4;
  memcpy(&m_FieldLevelMin, temp, 4);
  temp += 4;
  memcpy(&m_FieldLevelMax, temp, 4);
  temp += 4;

  memset(chartmp10, 0, 11);
  memcpy(chartmp10, temp, 10);
  m_SplitTime = chartmp10;

  temp += 10;

  char chartmp160[161];
  memset(chartmp160, 0, 161);
  memcpy(chartmp160, temp, 160);
  m_AdditionInfo = chartmp160;

  // headinfo.mnFileType = (EVSSCCacheDataType)m_DataType;
  headinfo.mstrLayerName = datasetname;
  headinfo.mstrDataTime = m_SplitTime;
  headinfo.mstrUpdateTime = m_SplitTime;
  headinfo.mnDataType = (EVSSCLayerDataType)m_DataFormat;
  // headinfo.mnFileFormat = (EVSSCFileFormat)m_DataStorageFormat;
  headinfo.mnLevelNum = m_LevelMax - m_LevelMin + 1;
  headinfo.mnSamplesPerTile = m_TileWidth;
  headinfo.mdfSpanForZeroLevel = m_Span;
  // if (headinfo.mpEnvelope == NULL) {
  //  info.mpEnvelope = new CEnvelope();
  //}

  // double west = m_Xmin;
  // double east = m_Xmax;
  // double south = m_Ymin;
  // double north = m_Ymax;
  // CesiumGeospatial::GlobeRectangle rectangle(west, south, east, north);

  headinfo.rectanglewest = m_Xmin;
  headinfo.rectangleeast = m_Xmax;
  headinfo.rectanglesouth = atan((exp(m_Ymin) - exp(-m_Ymin)) / 2);
  headinfo.rectanglenorth = atan((exp(m_Ymax) - exp(-m_Ymax)) / 2);
  headinfo.region = CesiumGeospatial::GlobeRectangle(
      m_Xmin,
      atan((exp(m_Ymin) - exp(-m_Ymin)) / 2),
      m_Xmax,
      atan((exp(m_Ymax) - exp(-m_Ymax)) / 2));
  // double rectanglewest;  //
  // double rectangleeast;  //
  // double rectanglesouth; //
  // double rectanglenorth; //
  // if (headinfo.mBundingRegion) {
  //  delete headinfo.mBundingRegion;
  //  headinfo.mBundingRegion = nullptr;
  //}
  // headinfo.mBundingRegion = new CesiumGeospatial::BoundingRegion(
  //    rectangle,
  //    0.0,
  //    0.0,
  //    CesiumGeospatial::Ellipsoid::CGCS2000);

  // if (m_Prejection == 4326) {
  //  headinfo.mnSRS = EVSSCSRS::SSC_SRS_WGS_1984;
  //} else if (m_Prejection == 54004) {
  //  headinfo.mnSRS = EVSSCSRS::SSC_SRS_Mercator;
  //}
  // if (headinfo.mpSR == NULL) {
  //  if (m_Prejection == 4326) {
  //    headinfo.mpSR =
  //        (ISpatialReference*)CCoordinateSystemFactory::createCoordSys(4326);
  //  } else if (m_Prejection == 54004) {
  //    headinfo.mpSR =
  //        (ISpatialReference*)CCoordinateSystemFactory::createCoordSys(3395);
  //  }
  //}
  headinfo.mnMaxDataLevel = m_LevelMax;
  headinfo.mnMinDataLevel = m_LevelMin;
  headinfo.mstrName = datasetname;
  // headinfo.mnCacheMode = (EVSSCCacheMode)m_OrigPostion;

  // headinfo.mDatasetType = DT_LOCAL_DEM_DATASET;

  return true;
}

bool isSubtreeLoadedInLayer(
    const CesiumGeometry::QuadtreeTileID& subtreeID,
    const LayerJsonTerrainLoader::Layer& layer) {
  assert(
      layer.availabilityLevels > 0 &&
      "Layer needs to support availabilityLevels");

  uint32_t subtreeLevelIdx;
  uint64_t subtreeMortonIdx;
  subtreeHash(subtreeID, layer, subtreeLevelIdx, subtreeMortonIdx);

  // it doesn't have the subtree exceeds max zooms, so just treat it
  // as loaded
  if (subtreeLevelIdx >= layer.loadedSubtrees.size()) {
    return true;
  }

  return layer.loadedSubtrees[subtreeLevelIdx].find(subtreeMortonIdx) !=
         layer.loadedSubtrees[subtreeLevelIdx].end();
}

void addRectangleAvailabilityToLayer(
    LayerJsonTerrainLoader::Layer& layer,
    const QuadtreeTileID& subtreeID,
    const std::vector<CesiumGeometry::QuadtreeTileRectangularRange>&
        rectangleAvailabilities) {
  for (const QuadtreeTileRectangularRange& range : rectangleAvailabilities) {
    layer.contentAvailability.addAvailableTileRange(range);
  }

  uint32_t subtreeLevelIdx;
  uint64_t subtreeMortonIdx;
  subtreeHash(subtreeID, layer, subtreeLevelIdx, subtreeMortonIdx);

  // it doesn't have the subtree exceeds max zooms, so just treat it
  // as loaded
  if (subtreeLevelIdx >= layer.loadedSubtrees.size()) {
    return;
  }

  layer.loadedSubtrees[subtreeLevelIdx].insert(subtreeMortonIdx);
}

void generateRasterOverlayUVs(
    const BoundingVolume& tileBoundingVolume,
    const glm::dmat4& tileTransform,
    const Projection& projection,
    TileLoadResult& result) {
  if (result.state != TileLoadResultState::Success) {
    return;
  }

  const BoundingRegion* pParentRegion = nullptr;
  if (result.updatedBoundingVolume) {
    pParentRegion =
        std::get_if<BoundingRegion>(&result.updatedBoundingVolume.value());
  } else {
    pParentRegion = std::get_if<BoundingRegion>(&tileBoundingVolume);
  }

  CesiumGltf::Model* pModel =
      std::get_if<CesiumGltf::Model>(&result.contentKind);
  if (pModel) {
    result.rasterOverlayDetails = CesiumRasterOverlays::RasterOverlayUtilities::
        createRasterOverlayTextureCoordinates(
            *pModel,
            tileTransform,
            pParentRegion ? std::make_optional<GlobeRectangle>(
                                pParentRegion->getRectangle())
                          : std::nullopt,
            {projection});
  }
}

std::string resolveTileUrl(
    const QuadtreeTileID& tileID,
    const LayerJsonTerrainLoader::Layer& layer) {
  if (layer.tileTemplateUrls.empty()) {
    return std::string();
  }

  return CesiumUtility::Uri::resolve(
      layer.baseUrl,
      CesiumUtility::Uri::substituteTemplateParameters(
          layer.tileTemplateUrls[0],
          [&tileID, &layer](const std::string& placeholder) -> std::string {
            if (placeholder == "level" || placeholder == "z") {
              return std::to_string(tileID.level);
            }
            if (placeholder == "x") {
              return std::to_string(tileID.x);
            }
            if (placeholder == "y") {
              return std::to_string(tileID.y);
            }
            if (placeholder == "version") {
              return layer.version;
            }

            return placeholder;
          }));
}

std::string resolveInvertedYTileUrl(
    const QuadtreeTileID& tileID,
    const LayerJsonTerrainLoader::Layer& layer,
    const QuadtreeTilingScheme& tilingScheme) {
  if (layer.tileTemplateUrls.empty()) {
    return std::string();
  }

  return EVUtility::resolveForEVTile(
      layer.baseUrl,
      CesiumUtility::Uri::substituteTemplateParameters(
          layer.tileTemplateUrls[0],
          [&tileID, &layer, &tilingScheme](
              const std::string& placeholder) -> std::string {
            if (placeholder == "level" || placeholder == "z") {
              return std::to_string(tileID.level);
            }
            if (placeholder == "x") {
              return std::to_string(tileID.x);
            }
            if (placeholder == "y") {
              uint32_t invertedY = tileID.computeInvertedY(tilingScheme);
              // return std::to_string(tileID.y);
              return std::to_string(invertedY);
            }
            if (placeholder == "EVlevel") {
              return std::to_string(tileID.level + 1);
            }
            if (placeholder == "version") {
              return layer.version;
            }

            return placeholder;
          }));
}

static uint32_t index2DTo1D(uint32_t x, uint32_t y, uint32_t width) {
  return y * width + x;
}

void convertDataToHeight(
    const std::span<const std::byte>& data,
    std::vector<float>& pbufferdata,
    const EVScene::DemlayerHeadinfo& headinfo,
    const float& exaggerationScale,
    float raiseHeight) {
  uint32_t heightBufferSize;
  size_t dataWidth, offsetX, width;

  if (headinfo.mnSamplesPerTile == 133) {
    width = headinfo.mnSamplesPerTile - 4;
    heightBufferSize = headinfo.mnSamplesPerTile * headinfo.mnSamplesPerTile;
  } else {
    width = 65;
    heightBufferSize = 65 * 65;
  }
  if (data.size() < 1) {
    return;
  }
  switch (headinfo.mnDataType) {
  case SSC_LayerDataType_DEM_int16:
    dataWidth = static_cast<size_t>(sqrt((data.size() / 2)));
    offsetX = (dataWidth - width) / 2;
    for (size_t j = 0; j < width; j++) {
      for (size_t i = 0; i < width; i++) {
        // size_t heightId = index2DTo1D(i, j, meshView.width);
        size_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const int16_t*>(&data[bufferId * 2]);

        float valuef =
            value > 9000
                ? 0.0
                : value *
                      exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;
        pbufferdata.emplace_back(valuef);
      }
    }
    break;
  case SSC_LayerDataType_DEM_Float:
    dataWidth = static_cast<uint32_t>(sqrt((data.size() / 4)));
    offsetX = (dataWidth - width) / 2;
    for (uint32_t j = 0; j < width; j++) {
      for (uint32_t i = 0; i < width; i++) {
        // uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const float*>(&data[bufferId * 4]);
        float valuef =
            value > 9000
                ? 0.0
                : value *
                      exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;
        pbufferdata.emplace_back(valuef);
      }
    }
    break;
  case SSC_LayerDataType_DEM_int:
    dataWidth = static_cast<uint32_t>(sqrt((data.size() / 4)));
    offsetX = (dataWidth - width) / 2;
    for (uint32_t j = 0; j < width; j++) {
      for (uint32_t i = 0; i < width; i++) {
        // uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const int*>(&data[bufferId * 4]);
        float valuef =
            value > 9000
                ? 0.0
                : value *
                      exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;
        pbufferdata.emplace_back(valuef);
      }
    }
    break;
  case SSC_LayerDataType_DEM_int8:
    dataWidth = static_cast<uint32_t>(sqrt((data.size())));
    offsetX = (dataWidth - width) / 2;
    for (uint32_t j = 0; j < width; j++) {
      for (uint32_t i = 0; i < width; i++) {
        // uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const int8_t*>(&data[bufferId]);
        float valuef =
            value > 9000
                ? 0.0
                : value *
                      exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;
        pbufferdata.emplace_back(valuef);
      }
    }
    break;
  default:
    break;
  }
}

void convertDataToHeightBUffer(
    const std::span<const std::byte>& data,
    std::vector<std::byte>& pbufferdata,
    bool enableWaterMask,
    const EVScene::DemlayerHeadinfo& headinfo,
    const float& exaggerationScale) {
  uint32_t heightBufferSize;
  size_t dataWidth, offsetX, width;

  if (headinfo.mnSamplesPerTile == 133) {
    width = headinfo.mnSamplesPerTile - 4;
    heightBufferSize = headinfo.mnSamplesPerTile * headinfo.mnSamplesPerTile;
  } else {
    width = 65;
    heightBufferSize = 65 * 65;
  }
  switch (headinfo.mnDataType) {
  case SSC_LayerDataType_DEM_int16:
    for (size_t i = 0; i < heightBufferSize; i++) {
      int16_t value = *reinterpret_cast<const int16_t*>(&data[i * 2]);
      float valuef =
          value > 9000
              ? 0.0
              : value *
                    exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.push_back(*pByte);
      pbufferdata.push_back(*(pByte + 1));
      pbufferdata.push_back(*(pByte + 2));
      pbufferdata.push_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_Float:
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const float*>(&data[i * 4]);
      float valuef =
          value > 9000
              ? 0.0
              : value *
                    exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.push_back(*pByte);
      pbufferdata.push_back(*(pByte + 1));
      pbufferdata.push_back(*(pByte + 2));
      pbufferdata.push_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_int:
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const int*>(&data[i * 4]);
      float valuef =
          value > 9000
              ? 0.0
              : value *
                    exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.push_back(*pByte);
      pbufferdata.push_back(*(pByte + 1));
      pbufferdata.push_back(*(pByte + 2));
      pbufferdata.push_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_int8:
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const int8_t*>(&data[i]);
      float valuef =
          value > 9000
              ? 0.0
              : value *
                    exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.push_back(*pByte);
      pbufferdata.push_back(*(pByte + 1));
      pbufferdata.push_back(*(pByte + 2));
      pbufferdata.push_back(*(pByte + 3));
    }
    break;
  default:
    break;
  }
}

Future<HeightMapMeshLoadResult> requestTileContent(
    const std::shared_ptr<spdlog::logger>& pLogger,
    const AsyncSystem& asyncSystem,
    const std::shared_ptr<IAssetAccessor>& pAssetAccessor,
    const QuadtreeTileID& tileID,
    const BoundingVolume& boundingVolume,
    const LayerJsonTerrainLoader::Layer& layer,
    const std::vector<IAssetAccessor::THeader>& requestHeaders,
    const QuadtreeTilingScheme& tilingScheme,
    const Tile* pParent,
    const Cesium3DTilesSelection::TilesetContentOptions& contentOptions,
    const TerrainLayerOptions& layerOptions) {
  // std::string url = resolveTileUrl(tileID, layer);
  if (layer.baseUrl.empty()) {
    return asyncSystem.createResolvedFuture().thenInWorkerThread(
        [asyncSystem,
         pLogger,
         tileID,
         boundingVolume,
         contentOptions,
         layerOptions]() {
          return HeightMapMeshLoader::load(
              tileID,
              boundingVolume,
              "",
              std::move(std::vector<std::byte>()),
              contentOptions,
              layerOptions);
        });
  }

  QuadtreeTileID requestTileId = tileID;
  bool useParentData = false;
  std::vector<QuadtreeTileID> tileIds;
  // 是否需要从父级请求数据
  while (requestTileId.level > layerOptions.headInfo.mnMaxDataLevel - 1) {
    tileIds.emplace_back(requestTileId);
    requestTileId = requestTileId.getParent();
    useParentData = true;
  }

  std::string url = resolveInvertedYTileUrl(requestTileId, layer, tilingScheme);

  return pAssetAccessor
      ->get(
          asyncSystem,
          {url/*,
           "Server7_" + layerOptions.headInfo.mstrName,
           requestTileId.level + 1,
           requestTileId.computeInvertedY(tilingScheme),
           requestTileId.x,
           true,
           false,
           false*/},
          requestHeaders)
      .thenInWorkerThread(
          [asyncSystem,
           pLogger,
           tileID,
           boundingVolume,
           pParent,
           tilingScheme,
           useParentData,
           requestTileId,
           tileIds,
           contentOptions,
           layerOptions](
        std::shared_ptr<IAssetRequest>&& pRequest) mutable{
      
          const IAssetResponse* pResponse = pRequest->response();
          if (!pResponse) {
            HeightMapMeshLoadResult result;
            result.errors.emplaceError(fmt::format(
              "Did not receive a valid response for tile content {}",
              pRequest->url()));
            result.pRequest = std::move(pRequest);
            return result;
          }

          if (pResponse->statusCode() != 0 &&
            (pResponse->statusCode() < 200 ||
              pResponse->statusCode() >= 300)) {
            HeightMapMeshLoadResult result;
            result.errors.emplaceError(fmt::format(
              "Receive status code {} for tile content {}",
              pResponse->statusCode(),
              pRequest->url()));
            result.pRequest = std::move(pRequest);
            return result;
          }

          if (useParentData&&pResponse->data().size() != 0) {
            //请求的父级别高程，采样到对应的级别。
            std::vector<std::byte> vectorData;
            convertDataToHeightBUffer(
                pResponse->data(),
                vectorData,
                contentOptions.enableWaterMask,
                layerOptions.headInfo,
                layerOptions.exaggerationScale);
            std::shared_ptr<std::vector<std::byte>> tempdata =
                std::make_shared<std::vector<std::byte>>(vectorData);
            while (!tileIds.empty()) {
              tempdata = HeightMapMeshLoader::UpsamplingFromParentData(
                  tempdata.get(),
                  tileIds.back(),
                  tilingScheme);
              tileIds.pop_back();
            }
            const std::span<std::byte> spandata{*tempdata};
            TerrainLayerOptions newlayerOptions = layerOptions;
            DemlayerHeadinfo& newHeadinfo = newlayerOptions.headInfo;
            newHeadinfo.mnDataType = SSC_LayerDataType_DEM_Float;
            return HeightMapMeshLoader::load(
                tileID,
                boundingVolume,
                pRequest->url(),
                spandata,
                contentOptions,
                newlayerOptions);
          }
          if (pResponse->data().size() == 0 ) {
            HeightMapMeshLoadResult result;
            if (!pParent) {
              result.errors.emplaceError(
                  "Upsample error : parent RenderContent empty!");
              result.pRequest = std::move(pRequest);
              return result;
            }
            
            if (!pParent->isRenderable()) {
              result.errors.emplaceError(
                  "Upsample error : parent is not ready!");
              result.pRequest = std::move(pRequest);
              return result;
            }
            const TileContent& parentContent = pParent->getContent();

            const TileRenderContent* pParentRenderContent =
                parentContent.getRenderContent();
            if (!pParentRenderContent) {
              // parent doesn't have mesh, so it's not possible to upsample
              result.errors.emplaceError(
                  "Upsample error : parent RenderContent empty!");
              result.pRequest = std::move(pRequest);
              return result;
            }
            const CesiumGltf::Model& parentModel =
                pParentRenderContent->getModel();
            const auto& attributes =
                parentModel.meshes[0].primitives[0].attributes;
            if (attributes.find("HEIGHTS") != attributes.end()) {
              size_t HeightsBufferId = attributes.find("HEIGHTS")->second;
               CesiumGltf::Buffer HeightsBuffer =
                  parentModel.buffers[HeightsBufferId];
              if (HeightsBuffer.cesium.data.size() == 0) {
                result.errors.emplaceError(
                    "Upsample error : parent heightdata empty!");
                result.pRequest = std::move(pRequest);
                return result;
              }
              return HeightMapMeshLoader::upSampleFromParent(
                  tileID,
                  boundingVolume,
                  pRequest->url(),
                  &HeightsBuffer.cesium.data,
                  tilingScheme,
                  contentOptions,
                  layerOptions);
            }
            
          }
          return HeightMapMeshLoader::load(
            tileID,
            boundingVolume,
            pRequest->url(),
            pResponse->data(),
                contentOptions,
                layerOptions);
      });
}

// Future<int> loadTileAvailability(
//   const std::shared_ptr<spdlog::logger>& pLogger,
//   const AsyncSystem& asyncSystem,
//   const std::shared_ptr<IAssetAccessor>& pAssetAccessor,
//   const QuadtreeTileID& tileID,
//   LayerJsonTerrainLoader::Layer& layer,
//   const std::vector<IAssetAccessor::THeader>& requestHeaders) {
//   std::string url = resolveTileUrl(tileID, layer);
//   return pAssetAccessor->get(asyncSystem, url, requestHeaders)
//     .thenInWorkerThread([pLogger,
//       tileID](std::shared_ptr<IAssetRequest>&& pRequest) {
//         const IAssetResponse* pResponse = pRequest->response();
//         if (pResponse) {
//           uint16_t statusCode = pResponse->statusCode();
//
//           if (!(statusCode != 0 && (statusCode < 200 || statusCode >= 300)))
//           {
//             return HeightMapMeshLoader::loadMetadata(pResponse->data(),
//             tileID);
//           }
//         }
//
//         SPDLOG_LOGGER_ERROR(
//           pLogger,
//          "Failed to load availability data from {}",
//          pRequest->url());
//        return HeightMapMeshMetadataResult();
//      })
//    .thenInMainThread([&layer,
//      tileID](HeightMapMeshMetadataResult&& metadata) {
//        addRectangleAvailabilityToLayer(layer, tileID, metadata.availability);
//        return 0;
//      });
//}

TileLoadResult convertToTileLoadResult(
    std::shared_ptr<CesiumAsync::IAssetAccessor> pAssetAccessor,
    HeightMapMeshLoadResult&& loadResult) {
  if (loadResult.errors || !loadResult.model) {
    return TileLoadResult::createFailedResult(
        pAssetAccessor,
        loadResult.pRequest);
  }

  return TileLoadResult{
      std::move(*loadResult.model),
      CesiumGeometry::Axis::Y,
      loadResult.updatedBoundingVolume,
      std::nullopt,
      std::nullopt,
      pAssetAccessor,
      nullptr,
      {},
      TileLoadResultState::Success,
      CesiumGeospatial::Ellipsoid::UNIT_SPHERE,
      std::nullopt};
}

Future<TilesetContentLoaderResult<TerrainLayerInfoLoader>> constructLoader(
    const AsyncSystem& asyncSystem,
    CesiumUtility::ErrorList& errors,
    std::vector<DemlayerHeadinfo>& headInfos,
    std::vector<TerrainLayerOptions>& options,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
    const TerrainTilesetOptions& terrainOptions = TerrainTilesetOptions()) {
  // Use the projection and tiling scheme of the main layer.
  // Any underlying layers must use the same.
  std::string projectionString = "EPSG:3857";
  CesiumGeospatial::Projection projection;
  CesiumGeospatial::GlobeRectangle quadtreeRectangleGlobe(0.0, 0.0, 0.0, 0.0);
  CesiumGeometry::Rectangle quadtreeRectangleProjected(0.0, 0.0, 0.0, 0.0);
  uint32_t quadtreeXTiles = 1;
  if (projectionString == "EPSG:4326") {
    CesiumGeospatial::GeographicProjection geographic;
    projection = geographic;
    quadtreeRectangleGlobe = GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
    quadtreeRectangleProjected = geographic.project(quadtreeRectangleGlobe);
    quadtreeXTiles = 2;
  } else if (projectionString == "EPSG:3857") {
    CesiumGeospatial::WebMercatorProjection webMercator;
    projection = webMercator;
    quadtreeRectangleGlobe = WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;
    quadtreeRectangleProjected = webMercator.project(quadtreeRectangleGlobe);
    quadtreeXTiles = 1;
  } else {
    errors.emplaceError(fmt::format(
        "layer contained an unknown projection value: {}",
        projectionString));
  }

  BoundingVolume boundingVolume =
      createDefaultLooseEarthBoundingVolume(quadtreeRectangleGlobe);

  CesiumGeometry::QuadtreeTilingScheme tilingScheme(
      quadtreeRectangleProjected,
      quadtreeXTiles,
      1);
  std::vector<LayerJsonTerrainLoader::Layer> layers;
  int32_t availabilityLevels = -1;

  auto option = options.begin();
  bool hasLayerVisble = false;
  int32_t defalutMaxZoom =
      /*tilesetOptions.bSampleTerrain? 24 :*/ terrainOptions.maxTileLevel;

  // 遍历所有图层
  for (auto headinfo : headInfos) {
    int32_t maxZoom =
        /*tilesetOptions.bSampleTerrain ? 24 : */ headinfo.mnMaxDataLevel;
    defalutMaxZoom = defalutMaxZoom > maxZoom ? maxZoom : defalutMaxZoom;
    QuadtreeRectangleAvailability availability(
        tilingScheme,
        uint32_t(maxZoom) - 1);
    // CesiumGeospatial::GlobeRectangle boundsRectangle(
    //    headinfo.rectanglewest,
    //    headinfo.rectanglesouth,
    //    headinfo.rectangleeast,
    //    headinfo.rectanglenorth);
    CesiumGeometry::Rectangle projectLayerRectangle =
        // quadtreeRectangleProjected;
        projectRectangleSimple(projection, headinfo.region);
    // 图层可见时设置图层可用级数
    hasLayerVisble = true;
    for (uint32_t level = 0; level < static_cast<uint32_t>(maxZoom); ++level) {
      auto tileMin = tilingScheme.positionToTile(
          projectLayerRectangle.getLowerLeft(),
          level);
      auto tileMax = tilingScheme.positionToTile(
          projectLayerRectangle.getUpperRight(),
          level);
      auto tileMin1 = tilingScheme.positionToTile(
          projectLayerRectangle.getUpperLeft(),
          level);
      auto tileMax1 = tilingScheme.positionToTile(
          projectLayerRectangle.getLowerRight(),
          level);
      availability.addAvailableTileRange(QuadtreeTileRectangularRange{
          level,
          tileMin->x,
          tileMin->y,
          tileMax->x,
          tileMax->y});
    }
    std::vector<std::string> urls;
    std::string FileFormat =
        headinfo.mnFileFormat == EVSSCFileFormat::SSC_FileFormat_BIL_GZIP
            ? ".gz"
            : "";
    std::string templeUrl =
        option->dataType == LayerDataType::LDT_LocalFile
            ? ""
            : "earthview/rest/services/edeserver/tms/" + option->serviceName +
                  "/{EVlevel}/{x}/{y}" +
                  FileFormat; // 区分是本地还是在线地形,server地形模板地址
    urls.push_back(templeUrl);
    std::string layerUrl =
        option->url.back() == '/' ? option->url : option->url + "/";
    std::string baseRequestUrl =
        option->dataType == LayerDataType::LDT_LocalFile
            ? layerUrl
            : EVUtility::UrlEncode(layerUrl); // 在线地形需要用网络编码路径
    layers.emplace_back(LayerJsonTerrainLoader::Layer{
        baseRequestUrl,
        "1.0",
        std::move(urls),
        "",
        std::move(availability),
        static_cast<uint32_t>(maxZoom),
        -1});
    option->headInfo = headinfo;
    option++;
  }
  // int32_t maxZoom = headinfo.mnMaxDataLevel;
  // 添加空地形
  std::vector<std::string> defalutUrls;
  defalutUrls.push_back("");
  TerrainLayerOptions
      tempOption("0", "defalutTerrain", "", true, LayerDataType::LDT_LocalFile);
  tempOption.headInfo.mnSamplesPerTile = 133;
  tempOption.headInfo.mnDataType = SSC_LayerDataType_DEM_int16;
  options.push_back(tempOption);
  QuadtreeRectangleAvailability defalutAvailability(
      tilingScheme,
      uint32_t(defalutMaxZoom) - 1);
  for (uint32_t level = 0; level < static_cast<uint32_t>(defalutMaxZoom);
       ++level) {
    auto tileMin = tilingScheme.positionToTile(
        quadtreeRectangleProjected.getLowerLeft(),
        level);
    auto tileMax = tilingScheme.positionToTile(
        quadtreeRectangleProjected.getUpperRight(),
        level);
    auto tileMin1 = tilingScheme.positionToTile(
        quadtreeRectangleProjected.getUpperLeft(),
        level);
    auto tileMax1 = tilingScheme.positionToTile(
        quadtreeRectangleProjected.getLowerRight(),
        level);
    defalutAvailability.addAvailableTileRange(QuadtreeTileRectangularRange{
        level,
        tileMin->x,
        tileMin->y,
        tileMax->x,
        tileMax->y});
  }
  layers.emplace_back(LayerJsonTerrainLoader::Layer{
      "",
      "1.0",
      std::move(defalutUrls),
      "",
      std::move(defalutAvailability),
      static_cast<uint32_t>(defalutMaxZoom),
      availabilityLevels});

  auto pLoader = std::make_unique<TerrainLayerInfoLoader>(
      tilingScheme,
      projection,
      std::move(layers),
      options,
      tilesetOptions);

  std::unique_ptr<Tile> pRootTile =
      std::make_unique<Tile>(pLoader.get(), TileEmptyContent());
  pRootTile->setUnconditionallyRefine();
  pRootTile->setBoundingVolume(boundingVolume);
  // pRootTile->setOriginBoundingVolume(boundingVolume);

  std::vector<Tile> childTiles;
  childTiles.reserve(quadtreeXTiles);

  for (uint32_t i = 0; i < quadtreeXTiles; ++i) {
    Tile& childTile = childTiles.emplace_back(pLoader.get());

    QuadtreeTileID id(0, i, 0);
    childTile.setTileID(id);

    const CesiumGeospatial::GlobeRectangle childGlobeRectangle =
        unprojectRectangleSimple(projection, tilingScheme.tileToRectangle(id));
    childTile.setBoundingVolume(
        createDefaultLooseEarthBoundingVolume(childGlobeRectangle));
    /*childTile.setOriginBoundingVolume(
        createDefaultLooseEarthBoundingVolume(childGlobeRectangle));*/
    childTile.setGeometricError(
        8.0 * calcQuadtreeMaxGeometricError(Ellipsoid::CGCS2000) *
        childGlobeRectangle.computeWidth());
  }

  pRootTile->createChildTiles(std::move(childTiles));

  // add credits
  std::vector<LoaderCreditResult> credits;
  // credits.reserve(layerCredits.size());
  // for (auto& credit : layerCredits) {
  //  credits.emplace_back(LoaderCreditResult{std::move(credit), true});
  //}
  TilesetContentLoaderResult<TerrainLayerInfoLoader> layerResult{
      std::move(pLoader),
      std::move(pRootTile),
      std::move(credits),
      std::vector<IAssetAccessor::THeader>{},
      std::move(errors)};
  return asyncSystem.createResolvedFuture(std::move(layerResult));
}

Future<DemlayerHeadinfo> loadDemHeadInfo(
    const TilesetExternals& externals,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
    TerrainLayerOptions& inLayerInfos) {
  if (inLayerInfos.url.find("http:") != std::string::npos ||
      inLayerInfos.url.find("https:") != std::string::npos) {
    inLayerInfos.dataType = LayerDataType::LDT_EVServer;
    std::string utf8url =
        inLayerInfos.url +
        "/earthview/rest/services/edeserver/getLayerInfo?layer=" +
        inLayerInfos.serviceName;
    // 读取web地形的头信息
    return externals.pAssetAccessor
        ->get(
            externals.asyncSystem,
            EVUtility::UrlEncode(utf8url),
            requestHeaders)
        .thenInMainThread([layerUrl = inLayerInfos.url,
                           layerName = inLayerInfos.name,
                           asyncSystem = externals.asyncSystem,
                           pAssetAccessor = externals.pAssetAccessor](
                              const std::shared_ptr<IAssetRequest>&
                                  pCompletedRequest) {
          DemlayerHeadinfo headinfo;
          TilesetContentLoaderResult<TerrainLayerInfoLoader> result;
          const CesiumAsync::IAssetResponse* pResponse =
              pCompletedRequest->response();
          const std::string& tileUrl = pCompletedRequest->url();
          if (!pResponse) {
            result.errors.emplaceError(fmt::format(
                "Did not receive a valid response for tile content {}",
                tileUrl));
            return headinfo;
          }

          uint16_t statusCode = pResponse->statusCode();
          if (statusCode != 0 && (statusCode < 200 || statusCode >= 300)) {
            result.errors.emplaceError(fmt::format(
                "Received status code {} for tile content {}",
                statusCode,
                tileUrl));
            result.statusCode = statusCode;
            return headinfo;
          }
          const CesiumAsync::HttpHeaders& completedRequestHeaders =
              pCompletedRequest->headers();
          std::vector<IAssetAccessor::THeader> flatHeaders(
              completedRequestHeaders.begin(),
              completedRequestHeaders.end());
          auto data = pResponse->data();
          rapidjson::Document layerJson;
          layerJson.Parse(
              reinterpret_cast<const char*>(pResponse->data().data()),
              pResponse->data().size());
          if (layerJson.HasParseError()) {
            result.errors.emplaceError(fmt::format(
                "Error when parsing layer.json, error code {} at byte "
                "offset {}",
                layerJson.GetParseError(),
                layerJson.GetErrorOffset()));
            return headinfo;
          }
          // 解析头信息
          const auto DataTypeIt = layerJson.FindMember("tileDataFormat");
          if (DataTypeIt != layerJson.MemberEnd() &&
              DataTypeIt->value.IsString()) {
            std::string datatype = DataTypeIt->value.GetString();
            if (datatype == "TDF_DEM_Int16") {
              headinfo.mnDataType =
                  EVSSCLayerDataType::SSC_LayerDataType_DEM_int16;
            } else if (datatype == "TDF_DEM_Int8") {
              headinfo.mnDataType =
                  EVSSCLayerDataType::SSC_LayerDataType_DEM_int8;
            } else if (datatype == "TDF_DEM_Int") {
              headinfo.mnDataType =
                  EVSSCLayerDataType::SSC_LayerDataType_DEM_int;
            } else if (datatype == "TDF_DEM_Float") {
              headinfo.mnDataType =
                  EVSSCLayerDataType::SSC_LayerDataType_DEM_Float;
            }
          }
          const auto fileFormatIt = layerJson.FindMember("dataStorageFormat");
          if (fileFormatIt != layerJson.MemberEnd() &&
              fileFormatIt->value.IsString()) {
            std::string fileformat = fileFormatIt->value.GetString();
            if (fileformat == "DataStorageType_BIL_GZIP") {
              headinfo.mnFileFormat = EVSSCFileFormat::SSC_FileFormat_BIL_GZIP;
            }
          }
          const auto minDataLevelIt = layerJson.FindMember("minLevel");
          if (minDataLevelIt != layerJson.MemberEnd() &&
              minDataLevelIt->value.IsNumber()) {
            headinfo.mnMinDataLevel = minDataLevelIt->value.GetInt();
          }
          const auto MaxDataLevelIt = layerJson.FindMember("maxLevel");
          if (MaxDataLevelIt != layerJson.MemberEnd() &&
              MaxDataLevelIt->value.IsNumber()) {
            headinfo.mnMaxDataLevel = MaxDataLevelIt->value.GetInt();
          }
          const auto samplesPerTileIt = layerJson.FindMember("tileWidth");
          if (samplesPerTileIt != layerJson.MemberEnd() &&
              samplesPerTileIt->value.IsNumber()) {
            headinfo.mnSamplesPerTile = samplesPerTileIt->value.GetInt();
          }
          double minx = layerJson.FindMember("minx")->value.GetDouble();
          double maxx = layerJson.FindMember("maxx")->value.GetDouble();
          double miny = layerJson.FindMember("miny")->value.GetDouble();
          double maxy = layerJson.FindMember("maxy")->value.GetDouble();

          std::string tileSpatialIt =
              layerJson.FindMember("tileSpatial")->value.GetString();
          if (tileSpatialIt == "4326") {
            headinfo.mnSRS = EVSSCSRS::SSC_SRS_WGS_1984;
            headinfo.region = CesiumGeospatial::GlobeRectangle(
                minx,
                atan((exp(miny) - exp(-miny)) / 2),
                maxx,
                atan((exp(maxy) - exp(-maxy)) / 2));
          } else if (tileSpatialIt == "54004") {
            headinfo.mnSRS = EVSSCSRS::SSC_SRS_Mercator;
            CesiumGeometry::Rectangle projectRectangle(minx, miny, maxx, maxy);
            CesiumGeospatial::WebMercatorProjection webMercator;
            headinfo.region = CesiumGeospatial::unprojectRectangleSimple(
                webMercator,
                projectRectangle);
          }

          headinfo.mnLevelNum =
              headinfo.mnMaxDataLevel - headinfo.mnMinDataLevel + 1;
          // headinfo.mnSRS = configs->srs;
          /*headinfo.mstrDataTime = configs->strDataTime;
          headinfo.mstrUpdateTime = configs->strUpdateTime;*/

          if (layerJson.FindMember("layersName") != layerJson.MemberEnd()) {
            headinfo.mstrLayerName =
                layerJson.FindMember("layersName")->value.GetString();
          }

          if (layerJson.FindMember("dataName") != layerJson.MemberEnd()) {
            headinfo.mstrName =
                layerJson.FindMember("dataName")->value.GetString();
          }

          // std::string requestUrl = EVUtility::MultibyteToUtf8(
          //     layerUrl + EV_REQUEST + layerName + "/");
          return headinfo;
        });
  } else {
    inLayerInfos.dataType = LayerDataType::LDT_LocalFile;
    // 读取本地地形的头信息
    DemlayerHeadinfo headinfo;
    std::string headPath =
        inLayerInfos.url.back() == '/'
            ? (inLayerInfos.url.substr(
                   0,
                   inLayerInfos.url.find_last_not_of('/') + 1) +
               ".ede")
            : inLayerInfos.url + ".ede";
    parseDemHeadInfo(headPath, headinfo);
    return externals.asyncSystem.createResolvedFuture(std::move(headinfo));
  }
}
CesiumAsync::Future<TilesetContentLoaderResult<TerrainLayerInfoLoader>>
TerrainLayerInfoLoader::createLoader(
    const TilesetExternals& externals,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
    std::vector<TerrainLayerOptions>& options,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
    const TerrainTilesetOptions& terrainOptions) {
  // bool bSampleMostDetailed = tilesetOptions.bSampleTerrain;
  auto asyncSystem = externals.asyncSystem;
  std::vector<Future<DemlayerHeadinfo>> demInfoRequests;
  for (auto& it : options) {
    demInfoRequests.emplace_back(
        loadDemHeadInfo(externals, requestHeaders, it));
  }

  return asyncSystem.all(std::move(demInfoRequests))
      .thenInMainThread(
          [asyncSystem, options, tilesetOptions, terrainOptions](
              std::vector<DemlayerHeadinfo>&& headInfoResults) mutable {
            TilesetContentLoaderResult<TerrainLayerInfoLoader> result;

            return constructLoader(
                asyncSystem,
                result.errors,
                headInfoResults,
                options,
                tilesetOptions,
                terrainOptions);
          });
}

CesiumAsync::Future<TileLoadResult> TerrainLayerInfoLoader::loadTileContent(
    const Cesium3DTilesSelection::TileLoadInput& loadInput) {
  const auto& tile = loadInput.tile;
  const auto& asyncSystem = loadInput.asyncSystem;
  const auto& pAssetAccessor = loadInput.pAssetAccessor;
  const auto& pLogger = loadInput.pLogger;
  const auto& requestHeaders = loadInput.requestHeaders;
  const auto& contentOptions = loadInput.contentOptions;

  // This type of loader should never have child loaders.
  // assert(tile.getLoader() == this);

  const QuadtreeTileID* pQuadtreeTileID =
      std::get_if<QuadtreeTileID>(&tile.getTileID());
  if (!pQuadtreeTileID) {
    // check if we need to upsample this tile
    const UpsampledQuadtreeNode* pUpsampleTileID =
        std::get_if<UpsampledQuadtreeNode>(&tile.getTileID());
    if (!pUpsampleTileID) {
      // This loader only handles QuadtreeTileIDs and UpsampledQuadtreeNode.
      return asyncSystem.createResolvedFuture(
          TileLoadResult::createFailedResult(
              loadInput.pAssetAccessor,
              nullptr));
    }

    // now do upsampling
    return upsampleParentTile(tile, asyncSystem);
  }

  // Always request the tile from the first layer in which this tile ID is
  // available.
  auto firstAvailableIt = this->_layers.begin();
  auto optionIt = this->_LayerOptions.begin();
  while (firstAvailableIt != this->_layers.end() &&
         (!firstAvailableIt->contentAvailability.isTileAvailable(
              *pQuadtreeTileID) ||
          !optionIt->visible)) {
    ++firstAvailableIt;
    ++optionIt;
  }

  if (firstAvailableIt == this->_layers.end()) {
    // No layer has this tile available.
    return asyncSystem.createResolvedFuture(
        TileLoadResult::createFailedResult(loadInput.pAssetAccessor, nullptr));
  }

  // Also load the same tile in any underlying layers for which this tile
  // is an availability level. This is necessary because, when we later
  // create this tile's children, we need to be able to create children
  // that are only available from an underlying layer, and we can only do
  // that if we know they're available.
  std::vector<Future<int>> availabilityRequests;

  auto it = firstAvailableIt;
  ++it;

  // while (it != this->_layers.end()) {
  //   if (it->availabilityLevels >= 1 &&
  //       (int32_t(pQuadtreeTileID->level) % it->availabilityLevels) == 0) {
  //     if (!isSubtreeLoadedInLayer(*pQuadtreeTileID, *it)) {
  //       availabilityRequests.emplace_back(loadTileAvailability(
  //           pLogger,
  //           asyncSystem,
  //           pAssetAccessor,
  //           *pQuadtreeTileID,
  //           *it,
  //           requestHeaders));
  //     }
  //   }

  //  ++it;
  //}

  // Start the actual content request.
  auto& currentLayer = *firstAvailableIt;
  Future<HeightMapMeshLoadResult> futureHeightMapMesh = requestTileContent(
      pLogger,
      asyncSystem,
      pAssetAccessor,
      *pQuadtreeTileID,
      tile.getBoundingVolume(),
      currentLayer,
      requestHeaders,
      this->getTilingScheme(),
      tile.getParent(),
      contentOptions,
      *optionIt);

  // determine if this tile is at the availability level of the current layer
  // and if we need to add the availability rectangles to the current layer. We
  // only add the rectangles if those are not not loaded
  bool shouldCurrLayerLoadAvailability = false;
  if (firstAvailableIt->availabilityLevels >= 1 &&
      int32_t(pQuadtreeTileID->level) % currentLayer.availabilityLevels == 0) {
    shouldCurrLayerLoadAvailability =
        !isSubtreeLoadedInLayer(*pQuadtreeTileID, currentLayer);
  }

  // If this tile has availability data, we need to add it to the layer in the
  // main thread.
  if (!availabilityRequests.empty() || shouldCurrLayerLoadAvailability) {
    auto finalFuture =
        availabilityRequests.empty()
            ? std::move(futureHeightMapMesh)
            : asyncSystem.all(std::move(availabilityRequests))
                  .thenImmediately(
                      [futureHeightMapMesh = std::move(futureHeightMapMesh)](
                          std::vector<int>&&) mutable {
                        return std::move(futureHeightMapMesh);
                      });

    return std::move(finalFuture)
        .thenInMainThread([this,
                           asyncSystem,
                           &currentLayer,
                           &tile,
                           shouldCurrLayerLoadAvailability,
                           pAssetAccessor](
                              HeightMapMeshLoadResult&& loadResult) {
          if (shouldCurrLayerLoadAvailability) {
            const QuadtreeTileID& tileID =
                std::get<QuadtreeTileID>(tile.getTileID());
            addRectangleAvailabilityToLayer(
                currentLayer,
                tileID,
                loadResult.availableTileRectangles);
          }

          // if this tile has one of the children that needs to be upsampled, we
          // will need to generate the tile raster overlay UVs in the worker
          // thread based on the projection of the loader since the upsampler
          // needs this UV to do the upsampling
          auto finalResult =
              convertToTileLoadResult(pAssetAccessor, std::move(loadResult));
          bool doesTileHaveUpsampledChild = tileHasUpsampledChild(tile);
          if (doesTileHaveUpsampledChild &&
              finalResult.state == TileLoadResultState::Success) {
            return asyncSystem.runInWorkerThread(
                [finalResult = std::move(finalResult),
                 projection = _projection,
                 tileTransform = tile.getTransform(),
                 tileBoundingVolume = tile.getBoundingVolume()]() mutable {
                  generateRasterOverlayUVs(
                      tileBoundingVolume,
                      tileTransform,
                      projection,
                      finalResult);
                  return finalResult;
                });
          }

          return asyncSystem.createResolvedFuture(std::move(finalResult));
        });
  }

  bool doesTileHaveUpsampledChild = tileHasUpsampledChild(tile);
  return std::move(futureHeightMapMesh)
      .thenImmediately([doesTileHaveUpsampledChild,
                        projection = this->_projection,
                        tileTransform = tile.getTransform(),
                        tileBoundingVolume = tile.getBoundingVolume(),
                        contentOptions,
                        pAssetAccessor](
                           HeightMapMeshLoadResult&& loadResult) mutable {
        if (loadResult.updatedBoundingVolume) {
          const CesiumGeospatial::BoundingRegion* pRegion =
              Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(
                  loadResult.updatedBoundingVolume.value());
          if (pRegion) {
            // 更新region包围盒
            double minZ = pRegion->getMinimumHeight();
            double maxZ = pRegion->getMaximumHeight();
            //for (CesiumGeospatial::CartographicPolygon polygon :
            //     contentOptions.editPolygons) {
            //  if (Cesium3DTilesSelection::CesiumImpl::outsidePolygons(
            //          *pRegion,
            //          {polygon})) {
            //    continue;
            //  }
            //  const std::optional<CesiumGeospatial::GlobeRectangle>&
            //      boundingRectangle = polygon.getBoundingRectangle();
            //  switch (polygon.getUesModel()) {
            //  case CesiumGeospatial::CartographicPolygon::POLYGON_REDUCE_HEIGHT:
            //    if (Cesium3DTilesSelection::CesiumImpl::withinPolygons(
            //            *pRegion,
            //            {polygon})) {
            //      // 瓦块在多边形内
            //      minZ = polygon.getHeight() - 0.1;
            //      maxZ = std::min(polygon.getHeight(), maxZ);
            //    } else if (
            //        boundingRectangle &&
            //        pRegion->getRectangle().computeIntersection(
            //            *boundingRectangle)) {
            //      minZ = minZ > polygon.getHeight() ? polygon.getHeight() - 0.1
            //                                        : minZ;
            //      maxZ = maxZ < polygon.getHeight() ? polygon.getHeight() + 0.1
            //                                        : maxZ;
            //    }
            //    break;
            //  case CesiumGeospatial::CartographicPolygon::POLYGON_HEIGHT:
            //    if (Cesium3DTilesSelection::CesiumImpl::withinPolygons(
            //            *pRegion,
            //            {polygon})) {
            //      // 瓦块在多边形内
            //      minZ = polygon.getHeight() - 0.1;
            //      maxZ = polygon.getHeight() + 0.1;
            //    } else if (
            //        boundingRectangle &&
            //        pRegion->getRectangle().computeIntersection(
            //            *boundingRectangle)) {
            //      minZ = minZ > polygon.getHeight() ? polygon.getHeight() - 0.1
            //                                        : minZ;
            //      maxZ = maxZ < polygon.getHeight() ? polygon.getHeight() + 0.1
            //                                        : maxZ;
            //    }
            //    break;
            //  default:
            //    break;
            //  }
            //}
            loadResult.updatedBoundingVolume = CesiumGeospatial::BoundingRegion(
                pRegion->getRectangle(),
                minZ,
                maxZ);
          }
        }

        // if this tile has one of the children needs to be upsampled, we will
        // need to generate its raster overlay UVs in the worker thread based
        // on the projection of the loader since the upsampler needs this UV
        // to do the upsampling
        auto result =
            convertToTileLoadResult(pAssetAccessor, std::move(loadResult));
        if (doesTileHaveUpsampledChild &&
            result.state == TileLoadResultState::Success) {
          generateRasterOverlayUVs(
              tileBoundingVolume,
              tileTransform,
              projection,
              result);
        }

        return result;
      });
}

Future<std::vector<float>> TerrainLayerInfoLoader::getTileHeightData(
    const AsyncSystem& asyncSystem,
    const std::shared_ptr<IAssetAccessor>& pAssetAccessor,
    const QuadtreeTileID& tileID,
    const BoundingVolume& boundingVolume,
    const std::vector<IAssetAccessor::THeader>& requestHeaders,
    const Cesium3DTilesSelection::TilesetContentOptions& contentOptions) {
  // Always request the tile from the first layer in which this tile ID is
  // available.
  auto firstAvailableIt = this->_layers.begin();
  auto optionIt = this->_LayerOptions.begin();
  while (firstAvailableIt != this->_layers.end() &&
         (!firstAvailableIt->contentAvailability.isTileAvailable(tileID) ||
          !optionIt->visible)) {
    ++firstAvailableIt;
    ++optionIt;
  }
  auto it = firstAvailableIt;
  ++it;
  // Start the actual content request.
  auto& layer = *firstAvailableIt;
  const TerrainLayerOptions& layerOptions = *optionIt;
  const QuadtreeTilingScheme& tilingScheme = this->getTilingScheme();
  if (layer.baseUrl.empty()) {
    return asyncSystem.createResolvedFuture(std::vector<float>());
  }

  QuadtreeTileID requestTileId = tileID;
  bool useParentData = false;
  std::vector<QuadtreeTileID> tileIds;
  // 是否需要从父级请求数据
  while (requestTileId.level > layerOptions.headInfo.mnMaxDataLevel - 1) {
    tileIds.emplace_back(requestTileId);
    requestTileId = requestTileId.getParent();
    useParentData = true;
  }

  std::string url = resolveInvertedYTileUrl(requestTileId, layer, tilingScheme);
  return pAssetAccessor
      ->get(
          asyncSystem,
          {url/*,
           "Server7_" + layerOptions.headInfo.mstrName,
           requestTileId.level + 1,
           requestTileId.computeInvertedY(tilingScheme),
           requestTileId.x,
           true,
           false,
           false*/},
          requestHeaders)
      .thenInWorkerThread(
          [asyncSystem,
           tileID,
           boundingVolume,
           tilingScheme,
           useParentData,
           requestTileId,
           tileIds,
           contentOptions,
           layerOptions](std::shared_ptr<IAssetRequest>&& pRequest) mutable {
            std::vector<float> vectortempData;
            const IAssetResponse* pResponse = pRequest->response();
            if (!pResponse) {
              return vectortempData;
            }

            if (pResponse->statusCode() != 0 &&
                (pResponse->statusCode() < 200 ||
                 pResponse->statusCode() >= 300)) {
              return vectortempData;
            }

            if (useParentData && pResponse->data().size() != 0) {
              std::vector<std::byte> vectorData;
              convertDataToHeightBUffer(
                  pResponse->data(),
                  vectorData,
                  contentOptions.enableWaterMask,
                  layerOptions.headInfo,
                  layerOptions.exaggerationScale);
              std::shared_ptr<std::vector<std::byte>> tempdata =
                  std::make_shared<std::vector<std::byte>>(vectorData);
              while (!tileIds.empty()) {
                tempdata = HeightMapMeshLoader::UpsamplingFromParentData(
                    tempdata.get(),
                    tileIds.back(),
                    tilingScheme);
                tileIds.pop_back();
              }
              auto newheadInfo = layerOptions.headInfo;
              newheadInfo.mnDataType =
                  EVSSCLayerDataType::SSC_LayerDataType_DEM_Float;
              convertDataToHeight(
                  *tempdata,
                  vectortempData,
                  newheadInfo,
                  layerOptions.exaggerationScale,
                  layerOptions.raiseHeight);
              return vectortempData;
            }
            convertDataToHeight(
                pResponse->data(),
                vectortempData,
                layerOptions.headInfo,
                layerOptions.exaggerationScale,
                layerOptions.raiseHeight);
            return vectortempData;
          });
}

TerrainLayerOptions
TerrainLayerInfoLoader::getlayerOptions(std::string layername) {
  for (auto option : this->_LayerOptions) {
    if (option.name == layername) {
      return option;
    }
  }
  return TerrainLayerOptions();
}

Future<bool> TerrainLayerInfoLoader::addLayer(
    const Cesium3DTilesSelection::TilesetExternals& externals,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
    TerrainLayerOptions& option,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions) {
  // bool bSampleMostDetailed = tilesetOptions.bSampleTerrain;
  auto asyncSystem = externals.asyncSystem;
  return loadDemHeadInfo(externals, requestHeaders, option)
      .thenInMainThread([asyncSystem, tilesetOptions, &option, this](
                            DemlayerHeadinfo&& headinfo) mutable {
        std::string projectionString = "EPSG:3857";
        CesiumGeospatial::Projection projection;
        CesiumGeospatial::GlobeRectangle quadtreeRectangleGlobe(
            0.0,
            0.0,
            0.0,
            0.0);
        CesiumGeometry::Rectangle quadtreeRectangleProjected(
            0.0,
            0.0,
            0.0,
            0.0);
        uint32_t quadtreeXTiles = 1;
        if (projectionString == "EPSG:4326") {
          CesiumGeospatial::GeographicProjection geographic;
          projection = geographic;
          quadtreeRectangleGlobe =
              GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
          quadtreeRectangleProjected =
              geographic.project(quadtreeRectangleGlobe);
          quadtreeXTiles = 2;
        } else if (projectionString == "EPSG:3857") {
          CesiumGeospatial::WebMercatorProjection webMercator;
          projection = webMercator;
          quadtreeRectangleGlobe =
              WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;
          quadtreeRectangleProjected =
              webMercator.project(quadtreeRectangleGlobe);
          quadtreeXTiles = 1;
        }
        CesiumGeometry::QuadtreeTilingScheme tilingScheme(
            quadtreeRectangleProjected,
            quadtreeXTiles,
            1);
        int32_t defalutMaxZoom = /*tilesetOptions.bSampleTerrain ? 24 :*/ 10;
        int32_t availabilityLevels = -1;
        int32_t maxZoom = /*tilesetOptions.bSampleTerrain ? 24 :*/ headinfo.mnMaxDataLevel;
        defalutMaxZoom = defalutMaxZoom > maxZoom ? maxZoom : defalutMaxZoom;
        QuadtreeRectangleAvailability availability(
            tilingScheme,
            uint32_t(maxZoom) - 1);
        // CesiumGeospatial::GlobeRectangle boundsRectangle(
        //   headinfo.rectanglewest,
        //   headinfo.rectanglesouth,
        //   headinfo.rectangleeast,
        //   headinfo.rectanglenorth);
        CesiumGeometry::Rectangle projectLayerRectangle =
            // quadtreeRectangleProjected;
            projectRectangleSimple(projection, headinfo.region);
        // 图层可见时设置图层可用级数
        for (uint32_t level = 0; level < static_cast<uint32_t>(maxZoom);
             ++level) {
          auto tileMin = tilingScheme.positionToTile(
              projectLayerRectangle.getLowerLeft(),
              level);
          auto tileMax = tilingScheme.positionToTile(
              projectLayerRectangle.getUpperRight(),
              level);
          auto tileMin1 = tilingScheme.positionToTile(
              projectLayerRectangle.getUpperLeft(),
              level);
          auto tileMax1 = tilingScheme.positionToTile(
              projectLayerRectangle.getLowerRight(),
              level);
          availability.addAvailableTileRange(QuadtreeTileRectangularRange{
              level,
              tileMin->x,
              tileMin->y,
              tileMax->x,
              tileMax->y});
        }
        std::vector<std::string> urls;
        std::string FileFormat =
            headinfo.mnFileFormat == EVSSCFileFormat::SSC_FileFormat_BIL_GZIP
                ? ".gz"
                : "";
        std::string templeUrl =
            option.dataType == LayerDataType::LDT_LocalFile
                ? ""
                : "earthview/rest/services/edeserver/tms/" +
                      option.serviceName + "/{EVlevel}/{x}/{y}" +
                      FileFormat; // 区分是本地还是在线地形,server地形模板地址
        urls.push_back(templeUrl);
        std::string layerUrl =
            option.url.back() == '/' ? option.url : option.url + "/";
        std::string baseRequestUrl =
            option.dataType == LayerDataType::LDT_LocalFile
                ? layerUrl
                : EVUtility::UrlEncode(layerUrl); // 在线地形需要用网络编码路径
        option.headInfo = headinfo;
        auto optionIt = this->_LayerOptions.begin();
        while (optionIt != this->_LayerOptions.end()) {
          if ((*optionIt).name == option.name) {
            *optionIt = option;
            break;
          } else {
            ++optionIt;
          }
        }
        if (optionIt == this->_LayerOptions.end()) {
          this->_layers.insert(
              this->_layers.begin(),
              LayerJsonTerrainLoader::Layer{
                  baseRequestUrl,
                  "1.0",
                  std::move(urls),
                  "",
                  std::move(availability),
                  static_cast<uint32_t>(maxZoom),
                  -1});
          this->_LayerOptions.insert(this->_LayerOptions.begin(), option);
        }

        return true;
      });
}

void TerrainLayerInfoLoader::removeLayer(
    const Cesium3DTilesSelection::TilesetExternals& externals,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
    const std::string& layername,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions) {
  auto layerOptionsIt = this->_LayerOptions.begin();
  auto layerIt = this->_layers.begin();
  while (layerOptionsIt != this->_LayerOptions.end()) {
    if ((*layerOptionsIt).name == layername) {
      this->_LayerOptions.erase(layerOptionsIt);
      this->_layers.erase(layerIt);
      break;
    }
    layerOptionsIt++;
    layerIt++;
  }
  return;
}

// std::vector<double> TerrainLayerInfoLoader::getTerrainHeight(
//     std::vector<CesiumGeospatial::Cartographic> positons,
//     const Cesium3DTilesSelection::TilesetExternals& externals,
//     const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders)
//     {
//   return std::vector<double>();
// }

} // namespace EVScene
