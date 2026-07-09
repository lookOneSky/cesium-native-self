#include "EVScene/TerrainTilesetContentManager.h"
#include "EVScene/TerrainLayerInfoLoader.h"
#include "EVScene/LayerOptions.h"
#include "EVScene/HeightMapMeshLoader.h"

#include "LayerJsonTerrainLoader.h"
#include "TileContentLoadInfo.h"
#include "TilesetJsonLoader.h"
#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include "CesiumGeospatial/WebMercatorProjection.h"
#include <CesiumUtility/Math.h>

#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/Future.h>

#include <CesiumUtility/IntrusivePointer.h>
#include <rapidjson/document.h>
#include <spdlog/logger.h>
#include "EVScene/TerrainTilesetContentManager.h"

using namespace CesiumGeospatial;
using namespace CesiumUtility;
using namespace Cesium3DTilesSelection;

namespace EVScene {

TerrainTilesetContentManager::TerrainTilesetContentManager(
    const Cesium3DTilesSelection::TilesetExternals& externals,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
    Cesium3DTilesSelection::RasterOverlayCollection&& overlayCollection,
    std::vector<TerrainLayerOptions>& options,
    const TerrainTilesetOptions& terrainOptions)
    : Cesium3DTilesSelection::TilesetContentManager(
          externals,
          tilesetOptions,
          std::move(overlayCollection),
          "") {
  mfAddHeight = 0;
  this->notifyTileStartLoading(nullptr);
  CesiumUtility::IntrusivePointer<TerrainTilesetContentManager> thiz = this;
  //externals.asyncSystem.createResolvedFuture()
  //    .thenInWorkerThread([pLogger = externals.pLogger,
  //                         asyncSystem = externals.asyncSystem,
  //                         pAssetAccessor = externals.pAssetAccessor,
  //                         contentOptions = tilesetOptions.contentOptions,
  //                         showCreditsOnScreen =
  //                             tilesetOptions.showCreditsOnScreen,
  //                         options = options]() mutable {
  //      return TerrainLayerInfoLoader::createLoader(
  //                 externals,
  //                 this->_requestHeaders, options)
  //          .thenImmediately(
  //              [](Cesium3DTilesSelection::TilesetContentLoaderResult<
  //                  Cesium3DTilesSelection::TilesetContentLoader>&& result) {
  //                return std::move(result);
  //              });
  //    })

  TerrainLayerInfoLoader::createLoader(
      externals,
      this->_requestHeaders,
      options,
      tilesetOptions,
      terrainOptions)
      .thenInMainThread(
          [thiz, errorCallback = tilesetOptions.loadErrorCallback](
              Cesium3DTilesSelection::TilesetContentLoaderResult<
                  Cesium3DTilesSelection::TilesetContentLoader>&& result) {

            TerrainLayerInfoLoader* pLoader = dynamic_cast<TerrainLayerInfoLoader*>(result.pLoader.get());
            if (pLoader)
            {
              for (auto& item : pLoader->_LayerOptions)
              {
                item.raiseHeight += thiz->mfAddHeight;
              }
            }

            thiz->notifyTileDoneLoading(result.pRootTile.get());
            thiz->propagateTilesetContentLoaderResult(
                Cesium3DTilesSelection::TilesetLoadType::TilesetJson,
                errorCallback,
                std::move(result));
            thiz->_rootTileAvailablePromise.resolve();
          })
      .catchInMainThread([thiz](std::exception&& e) {
        thiz->notifyTileDoneLoading(nullptr);
        SPDLOG_LOGGER_ERROR(
            thiz->_externals.pLogger,
            "An unexpected error occurs when loading tile: {}",
            e.what());
      });
}

CesiumGeospatial::GlobeRectangle
TerrainTilesetContentManager::getLayRectangle(std::string layername) {
  if (!_pLoader) {
    return CesiumGeospatial::GlobeRectangle::EMPTY;
  }
  TerrainLayerInfoLoader* p =dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
  TerrainLayerOptions layerOption = p->getlayerOptions(layername);

  return layerOption.headInfo.region;
    /*CesiumGeospatial::GlobeRectangle(
      layerOption.headInfo.rectanglewest,
      layerOption.headInfo.rectanglesouth,
      layerOption.headInfo.rectangleeast,
      layerOption.headInfo.rectanglenorth);*/
}

static uint32_t getUV2DTo1D(double u, double v, double width) {
  uint32_t x = static_cast<uint32_t>(u);
  uint32_t y = static_cast<uint32_t>(v);
  return y * width + x;
}

double getHeightFromData(
    const std::vector<std::byte>& pdata,
    double u,
    double v) {
  if (pdata.empty()) {
    return 0.0;
  }
  double dataWidth = sqrt((pdata.size() / 4));
  double width = dataWidth == 65.0 ? 65 : 129;

  int offsetX = 0;
  if (dataWidth < 65.0) {
    return 0.0;
  }
  if (dataWidth == 133.0) {
    offsetX = (dataWidth - width) / 2;
  }
  u = u * (width - 1);
  v = v * (width - 1);

  double left = floor(u);
  double right = ceil(u);
  double top = floor(v);
  double bottom = ceil(v);
  uint32_t leftTopBufferId =
      getUV2DTo1D(left + offsetX, top + offsetX, dataWidth);
  float leftTopHeight =
      *reinterpret_cast<const float*>(&pdata[leftTopBufferId * 4]);
  uint32_t rightTopBufferId =
      getUV2DTo1D(right + offsetX, top + offsetX, dataWidth);
  float rightTopHeight =
      *reinterpret_cast<const float*>(&pdata[rightTopBufferId * 4]);
  uint32_t leftBottomBufferId =
      getUV2DTo1D(left + offsetX, bottom + offsetX, dataWidth);
  float leftBottomHeight =
      *reinterpret_cast<const float*>(&pdata[leftBottomBufferId * 4]);
  uint32_t rightBottomBufferId =
      getUV2DTo1D(right + offsetX, bottom + offsetX, dataWidth);
  float rightBottomHeight =
      *reinterpret_cast<const float*>(&pdata[rightBottomBufferId * 4]);
  double lerpTopHeight = Math::lerp(static_cast<double>(leftTopHeight), static_cast<double>(rightTopHeight),u - left);
  double lerpBottomHeight =
      Math::lerp(static_cast<double>(leftBottomHeight), static_cast<double>(rightBottomHeight), u - left);
  return Math::lerp(static_cast<double>(lerpTopHeight), static_cast<double>(lerpBottomHeight), v - top);
}

std::shared_ptr<std::vector<std::byte>> upsampleParent(
    const std::vector<std::byte>* parent_data,
    const CesiumGeometry::QuadtreeTileID& dataTileID,
    const CesiumGeometry::QuadtreeTileID& parentTileID,
    const CesiumGeometry::QuadtreeTileID& childTileID,
    const CesiumGeometry::QuadtreeTilingScheme& tilingScheme){
  if (parentTileID == dataTileID) {
    return HeightMapMeshLoader::UpsamplingFromParentData(
        parent_data,
        childTileID,
        tilingScheme);
  }
  if (childTileID.getParent() == parentTileID) {
    return HeightMapMeshLoader::UpsamplingFromParentData(
        parent_data,
        childTileID,
        tilingScheme);
  }
  return upsampleParent(
      parent_data,
      dataTileID,
      childTileID.getParent(),
      childTileID,
      tilingScheme);
};

// CesiumAsync::Future<std::vector<CesiumGeospatial::Cartographic>>
//TerrainTilesetContentManager::sampleTerrainMostDetailed(
//    std::vector<CesiumGeospatial::Cartographic>& positons) {
//  TerrainLayerInfoLoader* pLoader =
//      dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
//   uint32_t maxReLevel = pLoader->_LayerOptions[0].headInfo.mnMaxDataLevel-1;
//
//  std::unordered_map<CesiumGeometry::QuadtreeTileID, std::vector<std::byte>> tempDatas;
//  CesiumGeospatial::WebMercatorProjection webMercator;
//  std::vector<CesiumGeometry::QuadtreeTileID> tileIds;
//  std::vector<CesiumAsync::Future<TileLoadResult>> tileRequests;
//  for (size_t i = 0; i < positons.size(); i++) {
//    glm::dvec3 projectPos = webMercator.project(positons[i]);
//    std::optional<CesiumGeometry::QuadtreeTileID> tileId =
//        pLoader->getTilingScheme().positionToTile(
//            glm::dvec2(projectPos.x, projectPos.y),
//            maxReLevel);
//    if (!tileId) {
//      continue;
//    }
//    bool alreadyHas = std::any_of(
//        tileIds.begin(),
//        tileIds.end(),
//        [tileId](const CesiumGeometry::QuadtreeTileID& loadedTileid) noexcept {
//          return loadedTileid == tileId;
//        });
//    if (alreadyHas) {
//      continue;
//    }
//    Tile tile(pLoader);
//    tile.setTileID(tileId.value());
//    CesiumGeometry::Rectangle boundingRectangle =
//        pLoader->getTilingScheme().tileToRectangle(tileId.value());
//    CesiumGeospatial::GlobeRectangle globeRectangle =
//        webMercator.unproject(boundingRectangle);
//    BoundingVolume boundingVolume = BoundingRegionWithLooseFittingHeights(
//        BoundingRegion(globeRectangle, -1000.0, 9000.0));
//    tile.setBoundingVolume(boundingVolume);
//    TilesetContentOptions contentOptions;
//    std::vector<CesiumGeospatial::CartographicPolygon> editPolygons;
//    //contentOptions.editPolygons = editPolygons;
//
//    TileLoadInput loadInput{
//        tile,
//        contentOptions,
//        this->_externals.asyncSystem,
//        this->_externals.pAssetAccessor,
//        this->_externals.pLogger,
//        this->_requestHeaders/*,
//        this->externalGltfDataRequest*/};
//    tileIds.push_back(tileId.value());
//    tileRequests.push_back(pLoader->loadTileContent(loadInput));
//  }
//  return this->_externals.asyncSystem.all(std::move(tileRequests))
//      .thenInWorkerThread(
//          [positons = std::move(positons),
//           tileIds,
//           pLoader,
//           maxReLevel,
//           tilingScheme = pLoader->getTilingScheme()](
//              std::vector<TileLoadResult>&& loadResults) {
//            std::vector<CesiumGeospatial::Cartographic> positonsResults;
//            CesiumGeospatial::WebMercatorProjection webMercator;
//            for (size_t i = 0; i < positons.size(); i++) {
//              glm::dvec3 projectPos = webMercator.project(positons[i]);
//              std::optional<CesiumGeometry::QuadtreeTileID> tileId =
//                  pLoader->getTilingScheme().positionToTile(
//                      glm::dvec2(projectPos.x, projectPos.y),
//                      maxReLevel);
//
//              if (!tileId) {
//                continue;
//              }
//              for (size_t j = 0; j < tileIds.size(); j++) {
//                if (tileIds[j] == tileId) {
//                  const TileLoadResult& tileResult = loadResults[j];
//                  const CesiumGltf::Model* parentModel =
//                      std::get_if<CesiumGltf::Model>(&tileResult.contentKind);
//                  if (!parentModel) {
//                    break;
//                  }
//                  const auto& attributes =
//                      parentModel->meshes[0].primitives[0].attributes;
//                  if (attributes.find("HEIGHTS") != attributes.end()) {
//                    size_t HeightsBufferId = attributes.find("HEIGHTS")->second;
//                    CesiumGltf::Buffer HeightsBuffer =
//                        parentModel->buffers[HeightsBufferId];
//                  
//                  uint32_t levelIt = maxReLevel+1;
//                  std::shared_ptr<std::vector<std::byte>> tempdata =
//                      std::make_shared<std::vector<std::byte>>(
//                          std::vector<std::byte>());
//                  *tempdata = HeightsBuffer.cesium.data;
//                  while (levelIt < 20) {
//                   std::optional<CesiumGeometry::QuadtreeTileID> tileId2 =
//                       pLoader->getTilingScheme().positionToTile(
//                           glm::dvec2(projectPos.x, projectPos.y),
//                           levelIt);
//                    tempdata = HeightMapMeshLoader::UpsamplingFromParentData(
//                               tempdata.get(),
//                              tileId2.value(),
//                                  tilingScheme);
//                    levelIt++;
//                  }
//                  std::optional<CesiumGeometry::QuadtreeTileID> tileId3 =
//                      pLoader->getTilingScheme().positionToTile(
//                          glm::dvec2(projectPos.x, projectPos.y),
//                          19);
//                  CesiumGeometry::Rectangle boundingRectangleT =
//                      pLoader->getTilingScheme().tileToRectangle(
//                          tileId3.value());
//                  CesiumGeospatial::GlobeRectangle rectangle =
//                      webMercator.unproject(boundingRectangleT);
//                  double terrainWest = rectangle.getWest();
//                  double terrainSouth = rectangle.getSouth();
//                  double terrainEast = rectangle.getEast();
//                  double terrainNorth = rectangle.getNorth();
//                  double UInTerrain = (positons[i].longitude - terrainWest) /
//                                      (terrainEast - terrainWest);
//                  UInTerrain = Math::clamp(UInTerrain, 0.0, 1.0);
//                  double VInTerrain = (terrainNorth - positons[i].latitude) /
//                                      (terrainNorth - terrainSouth);
//                  VInTerrain = Math::clamp(VInTerrain, 0.0, 1.0);
//                  double heightMeters = getHeightFromData(*tempdata,
//                      UInTerrain,
//                      VInTerrain);
//                  positonsResults.push_back(CesiumGeospatial::Cartographic(
//                      positons[i].longitude,
//                      positons[i].latitude,
//                      heightMeters));
//                  break;
//                  }
//                }
//              }
//            }
//            return positonsResults;
//
//        });
// 
//}

 CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>
TerrainTilesetContentManager::getTileContent(
    const Cesium3DTilesSelection::TileLoadInput& loadInput) {
  if (!_pLoader) {
    return loadInput.asyncSystem.createResolvedFuture(
        TileLoadResult::createFailedResult(loadInput.pAssetAccessor, nullptr));
  }
  TerrainLayerInfoLoader* pLoader =
      dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
  return pLoader->loadTileContent(loadInput);

 }

 CesiumAsync::Future<std::vector<float>>
 TerrainTilesetContentManager::getTileHeightData(
     const Cesium3DTilesSelection::Tile& tile,
     const Cesium3DTilesSelection::TilesetOptions& tilesetOptions) {
   const auto& asyncSystem = this->_externals.asyncSystem;
   const auto& pAssetAccessor = this->_externals.pAssetAccessor;
   const auto& pLogger = this->_externals.pLogger;
   const auto& requestHeaders = this->_requestHeaders;
   const auto& contentOptions = tilesetOptions.contentOptions;
   const QuadtreeTileID* pQuadtreeTileID =
       std::get_if<QuadtreeTileID>(&tile.getTileID());
   TerrainLayerInfoLoader* pLoader =
       dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
   return pLoader->getTileHeightData(
       asyncSystem,
       pAssetAccessor,
       *pQuadtreeTileID,
       tile.getBoundingVolume(),
       requestHeaders,
       contentOptions);
 }

CesiumAsync::Future<bool> TerrainTilesetContentManager::addLayer(
     const Cesium3DTilesSelection::TilesetExternals& externals,
     const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
     TerrainLayerOptions& options) {
   TerrainLayerInfoLoader* pLoader =
       dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
   const auto& requestHeaders = this->_requestHeaders;

   return pLoader->addLayer(externals, requestHeaders, options, tilesetOptions);
}

void TerrainTilesetContentManager::removeLayer(
    const Cesium3DTilesSelection::TilesetExternals& externals,
    const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
    const std::string& layername) {
  TerrainLayerInfoLoader* pLoader =
      dynamic_cast<TerrainLayerInfoLoader*>(_pLoader.get());
  const auto& requestHeaders = this->_requestHeaders;
  return pLoader
      ->removeLayer(externals, requestHeaders, layername, tilesetOptions);
}

bool TerrainTilesetContentManager::IsLoaderCreated()
{
  return _pLoader != nullptr;
}

} // namespace EVScene
