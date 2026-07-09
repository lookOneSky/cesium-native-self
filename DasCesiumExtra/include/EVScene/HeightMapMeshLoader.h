#pragma once
#include "Library.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <CesiumUtility/ErrorList.h>
#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeometry/QuadtreeTileRectangularRange.h>
#include <Cesium3DTilesSelection/TileID.h>
#include <CesiumGltf/Model.h>
#include "EVScene/TerrainLayerInfoLoader.h"
#include <rapidjson/document.h>
#include <span>


#include <cstddef>
#include <memory>
#include <optional>
#include <vector>
namespace CesiumAsync {
class IAssetRequest;
}
namespace EVScene {
struct HeightMapMeshLoadResult {
  /**
   * @brief The glTF model to be rendered for this tile.
   *
   * If this is `std::nullopt`, the tile cannot be rendered.
   * If it has a value but the model is blank, the tile can
   * be "rendered", but it is rendered as nothing.
   */
  std::optional<CesiumGltf::Model> model;

  /**
   * @brief An improved bounding volume for this tile.
   *
   * If this is available, then it is more accurate than the one the tile used
   * originally.
   */
  std::optional<Cesium3DTilesSelection::BoundingVolume> updatedBoundingVolume{};

  /**
   * @brief Available quadtree tiles discovered as a result of loading this
   * tile.
   */
  std::vector<CesiumGeometry::QuadtreeTileRectangularRange>
      availableTileRectangles{};

  /**
   * @brief The request that was used to download the tile content, if any.
   *
   * This field is only populated when there are request-related errors.
   */
  std::shared_ptr<CesiumAsync::IAssetRequest> pRequest;
  CesiumUtility::ErrorList errors;
};
struct HeightMapMeshMetadataResult {
  std::vector<CesiumGeometry::QuadtreeTileRectangularRange> availability;
  CesiumUtility::ErrorList errors;
};
/**
* 解析高度图地形的类
*/
class EVScene_API HeightMapMeshLoader final {
public:
  static std::string CONTENT_TYPE;//注册类型
  /**
   * @brief Create a {@link HeightMapMeshLoadResult} from the given data.
   *
   * @param tileID The tile ID
   * @param tileBoundingVoume The tile bounding volume
   * @param url The URL
   * @param data The actual input data
   * @return The {@link HeightMapMeshLoadResult}
   */
  static HeightMapMeshLoadResult load(
      const Cesium3DTilesSelection::TileID& tileID,
      const Cesium3DTilesSelection::BoundingVolume& tileBoundingVolume,
      const std::string& url,
      const std::span<const std::byte>& data,
      const Cesium3DTilesSelection::TilesetContentOptions& contentOptions,
      const TerrainLayerOptions& layerOptions);

  static std::shared_ptr<std::vector<std::byte>> UpsamplingFromParentData(
      const std::vector<std::byte>* parent_data,
      const CesiumGeometry::QuadtreeTileID& pQuadtreeTileID,
      const CesiumGeometry::QuadtreeTilingScheme& tilingScheme);

  static HeightMapMeshLoadResult upSampleFromParent(
      const CesiumGeometry::QuadtreeTileID& tileID,
      const Cesium3DTilesSelection::BoundingVolume& tileBoundingVolume,
      const std::string& url,
      const std::vector<std::byte>* parent_data,
      const CesiumGeometry::QuadtreeTilingScheme& tilingScheme,
      const Cesium3DTilesSelection::TilesetContentOptions& contentOptions,
      const TerrainLayerOptions& layerOptions);
};
}
