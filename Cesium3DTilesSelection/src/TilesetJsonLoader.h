#pragma once

#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetContentLoaderResult.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetSharedAssetSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumGltf/Schema.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <rapidjson/fwd.h>

#include <memory>
#include <string>
#include <vector>
#pragma region das

#include <CesiumGeospatial/Das/DasCartographicPolygon.h>
#pragma endregion

namespace Cesium3DTilesSelection {
class TilesetJsonLoader : public TilesetContentLoader {
public:
  TilesetJsonLoader(
      const std::string& baseUrl,
      CesiumGeometry::Axis upAxis,
      const CesiumGeospatial::Ellipsoid& ellipsoid CESIUM_DEFAULT_ELLIPSOID);

  CesiumAsync::Future<TileLoadResult>
  loadTileContent(const TileLoadInput& loadInput) override;

  TileChildrenResult createTileChildren(
      const Tile& tile,
      const CesiumGeospatial::Ellipsoid& ellipsoid
          CESIUM_DEFAULT_ELLIPSOID) override;

  const std::string& getBaseUrl() const noexcept;

  CesiumGeometry::Axis getUpAxis() const noexcept;

  const CesiumUtility::IntrusivePointer<CesiumGltf::Schema>&
  getExternalSchema() const noexcept;

  void addChildLoader(std::unique_ptr<TilesetContentLoader> pLoader);

#pragma region das
  /**
   * @brief 设置高度编辑多边形数组
   * @param heightPolygons 高度编辑多边形数组
   */
  void setHeightPolygons(const std::vector<CesiumGeospatial::DasHeightCartographicPolygon>& heightPolygons);

  /**
   * @brief 获取高度编辑多边形数组
   * @return 高度编辑多边形数组的引用
   */
  const std::vector<CesiumGeospatial::DasHeightCartographicPolygon>& getHeightPolygons() const noexcept;

#pragma endregion
  static CesiumAsync::Future<TilesetContentLoaderResult<TilesetJsonLoader>>
  createLoader(
      const TilesetExternals& externals,
      const std::string& tilesetJsonUrl,
      const std::vector<CesiumAsync::IAssetAccessor::THeader>& requestHeaders,
      const CesiumGeospatial::Ellipsoid& ellipsoid CESIUM_DEFAULT_ELLIPSOID);

  static CesiumAsync::Future<TilesetContentLoaderResult<TilesetJsonLoader>>
#pragma region das
  createLoader( const CesiumAsync::AsyncSystem& asyncSystem, const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor, const std::shared_ptr<spdlog::logger>& pLogger, const std::string& tilesetJsonUrl, const CesiumAsync::HttpHeaders& requestHeaders, rapidjson::Document&& tilesetJson, const CesiumGeospatial::Ellipsoid& ellipsoid CESIUM_DEFAULT_ELLIPSOID, const std::vector<CesiumGeospatial::DasHeightCartographicPolygon>* pHeightPolygons = nullptr);
#pragma endregion

  void setExternalSchema(CesiumGltf::Schema* schema) override;
  virtual CesiumUtility::IntrusivePointer<CesiumGltf::Schema>
  getExternalSchema() override;

protected:
  void setOwnerOfNestedLoaders(TilesetContentManager& owner) noexcept override;

private:
  std::string _baseUrl;
  CesiumGeospatial::Ellipsoid _ellipsoid;
  CesiumUtility::IntrusivePointer<TilesetSharedAssetSystem> _pSharedAssetSystem;
  CesiumUtility::IntrusivePointer<CesiumGltf::Schema> _pExternalSchema;

  /**
   * @brief The axis that was declared as the "up-axis" for glTF content.
   *
   * The glTF specification mandates that the Y-axis is the "up"-axis, so the
   * default value is @ref Axis::Y. Older tilesets may contain a string
   * property in the "assets" dictionary, named "gltfUpAxis", indicating a
   * different up-axis. Although the "gltfUpAxis" property is no longer part of
   * the 3D tiles specification, it is still considered for backward
   * compatibility.
   */
  CesiumGeometry::Axis _upAxis;

  std::vector<std::unique_ptr<TilesetContentLoader>> _children;
#pragma region das

  std::vector<CesiumGeospatial::DasHeightCartographicPolygon> _heightPolygons; // 高度编辑多边形数组
#pragma endregion
};
} // namespace Cesium3DTilesSelection
