#include "EVScene/TilesetUtils.h"

#include <CesiumGeometry/Axis.h>
#include <CesiumGeometry/Transforms.h>
#include <CesiumGltfContent/GltfUtilities.h>

#include <glm/gtc/quaternion.hpp>

using namespace Cesium3DTilesSelection;

namespace EVScene {

namespace {
static constexpr std::array<double, 16> IDENTITY_MATRIX = {
    1.0,
    0.0,
    0.0,
    0.0,
    0.0,
    1.0,
    0.0,
    0.0,
    0.0,
    0.0,
    1.0,
    0.0,
    0.0,
    0.0,
    0.0,
    1.0};

bool isLeaf(const Cesium3DTilesSelection::Tile& tile) {
  return tile.getChildren().empty();
}
static void applyGltfUpTransformToNode(
    CesiumGltf::Node& node,
    const glm::dmat4& axisTransform) {

  std::vector<double>& matrix = node.matrix;

  if (matrix.size() == 16 &&
      !std::equal(matrix.begin(), matrix.end(), IDENTITY_MATRIX.begin())) {
    glm::dmat4& nodeMatrix = *reinterpret_cast<glm::dmat4*>(matrix.data());
    nodeMatrix = axisTransform * nodeMatrix;

  } else if (node.rotation.size() == 4) {
    // glm::dquat axisRotation = glm::dquat(glm::dmat3(axisTransform));
    // glm::dquat& nodeRotation =
    //    *reinterpret_cast<glm::dquat*>(node.rotation.data());
    // quaternion rotation order
    // TODO: does this work??
    // nodeRotation = axisRotation * nodeRotation;
    glm::dmat4 translation(1.0);
    if (node.translation.size() == 3) {
      translation[3] = glm::dvec4(
          node.translation[0],
          node.translation[1],
          node.translation[2],
          1.0);
    }

    glm::dquat rotationQuat(1.0, 0.0, 0.0, 0.0);
    if (node.rotation.size() == 4) {
      rotationQuat[0] = node.rotation[0];
      rotationQuat[1] = node.rotation[1];
      rotationQuat[2] = node.rotation[2];
      rotationQuat[3] = node.rotation[3];
    }

    glm::dmat4 scale(1.0);
    if (node.scale.size() == 3) {
      scale[0].x = node.scale[0];
      scale[1].y = node.scale[1];
      scale[2].z = node.scale[2];
    }

    glm::dmat4 nodeTransform = translation * glm::dmat4(rotationQuat) * scale;
    glm::dmat4& nodeMatrix = *reinterpret_cast<glm::dmat4*>(matrix.data());
    nodeMatrix = axisTransform * nodeTransform * nodeMatrix;
  } else {
    matrix.resize(16);
    *reinterpret_cast<glm::dmat4*>(matrix.data()) = axisTransform;
  }
}
} // namespace

TilesetUtils::TilesetUtils() {}

TilesetUtils::~TilesetUtils() {}

void TilesetUtils::forEachTile(
    Cesium3DTilesSelection::Tile* pTile,
    const std::function<bool(Tile& tile)>& callback) {
  std::span<Tile> childs = pTile->getChildren();

  for (Tile& child : childs) {
    if (callback(child) || isLeaf(child)) {
      continue;
    }
    forEachTile(&child, callback);
  }
}

void TilesetUtils::forEachTile(
    Cesium3DTilesSelection::Tileset* pTile,
    const std::function<bool(Tile& tile)>& callback) {
  Tile* pRootTile = pTile->getRootTile();
  if (!pRootTile) {
    return;
  }

  forEachTile(pRootTile, callback);
}
void TilesetUtils::updateTileBoundingVolume(
    const Cesium3DTilesSelection::Tileset* pTileset,
    Tile* pTile) {
  //const BoundingVolume& tileBoundingVolume = pTile->getOriginBoundingVolume();
  //const std::optional<CesiumGeospatial::GlobeRectangle>& rectangle =
  //    Cesium3DTilesSelection::estimateGlobeRectangle(tileBoundingVolume);
  //if (!rectangle) {
  //  return;
  //}
  //auto polygons = pTileset->getOptions().contentOptions.editPolygons;
  //std::vector<CesiumGeospatial::CartographicPolygon> usedPolygons;
  //for (auto item : polygons) {
  //  if (Cesium3DTilesSelection::CesiumImpl::outsidePolygons(
  //          tileBoundingVolume,
  //          {item})) {
  //    continue;
  //  }
  //  if (Cesium3DTilesSelection::CesiumImpl::withinPolygons(
  //          tileBoundingVolume,
  //          {item})) {
  //    item.withinPolygon = true;
  //    usedPolygons.push_back(item);
  //    continue;
  //  }
  //  const std::optional<CesiumGeospatial::GlobeRectangle>& boundingRectangle =
  //      item.getBoundingRectangle();
  //  if (boundingRectangle &&
  //      (*rectangle).computeIntersection(*boundingRectangle)) {
  //    usedPolygons.push_back(item);
  //  }
  //}
  //// û�б༭������ཻ��������Ϊԭʼ�İ�Χ��
  //if (usedPolygons.empty()) {
  //  pTile->setBoundingVolume(pTile->getOriginBoundingVolume());
  //}
  //auto resultBounding = CesiumGltfContent::GltfUtilities::updateTileBounding(
  //    pTile->getTransform(),
  //    tileBoundingVolume,
  //    usedPolygons);
  //if (resultBounding) {
  //  pTile->setBoundingVolume(resultBounding.value());
  //}
}

//double TilesetUtils::getBoundingVolumeRadius(
//    const Cesium3DTilesSelection::BoundingVolume& boundingVolume) {
//  struct Operation {
//    double operator()(const CesiumGeometry::OrientedBoundingBox& boundingBox) {
//      glm::dmat3 halfAxes = boundingBox.getHalfAxes();
//      glm::vec3 u(halfAxes[0][0], halfAxes[0][1], halfAxes[0][2]);
//      glm::vec3 v(halfAxes[1][0], halfAxes[1][1], halfAxes[1][2]);
//      glm::vec3 w(halfAxes[2][0], halfAxes[2][1], halfAxes[2][2]);
//      u = u + v;
//      u = u + w;
//      return glm::length(u);
//    }
//
//    double operator()(const CesiumGeospatial::BoundingRegion& boundingRegion) {
//      glm::dmat3 halfAxes = boundingRegion.getBoundingBox().getHalfAxes();
//      glm::vec3 u(halfAxes[0][0], halfAxes[0][1], halfAxes[0][2]);
//      glm::vec3 v(halfAxes[1][0], halfAxes[1][1], halfAxes[1][2]);
//      glm::vec3 w(halfAxes[2][0], halfAxes[2][1], halfAxes[2][2]);
//      u = u + v;
//      u = u + w;
//      return glm::length(u);
//    }
//
//    double operator()(const CesiumGeometry::BoundingSphere& boundingSphere) {
//      return boundingSphere.getRadius();
//    }
//
//    double
//    operator()(const CesiumGeospatial::BoundingRegionWithLooseFittingHeights&
//                   boundingRegion) {
//      glm::dmat3 halfAxes =
//          boundingRegion.getBoundingRegion().getBoundingBox().getHalfAxes();
//      glm::vec3 u(halfAxes[0][0], halfAxes[0][1], halfAxes[0][2]);
//      glm::vec3 v(halfAxes[1][0], halfAxes[1][1], halfAxes[1][2]);
//      glm::vec3 w(halfAxes[2][0], halfAxes[2][1], halfAxes[2][2]);
//      u = u + v;
//      u = u + w;
//      return glm::length(u);
//    }
//    double
//    operator()(const CesiumGeospatial::S2CellBoundingVolume& s2Cell) noexcept {
//      glm::dmat3 halfAxes = getBoundingRegionFromBoundingVolume(s2Cell)
//                                ->getBoundingBox()
//                                .getHalfAxes();
//      glm::vec3 u(halfAxes[0][0], halfAxes[0][1], halfAxes[0][2]);
//      glm::vec3 v(halfAxes[1][0], halfAxes[1][1], halfAxes[1][2]);
//      glm::vec3 w(halfAxes[2][0], halfAxes[2][1], halfAxes[2][2]);
//      u = u + v;
//      u = u + w;
//      return glm::length(u);
//    }
//  };
//
//  return std::visit(Operation{}, boundingVolume);
//}

void TilesetUtils::applyGltfUpTransformToNodes(
    CesiumGltf::Model& gltf,
    const CesiumGeometry::Axis& gltfUpAxis) {

  glm::dmat4 axisTransform(1.0);

  switch (gltfUpAxis) {
  case CesiumGeometry::Axis::X:
    axisTransform = CesiumGeometry::Transforms::X_UP_TO_Z_UP;
    break;
  case CesiumGeometry::Axis::Y:
    axisTransform = CesiumGeometry::Transforms::Y_UP_TO_Z_UP;
    break;
  case CesiumGeometry::Axis::Z:
    break;
    // no transform required for Axis::Z
  }

  // factor in gltf up-axis transform to the top-level nodes in each
  // scene
  for (CesiumGltf::Scene& scene : gltf.scenes) {
    for (const int32_t& nodeId : scene.nodes) {
      if (nodeId < 0 || static_cast<size_t>(nodeId) >= gltf.nodes.size()) {
        continue;
      }

      applyGltfUpTransformToNode(
          gltf.nodes[static_cast<size_t>(nodeId)],
          axisTransform);
    }
  }

  // if there are no scenes, assume the first node is the root
  if (gltf.scenes.empty() && !gltf.nodes.empty()) {
    applyGltfUpTransformToNode(gltf.nodes[0], axisTransform);
  }
}

} // namespace EVScene
