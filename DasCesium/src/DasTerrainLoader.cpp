#include <DasCesium/DasTerrainLoader.h>

#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/matrix.hpp>

using namespace Cesium3DTilesSelection;
using namespace CesiumGeospatial;
using namespace CesiumGeometry;

namespace DasCesium {

DasTerrainLoader::DasTerrainLoader(
    const Ellipsoid& ellipsoid,
    const DasTerrainOptionsExtra& dasOptions)
    : EllipsoidTilesetLoader(ellipsoid), _dasOptions(dasOptions) {}

/*static*/ std::unique_ptr<Tileset> DasTerrainLoader::createTileset(
    const TilesetExternals& externals,
    const TilesetOptions& options,
    const DasTerrainOptionsExtra& dasOptions) {
  std::unique_ptr<DasTerrainLoader> pCustomLoader =
      std::make_unique<DasTerrainLoader>(options.ellipsoid, dasOptions);
  std::unique_ptr<Tile> pRootTile =
      std::make_unique<Tile>(pCustomLoader.get(), TileEmptyContent{});

  pRootTile->setRefine(TileRefine::Replace);
  pRootTile->setUnconditionallyRefine();

  const QuadtreeTileID rootID(0, 0, 0);
  pRootTile->setTileID(rootID);

  const BoundingRegion& rootBoundingRegion =
      pCustomLoader->createBoundingRegion(rootID);
  const GlobeRectangle& globeRectangle = rootBoundingRegion.getRectangle();

  pRootTile->setTransform(glm::translate(
      glm::dmat4x4(1.0),
      pCustomLoader->_projection.getEllipsoid().cartographicToCartesian(
          globeRectangle.getNorthwest())));
  pRootTile->setBoundingVolume(rootBoundingRegion);

  // Geometric error of the root is just a guess, but it seems to work well
  // enough.
  pRootTile->setGeometricError(1e6);

  return std::make_unique<Tileset>(
      externals,
      std::move(pCustomLoader),
      std::move(pRootTile),
      options);
}

EllipsoidTilesetLoader::Geometry
DasTerrainLoader::createGeometry(const Tile& tile) const {
  static constexpr size_t resolution = 24;

  std::vector<uint16_t> indices;
  indices.reserve(6 * (resolution - 1) * (resolution - 1));

  std::vector<glm::vec3> vertices(resolution * resolution);
  std::vector<glm::vec3> normals(vertices.size());

  const Ellipsoid& ellipsoid = _projection.getEllipsoid();
  const GlobeRectangle& rectangle =
      std::get<BoundingRegion>(tile.getBoundingVolume()).getRectangle();

  double west = rectangle.getWest();
  double east = rectangle.getEast();
  double north = rectangle.getNorth();
  double south = rectangle.getSouth();

  double lonStep = (east - west) / (resolution - 1);
  double latStep = (south - north) / (resolution - 1);

  glm::dmat4 inverseTransform = glm::inverse(tile.getTransform());

  for (uint16_t x = 0; x < resolution; x++) {
    double longitude = (lonStep * x) + west;
    for (uint16_t y = 0; y < resolution; y++) {
      double latitude = (latStep * y) + north;

      // 应用高度偏移
      Cartographic cartographic(
          longitude,
          latitude,
          _dasOptions.dAddHeightMeter);

      uint16_t index = static_cast<uint16_t>((resolution * x) + y);
      vertices[index] = glm::dvec3(
          inverseTransform *
          glm::dvec4(ellipsoid.cartographicToCartesian(cartographic), 1.0));
      normals[index] = ellipsoid.geodeticSurfaceNormal(cartographic);

      if (x < resolution - 1 && y < resolution - 1) {
        uint16_t a = index + 1;
        uint16_t b = index + resolution;
        uint16_t c = b + 1;
        indices.insert(indices.end(), {b, index, a, b, a, c});
      }
    }
  }

  return Geometry{std::move(indices), std::move(vertices), std::move(normals)};
}

CesiumGeospatial::BoundingRegion
DasTerrainLoader::createBoundingRegion(const CesiumGeometry::QuadtreeTileID& quadtreeID) const {
  //实际高程应该先给非常大的包围盒。不然直接不LoadContent了。包围盒使用的是child包围盒。
  return BoundingRegion(
      _projection.unproject(_tilingScheme.tileToRectangle(quadtreeID)),
      _dasOptions.dAddHeightMeter,
      _dasOptions.dAddHeightMeter,
      _projection.getEllipsoid());
}

CesiumAsync::Future<TileLoadResult>
DasTerrainLoader::loadTileContent(const TileLoadInput& input) {
#pragma region Das
  CesiumGltf::Model model = createModel(createGeometry(input.tile));
  // 添加 Tile ID 信息到 extras
  const CesiumGeometry::QuadtreeTileID* pQuadtreeID =
      std::get_if<CesiumGeometry::QuadtreeTileID>(&input.tile.getTileID());
  if (pQuadtreeID) {
    std::string tileUrl = "L" + std::to_string(pQuadtreeID->level) + "X" +
                          std::to_string(pQuadtreeID->x) + "Y" +
                          std::to_string(pQuadtreeID->y);
#pragma endregion
    model.extras["Cesium3DTiles_TileUrl"] = CesiumUtility::JsonValue(tileUrl);
  }

  TileLoadResult result{
      std::move(model),
      Axis::Z,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      input.pAssetAccessor,
      nullptr,
      {},
      TileLoadResultState::Success};

  result.updatedBoundingVolume = input.tile.getBoundingVolume();
  const GlobeRectangle& rectangle =
      std::get<BoundingRegion>(input.tile.getBoundingVolume()).getRectangle();
  result.updatedBoundingVolume = BoundingRegion(rectangle, _dasOptions.dAddHeightMeter, _dasOptions.dAddHeightMeter);
  result.updatedContentBoundingVolume = BoundingRegion(
      rectangle,
      _dasOptions.dAddHeightMeter,
      _dasOptions.dAddHeightMeter);

  return input.asyncSystem.createResolvedFuture(std::move(result));
}

} // namespace DasCesium
