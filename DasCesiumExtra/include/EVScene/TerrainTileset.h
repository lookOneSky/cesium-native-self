#pragma once
#include "Library.h"
#include"Cesium3DTilesSelection/Tileset.h"
#include "LayerOptions.h"
#include "CesiumGeospatial/GlobeRectangle.h"

namespace Cesium3DTilesSelection {
class TilesetContentManager;
}

namespace EVScene {
/*
 * ���ζ���
 */
class EVScene_API TerrainTileset : public Cesium3DTilesSelection::Tileset {

public:
  TerrainTileset(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      std::vector<TerrainLayerOptions>& layerOptions,
      const Cesium3DTilesSelection::TilesetOptions& options = Cesium3DTilesSelection::TilesetOptions(),
      const TerrainTilesetOptions& terrainTilesetOption = TerrainTilesetOptions());
  virtual ~TerrainTileset();
  /*
   * 
   */
  CesiumGeospatial::GlobeRectangle
  getLayRectangle(std::string layername);

  //CesiumAsync::Future<std::vector<CesiumGeospatial::Cartographic>> sampleTerrainMostDetailed(
  //    std::vector<CesiumGeospatial::Cartographic>& positons);

  CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>
  getTileContent(const Cesium3DTilesSelection::TileLoadInput& loadInput);

  /// <summary>
  /// 存在创建慢的情况
  /// </summary>
  /// <returns></returns>
  bool IsLoaderCreated();
  /// <summary>
  /// 新加的影像是否加载完成
  /// </summary>
  /// <returns></returns>
  bool IsImageLayerAdded();

  /// <summary>
  /// 返回去边缘的高度信息
  /// </summary>
  /// <param name="tile"></param>
  /// <returns></returns>
  CesiumAsync::Future<std::vector<float>>
  getTileHeightData(
      const Cesium3DTilesSelection::Tile& tile);

  CesiumAsync::Future<bool> addLayer(TerrainLayerOptions& layerOptions);
  void removeLayer(const std::string& layername);

  /// <summary>
  /// 海水抬高设置参数
  /// </summary>
  /// <param name="fAddHeight"></param>
  /// <returns></returns>
  void setTerrainAddHeight(float fAddHeight);
  float getTerrainAddHeight();
};

} // namespace EVScene
