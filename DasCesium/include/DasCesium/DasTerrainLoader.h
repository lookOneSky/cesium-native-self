
#pragma once

#pragma region das
#include <DasCesium/DasOptions.h>
#include <Cesium3DTilesSelection/EllipsoidTilesetLoader.h>
#include <Cesium3DTilesSelection/Tileset.h>
#pragma endregion

#pragma region das
namespace DasCesium {
#pragma endregion

#pragma region das
/**
 * @brief Das自定义地形加载器，继承自EllipsoidTilesetLoader
 * 支持通过DasTerrainOptionsExtra配置额外的地形选项，如高度偏移
 */
class DasTerrainLoader : public Cesium3DTilesSelection::EllipsoidTilesetLoader {
public:
  /**
   * @brief 构造函数
   *
   * @param ellipsoid 椭球体参数
   * @param dasOptions Das自定义地形选项
   */
  DasTerrainLoader(
      const CesiumGeospatial::Ellipsoid& ellipsoid,
      const DasTerrainOptionsExtra& dasOptions = DasTerrainOptionsExtra{});
#pragma endregion

#pragma region das
  /**
   * @brief 创建带有Das自定义选项的Tileset
   *
   * @param externals 外部接口
   * @param options Tileset选项
   * @param dasOptions Das自定义地形选项
   * @return 创建的Tileset实例
   */
  static std::unique_ptr<Cesium3DTilesSelection::Tileset> createTileset(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& options =
          Cesium3DTilesSelection::TilesetOptions{},
      const DasTerrainOptionsExtra& dasOptions = DasTerrainOptionsExtra{});
#pragma endregion

#pragma region das
  CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>
  loadTileContent(const Cesium3DTilesSelection::TileLoadInput& input) override;
#pragma endregion

#pragma region das
protected:
  /**
   * @brief 重写createGeometry方法，在生成几何体时应用高度偏移
   *
   * @param tile 瓦块对象
   * @return 生成的几何体数据
   */
  Geometry createGeometry(const Cesium3DTilesSelection::Tile& tile) const override;
#pragma endregion

#pragma region das
  virtual CesiumGeospatial::BoundingRegion
  createBoundingRegion(const CesiumGeometry::QuadtreeTileID& quadtreeID) const override;
#pragma endregion

#pragma region das
private:
  DasTerrainOptionsExtra _dasOptions; // Das自定义地形选项
};
#pragma endregion

#pragma region das
} // namespace DasCesium
#pragma endregion
