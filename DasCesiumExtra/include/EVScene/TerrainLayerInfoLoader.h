#pragma once

#include <LayerJsonTerrainLoader.h>
#include "CesiumGeospatial/calcQuadtreeMaxGeometricError.h"

#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumUtility/JsonHelpers.h>
#include <CesiumUtility/Uri.h>
#include "LayerOptions.h"

#include <libmorton/morton.h>
#include <rapidjson/document.h>
using namespace Cesium3DTilesSelection;
using namespace CesiumAsync;
using namespace CesiumGeometry;
using namespace CesiumGeospatial;
namespace EVScene {
/**
* 地形json信息加载
*/
class TerrainLayerInfoLoader : public Cesium3DTilesSelection::LayerJsonTerrainLoader {

public:
  
  TerrainLayerInfoLoader(
      const CesiumGeometry::QuadtreeTilingScheme& tilingScheme,
      const CesiumGeospatial::Projection& projection,
      std::vector<Layer>&& layers,
      std::vector<TerrainLayerOptions>& options,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions);

  static CesiumAsync::Future<Cesium3DTilesSelection::TilesetContentLoaderResult<TerrainLayerInfoLoader>>
    createLoader(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
      std::vector<TerrainLayerOptions>& options,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      const TerrainTilesetOptions& terrainOptions = TerrainTilesetOptions());

  //static CesiumAsync::Future<TilesetContentLoaderResult<TerrainLayerInfoLoader>>
  //createLoader(
  //    const CesiumAsync::AsyncSystem& asyncSystem,
  //    const std::string& layerJsonUrl);

  /*重载加载tilecontent函数，支持多图层*/
  CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult> loadTileContent(
      const Cesium3DTilesSelection::TileLoadInput& loadInput) override;

  Future<std::vector<float>> getTileHeightData(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<IAssetAccessor>& pAssetAccessor,
      const QuadtreeTileID& tileID,
      const BoundingVolume& boundingVolume,
      const std::vector<IAssetAccessor::THeader>& requestHeaders,
      const Cesium3DTilesSelection::TilesetContentOptions& contentOptions);

  TerrainLayerOptions getlayerOptions(std::string layername);
  /*std::vector<double> getTerrainHeight(
      std::vector<CesiumGeospatial::Cartographic> positons,
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders);*/
  Future<bool> addLayer(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
       TerrainLayerOptions& options,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions);
  void removeLayer(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
      const std::string& layername,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions);

public:
  std::vector<TerrainLayerOptions> _LayerOptions;
  bool bSampleMostDetailed;
  std::vector<CesiumGeospatial::CartographicPolygon> _editPolygons;
};
}
