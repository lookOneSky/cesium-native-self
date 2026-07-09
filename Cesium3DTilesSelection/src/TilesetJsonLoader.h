#pragma once

#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetContentLoaderResult.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetSharedAssetSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>

#include <rapidjson/fwd.h>

#include <memory>
#include <string>
#include <vector>

#pragma region Das
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

  void addChildLoader(std::unique_ptr<TilesetContentLoader> pLoader);

#pragma region Das
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
  createLoader(
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
      const std::shared_ptr<spdlog::logger>& pLogger,
      const std::string& tilesetJsonUrl,
      const CesiumAsync::HttpHeaders& requestHeaders,
      const rapidjson::Document& tilesetJson,
      const CesiumGeospatial::Ellipsoid& ellipsoid CESIUM_DEFAULT_ELLIPSOID
#pragma region Das
      ,const std::vector<CesiumGeospatial::DasHeightCartographicPolygon>* pHeightPolygons = nullptr
#pragma endregion
      );

private:
  std::string _baseUrl;
  CesiumGeospatial::Ellipsoid _ellipsoid;
  CesiumUtility::IntrusivePointer<TilesetSharedAssetSystem> _pSharedAssetSystem;

  /**
   * @brief The axis that was declared as the "up-axis" for glTF content.
   *
   * The glTF specification mandates that the Y-axis is the "up"-axis, so the
   * default value is {@link Axis::Y}. Older tilesets may contain a string
   * property in the "assets" dictionary, named "gltfUpAxis", indicating a
   * different up-axis. Although the "gltfUpAxis" property is no longer part of
   * the 3D tiles specification, it is still considered for backward
   * compatibility.
   */
  CesiumGeometry::Axis _upAxis;

  std::vector<std::unique_ptr<TilesetContentLoader>> _children;

#pragma region Das
  std::vector<CesiumGeospatial::DasHeightCartographicPolygon> _heightPolygons; // 高度编辑多边形数组
#pragma endregion
};
} // namespace Cesium3DTilesSelection
