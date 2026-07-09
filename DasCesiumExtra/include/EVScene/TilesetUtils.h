#pragma once
#include "Cesium3DTilesSelection/BoundingVolume.h"
#include "Cesium3DTilesSelection/Tile.h"
#include "Cesium3DTilesSelection/Tileset.h"
#pragma region EV
#include "TileUtilities.h"
#include <CesiumGltfContent/GltfUtilities.h>
#pragma endregion
namespace EVScene {

class TilesetUtils {
public:
  TilesetUtils();
  ~TilesetUtils();

  /// <summary>
  /// 改写的旧代码，根节点没遍历。统一
  /// </summary>
  /// <param name="pTile"></param>
  /// <param name="std::function<bool"></param>
  /// <param name="tile"></param>
  /// <param name="callback"></param>
  /// <returns></returns>
  static void forEachTile(
      Cesium3DTilesSelection::Tile* pTile,
      const std::function<bool(Cesium3DTilesSelection::Tile& tile)>& callback);
  ///遍历所有瓦块效率较低，不建议在tick中多次使用。
  static void forEachTile(
      Cesium3DTilesSelection::Tileset* pTile,
      const std::function<bool(Cesium3DTilesSelection::Tile& tile)>& callback);
#pragma region EV
  static void updateTileBoundingVolume(
      const Cesium3DTilesSelection::Tileset* pTileset,
      Cesium3DTilesSelection::Tile* pTile);
#pragma endregion
  /*static double getBoundingVolumeRadius(
      const Cesium3DTilesSelection::BoundingVolume& boundingVolume);*/
  /**
   * @brief Apply the transform to nodes so that the up-axis of
   * the given model is the Z-axis.
   *
   * By default, the up-axis of a glTF model will the the Y-axis.
   *
   * Depending on whether this value is CesiumGeometry::Axis::X,
   * Y, or Z, the given matrix will be multiplied with a matrix
   * that converts the respective axis to be the Z-axis, as
   * required by the 3D Tiles standard.
   *
   * @param gltf The glTF model
   * @param gltfUpAxis The matrix that will be multiplied with the
   * transform
   */
  static void applyGltfUpTransformToNodes(
      CesiumGltf::Model& gltf,
      const CesiumGeometry::Axis& gltfUpAxis);

private:
};
} // namespace EVScene
