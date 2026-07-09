#pragma once
#include "TilesetContentManager.h"
#include "Cesium3DTilesSelection/BoundingVolume.h"
#include "LayerOptions.h"
#include "CesiumGeospatial/GlobeRectangle.h"

namespace EVScene {
/**
* 地形tileset管理器
*/
class TerrainTilesetContentManager
    : public Cesium3DTilesSelection::TilesetContentManager {
public:
  TerrainTilesetContentManager(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      Cesium3DTilesSelection::RasterOverlayCollection&& overlayCollection,
      std::vector<TerrainLayerOptions>& options,
      const TerrainTilesetOptions& terrainOpitons = TerrainTilesetOptions());


  CesiumGeospatial::GlobeRectangle getLayRectangle(std::string layername);

  //CesiumAsync::Future<std::vector<CesiumGeospatial::Cartographic>>
  //sampleTerrainMostDetailed(
  //    std::vector<CesiumGeospatial::Cartographic>& positons);

  CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>
  getTileContent(const Cesium3DTilesSelection::TileLoadInput& loadInput);

  CesiumAsync::Future<std::vector<float>> getTileHeightData(
      const Cesium3DTilesSelection::Tile& tile,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions);
  CesiumAsync::Future<bool> addLayer(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      TerrainLayerOptions& options);
  void removeLayer(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      const std::string& layername);

	/// <summary>
	/// 存在创建慢的情况
	/// </summary>
	/// <returns></returns>
	bool IsLoaderCreated();

public:
  float mfAddHeight;//海水用的抬高
};
} // namespace EVScene
