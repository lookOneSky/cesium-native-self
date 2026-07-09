#include "EVScene/HeightMapMeshLoader.h"
#include "Cesium3DTilesSelection/Tile.h"
#include "Cesium3DTilesSelection/Tileset.h"
#include "Cesium3DTilesSelection/spdlog-cesium.h"
#include <CesiumGltfContent/GltfUtilities.h>

#include "CesiumGltfContent/SkirtMeshMetadata.h"
#include "CesiumGeospatial/calcQuadtreeMaxGeometricError.h"
#include <CesiumGltf/AccessorView.h>
//#include <CesiumGltf/ExtensionBoundingRectangle.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumGeometry/QuadtreeTileRectangularRange.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumUtility/JsonHelpers.h>
#include <CesiumUtility/Math.h>
#include <CesiumUtility/Tracing.h>
#include <CesiumUtility/Uri.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include<glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/trigonometric.hpp>
#include <glm/exponential.hpp>
#include <cstddef>
#include <stdexcept>
#include <iostream>

using namespace EVUtility::LayerHeadType;
using namespace Cesium3DTilesSelection;
using namespace CesiumUtility;
using namespace CesiumGeospatial;
using namespace CesiumGeometry;
namespace EVScene {
std::string HeightMapMeshLoader::CONTENT_TYPE =
    "application/HeightMap-terrain-mesh";
struct HeightmapStructure {
  double heightScale = 1;
  double heightOffset = 0;
  uint32_t elementsPerHeight = 1;
  uint32_t elementMultiplier = 1;
  bool isBigEndian = false;
  double lowestEncodedHeight = 0;
  double heighestEncodedHeight = 256 * 256 - 1;
};
struct HeightmapMeshData {
  std::span<const float> heightBuffer;
  uint32_t width;
  uint32_t height;
  // The minimum and maximum heights in the area covered by this tile.
  // The minimum may be lower and the maximum may be higher than
  // the height of any vertex in this tile in the case that the min/max vertex
  // was removed during mesh simplification, but these are the appropriate
  // values to use for analysis or visualization.
  float MinimumHeight;
  float MaximumHeight;
  const HeightmapStructure* structure;
  //std::span<const std::byte> NormalBufferFromHeight;
  uint32_t childTileMask = 15;
  bool onlyWater = false;
  bool onlyLand = true;
  uint32_t triangleCount;
};
static uint32_t index2DTo1D(uint32_t x, uint32_t y, uint32_t width) {
  return y * width + x;
}
static double calculateSkirtHeight(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const CesiumGeospatial::GlobeRectangle& rectangle) noexcept {
  const double levelMaximumGeometricError =
      calcQuadtreeMaxGeometricError(ellipsoid) * rectangle.computeWidth();
  return levelMaximumGeometricError * 5.0;
}

template <class E, class I>
static void addSkirt(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const glm::dvec3& center,
    uint32_t currentVertexCount,
    uint32_t currentIndicesCount,
    double skirtHeight,
    double longitudeOffset,
    double latitudeOffset,
    const std::vector<glm::dvec3>& uvsAndHeights,
    const std::vector<E>& edgeIndices,
    const std::span<float>& positions,
    const std::span<float>& uvsAndHeightsData,
    //const std::span<float>& normals,
    const std::span<I>& indices) {

  size_t newEdgeIndex = currentVertexCount;
  size_t positionIdx = static_cast<size_t>(currentVertexCount * 3.0);
  size_t indexIdx = currentIndicesCount;
  for (size_t i = 0; i < edgeIndices.size(); ++i) {
    E edgeIdx = edgeIndices[i];

    const double longitude = uvsAndHeights[edgeIdx].x + longitudeOffset;
    const double latitude = uvsAndHeights[edgeIdx].y + latitudeOffset;
    const double heightMeters = uvsAndHeights[edgeIdx].z - skirtHeight;
    glm::dvec3 position = ellipsoid.cartographicToCartesian(
        Cartographic(longitude, latitude, heightMeters));
    position -= center;
    uvsAndHeightsData[positionIdx] = uvsAndHeightsData[edgeIdx*3];
    uvsAndHeightsData[positionIdx + 1] = uvsAndHeightsData[edgeIdx * 3+1];
    uvsAndHeightsData[positionIdx + 2] = uvsAndHeightsData[edgeIdx * 3+2];
    positions[positionIdx] = static_cast<float>(position.x);
    positions[positionIdx + 1] = static_cast<float>(position.y);
    positions[positionIdx + 2] = static_cast<float>(position.z);

    /*if (!normals.empty()) {
      const size_t componentIndex = static_cast<size_t>(edgeIdx * 3.0);
      normals[positionIdx] = normals[componentIndex];
      normals[positionIdx + 1] = normals[componentIndex + 1];
      normals[positionIdx + 2] = normals[componentIndex + 2];
    }*/

    if (i < edgeIndices.size() - 1) {
      E nextEdgeIdx = edgeIndices[i + 1];
      indices[indexIdx++] = static_cast<I>(edgeIdx);
      indices[indexIdx++] = static_cast<I>(nextEdgeIdx);
      indices[indexIdx++] = static_cast<I>(newEdgeIndex);

      indices[indexIdx++] = static_cast<I>(newEdgeIndex);
      indices[indexIdx++] = static_cast<I>(nextEdgeIdx);
      indices[indexIdx++] = static_cast<I>(newEdgeIndex + 1);
    }

    ++newEdgeIndex;
    positionIdx += 3;
  }
}

template <class E, class I>
static void addSkirts(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const glm::dvec3& center,
    uint32_t currentVertexCount,
    uint32_t currentIndicesCount,
    double skirtHeight,
    double longitudeOffset,
    double latitudeOffset,
    const std::vector<glm::dvec3>& uvsAndHeights,
    const std::span<float>& outputPositions,
    const std::span<float>& uvsAndHeightsData,
    //const std::span<float>& outputNormals,
    const std::span<I>& outputIndices,
    uint32_t edgeWidth) {
  // const uint32_t edgeVertexCount  = 133;
  /*const uint32_t southVertexCount = 133;
  const uint32_t eastVertexCount  = 133;
  const uint32_t northVertexCount = 133;*/
  const uint32_t width = edgeWidth;

  // add skirt indices, vertices, and normals
  std::vector<uint32_t> westEdgeIndices(width);
  std::vector<uint32_t> eastEdgeIndices(width);
  std::vector<uint32_t> southEdgeIndices(width);
  std::vector<uint32_t> northEdgeIndices(width);

  for (size_t i = 0; i < width; i++) {
    westEdgeIndices[i] = static_cast<uint32_t>((width-1 - i) * width);
    eastEdgeIndices[i] = static_cast<uint32_t>((i + 1) * width - 1);
    southEdgeIndices[i] = static_cast<uint32_t>(i);
    northEdgeIndices[i] = static_cast<uint32_t>(width - 1 - i + (width-1)*width);
  }
  addSkirt(
      ellipsoid,
      center,
      currentVertexCount,
      currentIndicesCount,
      skirtHeight,
      -longitudeOffset,
      0.0,
      uvsAndHeights,
      westEdgeIndices,
      outputPositions,
      uvsAndHeightsData,
      //outputNormals,
      outputIndices);

  currentVertexCount += width;
  currentIndicesCount += (width - 1) * 6;

  addSkirt(
      ellipsoid,
      center,
      currentVertexCount,
      currentIndicesCount,
      skirtHeight,
      0.0,
      -latitudeOffset,
      uvsAndHeights,
      southEdgeIndices,
      outputPositions,
      uvsAndHeightsData,
      //outputNormals,
      outputIndices);
  currentVertexCount += width;
  currentIndicesCount += (width - 1) * 6;

  addSkirt(
      ellipsoid,
      center,
      currentVertexCount,
      currentIndicesCount,
      skirtHeight,
      longitudeOffset,
      0.0,
      uvsAndHeights,
      eastEdgeIndices,
      outputPositions,
      uvsAndHeightsData,
      //outputNormals,
      outputIndices);
  currentVertexCount += width;
  currentIndicesCount += (width - 1) * 6;

  addSkirt(
      ellipsoid,
      center,
      currentVertexCount,
      currentIndicesCount,
      skirtHeight,
      0.0,
      latitudeOffset,
      uvsAndHeights,
      northEdgeIndices,
      outputPositions,
      uvsAndHeightsData,
      //outputNormals,
      outputIndices);
}

static void addRegularGridIndices(
    const uint32_t width,
    const uint32_t height,
    const std::span<uint32_t>& indices,
    size_t offset) {
  uint32_t index = 0;
  for (size_t j = 0; j < height - 1; j++) {
    for (size_t i = 0; i < width - 1; i++) {
      uint32_t upperLeft = index;
      uint32_t lowerLeft = upperLeft + width;
      uint32_t lowerRight = lowerLeft + 1;
      uint32_t upperRight = upperLeft + 1;

      indices[offset++] = upperLeft;
      indices[offset++] = lowerLeft;
      indices[offset++] = upperRight;
      indices[offset++] = upperRight;
      indices[offset++] = lowerLeft;
      indices[offset++] = lowerRight;

      ++index;
    }
    ++index;
  }
}
static double LatitudeRadianToMercator(double lat) {
  double tan = glm::tan(lat);
  double mercatorRadians = log(tan + glm::sqrt(tan * tan + 1));
  return mercatorRadians;
}
static double mercatorToLatitudeRadian(double lat) {
  const static double t = 180.0f / Math::OnePi;
  double degree = glm::atan(glm::sinh(lat));
  degree *= t;
   if (degree > 85)
    degree = 90;
   if (degree < -85)
    degree = -90;
  return degree * Math::OnePi / 180.0f;
}
double newLerp(double north, double south, double index) {
  // 
  double newnorth = LatitudeRadianToMercator(north);
  double newsouth = LatitudeRadianToMercator(south);
  double latitude = Math::lerp(newnorth, newsouth, index);
  return mercatorToLatitudeRadian(latitude);
}

bool isNodataValue(float valuef) {
  return valuef > 9000 || valuef < -9000;
}

/// <summary>
/// 
/// </summary>
/// <param name="data"></param>
/// <param name="pdata">去无效边缘值的高程</param>
/// <param name="pbufferdata">高程转float存储</param>
/// <param name="headinfo"></param>
/// <param name="exaggerationScale"></param>
/// <param name="raiseHeight"></param>
/// <returns></returns>
static std::optional<HeightmapMeshData> parseHeightmapMesh(
    const std::span<const std::byte>& data,
    std::vector<std::byte>& pdata,
    std::vector<std::byte>& pbufferdata,
    const EVScene::DemlayerHeadinfo& headinfo,
    const float& exaggerationScale,
    float raiseHeight) {
  HeightmapMeshData meshView;
  uint32_t heightBufferSize;
  if (headinfo.mnSamplesPerTile==133) {
    meshView.width = headinfo.mnSamplesPerTile - 4;
    meshView.height = headinfo.mnSamplesPerTile - 4;
    heightBufferSize = headinfo.mnSamplesPerTile * headinfo.mnSamplesPerTile;
  } else {
    meshView.width = 65;
    meshView.height = 65;
    heightBufferSize = 65 * 65;
  }
  meshView.triangleCount = (meshView.width - 1) * (meshView.height - 1) * 2;
  meshView.structure = new HeightmapStructure();
  //std::unique_ptr<std::vector<std::byte>> pdata =
      //std::make_unique<std::vector<std::byte>>();
  if (data.size() < 1) {
		//无高程数据的海水
		meshView.MinimumHeight = 0;
		meshView.MaximumHeight = 0;
		meshView.MaximumHeight += raiseHeight;
    return meshView;
  }
  size_t vertexCount = meshView.width * meshView.height;
  //std::vector<std::byte> pdata(sizeof(float) * vertexCount);
  size_t dataWidth, offsetX;
  switch (headinfo.mnDataType) {
  case SSC_LayerDataType_DEM_int16:
    dataWidth = static_cast<size_t>(sqrt((data.size() / 2)));
    offsetX =( dataWidth - meshView.width)/2;
    for (size_t j = 0; j < meshView.width; j++) {
      for (size_t i = 0; i < meshView.width; i++) {
        //size_t heightId = index2DTo1D(i, j, meshView.width);
        size_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        int16_t value = *reinterpret_cast<const int16_t*>(&data[bufferId * 2]);

        float valuef = isNodataValue(value)? 0.0: value*exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;

        const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
        pdata.emplace_back(*pByte);
        pdata.emplace_back(*(pByte+1));
        pdata.emplace_back(*(pByte + 2));
        pdata.emplace_back(*(pByte + 3));

      }
    }
    for (size_t i = 0; i < heightBufferSize; i++) {
      int16_t value = *reinterpret_cast<const int16_t*>(&data[i * 2]);
      float valuef =
        isNodataValue(value) ? 0.0 : value*exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.emplace_back(*pByte);
      pbufferdata.emplace_back(*(pByte + 1));
      pbufferdata.emplace_back(*(pByte + 2));
      pbufferdata.emplace_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_Float:
    dataWidth = static_cast<uint32_t>(sqrt((data.size() / 4)));
    offsetX = (dataWidth - meshView.width) / 2;
    for (uint32_t j = 0; j < meshView.width; j++) {
      for (uint32_t i = 0; i < meshView.width; i++) {
        //uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const float*>(&data[bufferId * 4]);
        float valuef = isNodataValue(value)
                           ? 0.0
                           : value*exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;
        const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
        pdata.emplace_back(*pByte);
        pdata.emplace_back(*(pByte + 1));
        pdata.emplace_back(*(pByte + 2));
        pdata.emplace_back(*(pByte + 3));
      }
    }
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const float*>(&data[i * 4]);
      float valuef =
        isNodataValue(value) ? 0.0 : value*exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.emplace_back(*pByte);
      pbufferdata.emplace_back(*(pByte + 1));
      pbufferdata.emplace_back(*(pByte + 2));
      pbufferdata.emplace_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_int:
    dataWidth = static_cast<uint32_t>(sqrt((data.size() / 4)));
    offsetX = (dataWidth - meshView.width) / 2;
    for (uint32_t j = 0; j < meshView.width; j++) {
      for (uint32_t i = 0; i < meshView.width; i++) {
        //uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const int*>(&data[bufferId * 4]);
        float valuef =
          isNodataValue(value) ? 0.0 : value*exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;

        const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
        pdata.emplace_back(*pByte);
        pdata.emplace_back(*(pByte + 1));
        pdata.emplace_back(*(pByte + 2));
        pdata.emplace_back(*(pByte + 3));
      }
    }
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const int*>(&data[i * 4]);
      float valuef =
        isNodataValue(value) ? 0.0 : value*exaggerationScale; // dem with no data has value of 9000+

      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.emplace_back(*pByte);
      pbufferdata.emplace_back(*(pByte + 1));
      pbufferdata.emplace_back(*(pByte + 2));
      pbufferdata.emplace_back(*(pByte + 3));
    }
    break;
  case SSC_LayerDataType_DEM_int8:
    dataWidth = static_cast<uint32_t>(sqrt((data.size())));
    offsetX = (dataWidth - meshView.width) / 2;
    for (uint32_t j = 0; j < meshView.width; j++) {
      for (uint32_t i = 0; i < meshView.width; i++) {
        //uint32_t heightId = index2DTo1D(i, j, dataWidth - offsetX * 2);
        uint32_t bufferId = index2DTo1D(i + offsetX, j + offsetX, dataWidth);
        float value = *reinterpret_cast<const int8_t*>(&data[bufferId]);
        float valuef =
          isNodataValue(value)
                ? 0.0
                : value *
                      exaggerationScale; // dem with no data has value of 9000+
        valuef += raiseHeight;

        const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
        //pdata[heightId * 4] = *pByte;
        //pdata[heightId * 4 + 1] = *(pByte + 1);
        //pdata[heightId * 4 + 2] = *(pByte + 2);
        //pdata[heightId * 4 + 3] = *(pByte + 3);
        pdata.emplace_back(*pByte);
        pdata.emplace_back(*(pByte + 1));
        pdata.emplace_back(*(pByte + 2));
        pdata.emplace_back(*(pByte + 3));
      }
    }
    for (uint32_t i = 0; i < heightBufferSize; i++) {
      float value = *reinterpret_cast<const int8_t*>(&data[i]);
      float valuef =
        isNodataValue(value)
              ? 0.0
              : value *
                    exaggerationScale; // dem with no data has value of 9000+
      const std::byte* pByte = reinterpret_cast<const std::byte*>(&valuef);
      pbufferdata.emplace_back(*pByte);
      pbufferdata.emplace_back(*(pByte + 1));
      pbufferdata.emplace_back(*(pByte + 2));
      pbufferdata.emplace_back(*(pByte + 3));
    }
    break;
  default:
    meshView.MinimumHeight = 0.0;
    meshView.MaximumHeight = 0.0;
    return meshView;
    break;
  }
  meshView.heightBuffer = std::span<const float>(
      reinterpret_cast<const float*>(pdata.data()),
      vertexCount);
  //const std::span<const int16_t> heightBuffer133 = std::span<const int16_t>(
  //  reinterpret_cast<const int16_t*>(data.data()),
  //    (data.size()) / 2);
  float minHeight = std::numeric_limits<float>::max();
  float maxHeight = std::numeric_limits<float>::min();
  for (size_t i = 0; i < vertexCount; i++) {
    minHeight = std::min(minHeight, meshView.heightBuffer[i]);
    maxHeight = std::max(maxHeight, meshView.heightBuffer[i]);
  }
  meshView.MinimumHeight = minHeight;
  meshView.MaximumHeight = maxHeight;
  return meshView;
}
template <class T>
static std::vector<std::byte> generateNormals(
    const std::span<const float>& positions,
    const std::span<T>& indices,
    size_t currentNumOfIndex) {
  std::vector<std::byte> normalsBuffer(positions.size() * sizeof(float));
  const std::span<float> normals(
      reinterpret_cast<float*>(normalsBuffer.data()),
      positions.size());
  for (size_t i = 0; i < currentNumOfIndex; i += 3) {
    T id0 = indices[i];
    T id1 = indices[i + 1];
    T id2 = indices[i + 2];
    const size_t id0x3 = static_cast<size_t>(id0) * 3;
    const size_t id1x3 = static_cast<size_t>(id1) * 3;
    const size_t id2x3 = static_cast<size_t>(id2) * 3;

    const glm::vec3 p0 =
        glm::vec3(positions[id0x3], positions[id0x3 + 1], positions[id0x3 + 2]);
    const glm::vec3 p1 =
        glm::vec3(positions[id1x3], positions[id1x3 + 1], positions[id1x3 + 2]);
    const glm::vec3 p2 =
        glm::vec3(positions[id2x3], positions[id2x3 + 1], positions[id2x3 + 2]);

    glm::vec3 normal = glm::cross(p1 - p0, p2 - p0);
    normals[id0x3] += normal.x;
    normals[id0x3 + 1] += normal.y;
    normals[id0x3 + 2] += normal.z;

    normals[id1x3] += normal.x;
    normals[id1x3 + 1] += normal.y;
    normals[id1x3 + 2] += normal.z;

    normals[id2x3] += normal.x;
    normals[id2x3 + 1] += normal.y;
    normals[id2x3 + 2] += normal.z;
  }

  for (size_t i = 0; i < normals.size(); i += 3) {
    glm::vec3 normal(normals[i], normals[i + 1], normals[i + 2]);
    if (!Math::equalsEpsilon(glm::dot(normal, normal), 0.0, Math::Epsilon7)) {
      normal = glm::normalize(normal);
      normals[i] = normal.x;
      normals[i + 1] = normal.y;
      normals[i + 2] = normal.z;
    }
  }

  return normalsBuffer;
}

namespace
{
    bool isTileInterRectangle(const BoundingVolume& tileVolume, const CesiumGeospatial::GlobeRectangle& range)
    {
	      bool bResult = false;
	      //const CesiumGeospatial::BoundingRegion* tileRegion = std::get_if<CesiumGeospatial::BoundingRegion>(&tileVolume);
        const BoundingRegion* tileRegion =std::get_if<BoundingRegion>(&tileVolume);
        if (!tileRegion)
        {
            const BoundingRegionWithLooseFittingHeights* pLooseRegion =std::get_if<BoundingRegionWithLooseFittingHeights>(&tileVolume);
            if (pLooseRegion)
            {
                BoundingRegion boundingRegion = BoundingRegion(pLooseRegion->getBoundingRegion().getRectangle(),  0, 0);
                tileRegion = &boundingRegion;
            }
        }
        if (!tileRegion)
        {
	          return bResult;
        }

		    const CesiumGeospatial::GlobeRectangle& tileRectangle = tileRegion->getRectangle();

        double rangeWest = range.getWest();
		    double rangeEast = range.getEast();
		    double rangeSouth = range.getSouth();
		    double rangeNorth = range.getNorth();

        double tileWest = tileRectangle.getWest();
        double tileEast = tileRectangle.getEast();
        double tileSouth = tileRectangle.getSouth();
        double tileNorth = tileRectangle.getNorth();

		    if ((tileWest <= rangeEast && tileEast >= rangeWest) &&(tileSouth <= rangeNorth && tileNorth >= rangeSouth))
        {//在内部
            //double rangeH = std::abs(rangeWest - rangeEast);
            //double rangeV = std::abs(rangeSouth - rangeNorth);
            //double tileH = std::abs(tileWest - tileEast) / 50.0;
            //double tileV = std::abs(tileSouth - tileNorth) / 50.0;
            //if (rangeH > tileH || rangeV > tileV)
            //{//相差不大，50分之一
            //    bResult = true;
            //}
          bResult = true;
		    }
	      return bResult;
    }
}

HeightMapMeshLoadResult HeightMapMeshLoader::load(
    const Cesium3DTilesSelection::TileID& tileID,
    const Cesium3DTilesSelection::BoundingVolume& tileBoundingVolume,
    const std::string& url,
    const std::span<const std::byte>& data,
    const Cesium3DTilesSelection::TilesetContentOptions& contentOptions,
    const TerrainLayerOptions& layerOptions
    ) {
  CESIUM_TRACE("HeightmapMeshContent::load");
  //尝试获取编辑信息中与这个块有关的数据
  const Ellipsoid& ellipsoid = Ellipsoid::CGCS2000;
  CesiumGeospatial::WebMercatorProjection projection ;
  int maxVersion = -1;
  //std::vector<SingleActionInfo> effectActions;
  //Cesium3DTilesSelection::Tileset::editInfoPtr.get()->lockData();
  //if (Cesium3DTilesSelection::Tileset::editInfoPtr.get()->actionInfos.size() != 0
  //  &&!contentOptions.isOcean && contentOptions .enableEdit)
  //{
		//  const BoundingRegion* tileRegion = std::get_if<BoundingRegion>(&tileBoundingVolume);
		//  if (!tileRegion)
		//  {
		//		  const BoundingRegionWithLooseFittingHeights* pLooseRegion = std::get_if<BoundingRegionWithLooseFittingHeights>(&tileBoundingVolume);
		//		  if (pLooseRegion)
		//		  {
		//			    BoundingRegion boundingRegion = BoundingRegion(pLooseRegion->getBoundingRegion().getRectangle(), 0, 0);
		//			    tileRegion = &boundingRegion;
		//		  }
		//  }
		//  const CesiumGeospatial::GlobeRectangle& tileRectangle = tileRegion->getRectangle();
		//  double tileWest = tileRectangle.getWest() * 180.0 / 3.14159265358979353;
		//  double tileEast = tileRectangle.getEast() * 180.0 / 3.14159265358979353;
		//  double tileSouth = tileRectangle.getSouth() * 180.0 / 3.14159265358979353;
		//  double tileNorth = tileRectangle.getNorth() * 180.0 / 3.14159265358979353;
  //    glm::dvec3 Northeast = CesiumGeospatial::Ellipsoid::CGCS2000.cartographicToCartesian(CesiumGeospatial::Cartographic::fromDegrees(tileEast, tileNorth, 0));
  //    glm::dvec3 Southwest = CesiumGeospatial::Ellipsoid::CGCS2000.cartographicToCartesian(CesiumGeospatial::Cartographic::fromDegrees(tileWest, tileSouth, 0));
  //    float tileLength = glm::distance(Northeast, Southwest);

  //    std::weak_ptr<TerrainEditInfo> wptr = Cesium3DTilesSelection::Tileset::editInfoPtr;
  //    if(auto tei_locked = wptr.lock())
  //    {
  //        if(tei_locked.get()->actionInfos.size() != 0 && isTileInterRectangle(tileBoundingVolume, tei_locked.get()->rectangle))
  //        {//与总的编辑的包围盒相交
  //            for (std::weak_ptr<SingleActionInfo> actionWptr : tei_locked.get()->actionInfos)
  //            {
  //                if(auto actionWptr_locked = actionWptr.lock())
  //                {
  //                    if(isTileInterRectangle(tileBoundingVolume, actionWptr_locked.get()->singleActionRectangle))
  //                    {//与动作的包围盒相交
  //                        if ((tileLength / 500.0) > actionWptr_locked.get()->radius)
  //                        {
  //                            continue;
  //                        }
  //                        SingleActionInfo actionInfo;
  //                        actionInfo.height = actionWptr_locked.get()->height;
  //                        actionInfo.radius = actionWptr_locked.get()->radius;
  //                        actionInfo.isRelativeHeight = actionWptr_locked.get()->isRelativeHeight;
  //                        actionInfo.steepness = actionWptr_locked.get()->steepness;
  //                        for (size_t i = 0; i < actionWptr_locked.get()->getSize(); i++)
  //                        {
  //                            if(isTileInterRectangle(tileBoundingVolume, actionWptr_locked.get()->getPointRectangle(i)))
  //                            {//与动作内的具体的点相交
  //                                glm::dvec3 point = actionWptr_locked.get()->getPoint(i);
  //                                //point = CesiumGeospatial::projectPosition (projection,  CesiumGeospatial::Cartographic(CesiumGeospatial::Cartographic::fromDegrees(point.x, point.y, 0)));
  //                                point = CesiumGeospatial::Ellipsoid::CGCS2000.cartographicToCartesian(CesiumGeospatial::Cartographic::fromDegrees(point.x, point.y, 0));
  //                                actionInfo.addPoint(point,false);
  //                                maxVersion = actionWptr_locked.get()->version > maxVersion  ?  actionWptr_locked.get()->version : maxVersion;
  //                            }
  //                        }
  //                        if(actionInfo.getSize() != 0)
  //                        {
  //                            effectActions.emplace_back(actionInfo);
  //                        }
  //                    }
  //                }
  //            }
  //        }
  //    }
  //}
  //Cesium3DTilesSelection::Tileset::editInfoPtr.get()->unlockData();

  CESIUM_TRACE("HeightmapMeshContent::parseHeightmapMesh");

  const QuadtreeTileID& id = std::get<QuadtreeTileID>(tileID);
  uint32_t level= id.level;
  if (id == QuadtreeTileID(21, 1704427, 1236811) ||
      id == QuadtreeTileID(20, 852213, 618406)) {
    int ppq = 0;
  }
  HeightMapMeshLoadResult result;
  if (layerOptions.headInfo.mnDataType == SSC_LayerDataType_Unknown) {
    result.errors.emplaceError("mnDataType is unKnown ");
    return result;
  }
  std::vector<std::byte> pdata;
  std::vector<std::byte> heightBufferdata;

  const std::optional<HeightmapMeshData> meshView = parseHeightmapMesh(
      data,
      pdata,
      heightBufferdata,
      layerOptions.headInfo,
      layerOptions.exaggerationScale,
      layerOptions.raiseHeight);
  CESIUM_TRACE("HeightmapMeshContent::rectangle");

  if (!meshView) {
    result.errors.emplaceError(
        "Unable to parse heightmap-mesh-1.0 tile.");
    return result;
  }
  const BoundingRegion* pRegion =
      std::get_if<BoundingRegion>(&tileBoundingVolume);
  if (!pRegion) {
    const BoundingRegionWithLooseFittingHeights* pLooseRegion =
        std::get_if<BoundingRegionWithLooseFittingHeights>(&tileBoundingVolume);
    if (pLooseRegion) {
      BoundingRegion boundingRegion = BoundingRegion(
          pLooseRegion->getBoundingRegion().getRectangle(),
          meshView.value().MinimumHeight,
          meshView.value().MaximumHeight);
      pRegion = &boundingRegion;
    }
  }
  if (!pRegion) {
    result.errors.emplaceError(
        "Unable to create quantized-mesh-1.0 tile because the tile's bounding "
        "volume is not a bounding region.");
    return result;
  }
  const CesiumGeospatial::GlobeRectangle& rectangle = pRegion->getRectangle();
  CesiumGeospatial::GlobeRectangle newBoundingRectangle(rectangle);
  const double skirtHeight = calculateSkirtHeight(ellipsoid, rectangle) +
                             meshView.value().MaximumHeight;

  const uint32_t vertexCount = meshView->width * meshView->height;
  const uint32_t skirtVertexCount = meshView->width * 4;
  // decode position without skirt, but preallocate position buffer to include
  // skirt as well
  std::vector<std::byte> outputPositionsBuffer(
      (vertexCount + skirtVertexCount) * 3 * sizeof(float));
  std::span<float> outputPositions(
      reinterpret_cast<float*>(outputPositionsBuffer.data()),
      (vertexCount + skirtVertexCount) * 3);
  size_t positionOutputIndex = 0;
  size_t lonlatOutputIndex = 0;

  double minX = std::numeric_limits<double>::max();
  double minY = std::numeric_limits<double>::max();
  double minZ = std::numeric_limits<double>::max();
  double maxX = std::numeric_limits<double>::lowest();
  double maxY = std::numeric_limits<double>::lowest();
  double maxZ = std::numeric_limits<double>::lowest();

  // const glm::dvec3 center      =
  // ellipsoid.cartographicToCartesian(rectangle.computeCenter());
  const glm::dvec3 center = pRegion->getBoundingBox().getCenter();

  double minimumHeight = meshView->MinimumHeight;
  double maximumHeight = meshView->MaximumHeight;
  const double west = rectangle.getWest();
  const double south = rectangle.getSouth();
  const double east = rectangle.getEast();
  const double north = rectangle.getNorth();

  //南北极处理
  double newnorth =
     std::abs(north - CesiumGeospatial::WebMercatorProjection::MAXIMUM_LATITUDE )<0.00000001
          ? CesiumUtility::Math::OnePi / 2
          : north;
  double newsouth =
      std::abs(
          south + CesiumGeospatial::WebMercatorProjection::MAXIMUM_LATITUDE) <
              0.00000001
          ? -CesiumUtility::Math::OnePi / 2
          : south;
  newBoundingRectangle.setNorth(newnorth);
  newBoundingRectangle.setSouth(newsouth);

  const double longitudeOffset =  (east - west) * 0.0001;
  const double latitudeOffset = (newnorth - newsouth) * 0.0001;
  std::vector<glm::dvec3> uvsAndHeights;
  uvsAndHeights.reserve(vertexCount);
  std::vector<std::byte> outputUvsAndHeightsBuffer(
      (vertexCount + skirtVertexCount) * 3 * sizeof(float));
  std::span<float> uvsAndHeightsData(
      reinterpret_cast<float*>(outputUvsAndHeightsBuffer.data()),
      (vertexCount + skirtVertexCount) * 3);

  double width = static_cast<double>(meshView->width - 1);
  double height2 = static_cast<double>(meshView->height - 1);

  struct  PointChangeInfo
  {
      float weight = 0;
      PointChangeInfo(){};
  };
  std::vector<PointChangeInfo> changeInfos(vertexCount);
  CESIUM_TRACE("HeightmapMeshContent::vertexCount");

  for (size_t i = 0; i < vertexCount; ++i) {
    int32_t u = i % meshView->width;
    int32_t v = static_cast<int32_t>(floor(i / meshView->height));
    if (u == 0 && (v == 128 || v == 0)) {
      int ppa = 0;
    }
    double uRatio = static_cast<double>(u) / width;
    double vRatio = static_cast<double>(v) / height2;

    double longitude = Math::lerp(west, east, uRatio);
    double latitude = Math::lerp(newnorth, newsouth, vRatio);
    //double latitude = newLerp(north,south, vRatio);
    

    double heightMeters = meshView->heightBuffer.empty() ? 0.0 : static_cast<double>(meshView->heightBuffer[i]);
    if (meshView->heightBuffer.empty())
    {
      heightMeters += layerOptions.raiseHeight;
    }

     //响应地形编辑--------------------------BEGIN------------------------------------------------------------------------------------------------------------
    //glm::dvec3 vertexPosition = CesiumGeospatial::projectPosition (projection,  CesiumGeospatial::Cartographic(Cartographic(longitude, latitude, 0)));
    //if (!contentOptions.isOcean)
    {//不是海水，才响应
			glm::dvec3 vertexPosition = CesiumGeospatial::Ellipsoid::CGCS2000.cartographicToCartesian(Cartographic(longitude, latitude, 0));
			//for (size_t j = 0; j < effectActions.size(); j++)
			//{
			//	float editRadius = effectActions[j].radius;
			//	float editHeight = effectActions[j].height;
			//	float steepness = effectActions[j].steepness;
			//	bool isRelativeHeight = effectActions[j].isRelativeHeight;
			//	float MinDistance = editRadius;
			//	for (size_t k = 0; k < effectActions[j].getSize(); k++)
			//	{//一个动作内的循环
			//		glm::dvec3 actionPoint = effectActions[j].getPoint(k);
			//		float distance = glm::distance(actionPoint, vertexPosition);
			//		if (distance <= MinDistance)
			//		{
			//			MinDistance = distance;
			//			{//带弧度的
			//				float weight = 1.0 - distance / editRadius;
			//				weight = weight * (steepness + weight);
			//				weight = weight > 1.0 ? 1.0 : weight;
			//				if (changeInfos[i].weight < weight)
			//				{
			//					//先找到影响比重最大的
			//					changeInfos[i].weight = weight;
			//					if (weight >= 1.0)
			//					{
			//						break;
			//					}
			//				}
			//			}
			//		}
			//	}
			//	if (changeInfos[i].weight != 0)
			//	{
			//		float targetHeight = editHeight;
			//		if (!isRelativeHeight)
			//		{
			//			targetHeight = editHeight - heightMeters;
			//		}
			//		heightMeters += (targetHeight * glm::abs(glm::sin((changeInfos[i].weight - 0.5) * CesiumUtility::Math::OnePi) + 1.0) * 0.5);
			//		//一个动作循环完之后，重置该点info的weight内容
			//		changeInfos[i].weight = 0;
			//	}

			//}
			//if (!effectActions.empty())
			//{
			//	minimumHeight = minimumHeight > heightMeters ? heightMeters : minimumHeight;
			//	maximumHeight = maximumHeight < heightMeters ? heightMeters : maximumHeight;
			//}
    }
    //响应地形编辑--------------------------END------------------------------------------------------------------------------------------------------------


    glm::dvec3 position = ellipsoid.cartographicToCartesian(Cartographic(longitude, latitude, heightMeters));
   /* if (!posvit) {
      posvit = position;
    }*/
    position -= center;
    uvsAndHeightsData[positionOutputIndex] = static_cast<float>(CesiumUtility::Math::radiansToDegrees(longitude));
    uvsAndHeightsData[positionOutputIndex + 1] = static_cast<float>(CesiumUtility::Math::radiansToDegrees(latitude));
    uvsAndHeightsData[positionOutputIndex + 2] =
        static_cast<float>(heightMeters);
    outputPositions[positionOutputIndex++] = static_cast<float>(position.x);
    outputPositions[positionOutputIndex++] = static_cast<float>(position.y);
    outputPositions[positionOutputIndex++] = static_cast<float>(position.z);
   
    minX = glm::min(minX, position.x);
    minY = glm::min(minY, position.y);
    minZ = glm::min(minZ, position.z);

    maxX = glm::max(maxX, position.x);
    maxY = glm::max(maxY, position.y);
    maxZ = glm::max(maxZ, position.z);

    uvsAndHeights.emplace_back(longitude, latitude, heightMeters);
    
  }
  CESIUM_TRACE("HeightmapMeshContent::addRegularGridIndices");

  // decode normal vertices of the tile as well as its metadata without skirt
  std::vector<std::byte> outputNormalsBuffer;
  std::span<float> outputNormals;

  /*if (!meshView->octEncodedNormalBuffer.empty())
  {
    const uint32_t totalNormalFloats = (vertexCount + skirtVertexCount) * 3;
    outputNormalsBuffer.resize(totalNormalFloats * sizeof(float));
    outputNormals =
  std::span<float>(reinterpret_cast<float*>(outputNormalsBuffer.data()),
  totalNormalFloats); decodeNormals(meshView->octEncodedNormalBuffer,
  outputNormals); outputNormals =
  std::span<float>(reinterpret_cast<float*>(outputNormalsBuffer.data()),
  outputNormalsBuffer.size() / sizeof(float));
  }*/
  // indices buffer for gltf to include tile and skirt indices. Caution of
  // indices type since adding skirt means the number of vertices is potentially
  // over maximum of uint16_t
  std::vector<std::byte> outputIndicesBuffer;
  uint32_t indexSizeBytes = sizeof(uint32_t);
  uint32_t indicesCount = meshView->triangleCount * 3;
  uint32_t skirtTriangleCountCount = (skirtVertexCount - 4) * 2;

  uint32_t skirtIndicesCount = skirtTriangleCountCount * 3;

  // decode the tile indices without skirt.
  const size_t outputIndicesCount = indicesCount + skirtIndicesCount;
  outputIndicesBuffer.resize(outputIndicesCount * sizeof(uint32_t));
  // const std::span<const uint32_t> indices(reinterpret_cast<const
  // uint32_t*>(meshView->indicesBuffer.data()), indicesCount);
  std::span<uint32_t> outputIndices(
      reinterpret_cast<uint32_t*>(outputIndicesBuffer.data()),
      outputIndicesCount);
  // decodeIndices(indices, outputIndices);
  addRegularGridIndices(meshView->width, meshView->height, outputIndices, 0);

  

  // add skirt
  //if (contentOptions.createSkirt)
  //{
  //  addSkirts<uint32_t, uint32_t>(
  //    ellipsoid,
  //      center,
  //    vertexCount,
  //    indicesCount,
  //    skirtHeight,
  //    longitudeOffset,
  //    latitudeOffset,
  //    uvsAndHeights,
  //    outputPositions,
  //    uvsAndHeightsData,
  //    //outputNormals,
  //    outputIndices, meshView->width);
  //}
  // generate normals if no provided
  if (outputNormalsBuffer.empty()) {
    outputNormalsBuffer =
        generateNormals(outputPositions, outputIndices, indicesCount);
    outputNormals = std::span<float>(
        reinterpret_cast<float*>(outputNormalsBuffer.data()),
        outputNormalsBuffer.size() / sizeof(float));
  }
  indexSizeBytes = sizeof(uint32_t);

  CESIUM_TRACE("HeightmapMeshContent::creategltf");

  // create gltf
  result.model.emplace();
  CesiumGltf::Model& model = result.model.value();
  //modelRectangle = rectangle;
  CesiumGltf::Material& material = model.materials.emplace_back();
  CesiumGltf::MaterialPBRMetallicRoughness& pbr =
      material.pbrMetallicRoughness.emplace();
  pbr.metallicFactor = 0.0;
  pbr.roughnessFactor = 1.0;

  const size_t meshId = model.meshes.size();
  model.meshes.emplace_back();
  CesiumGltf::Mesh& mesh = model.meshes[meshId];
  mesh.primitives.emplace_back();

  CesiumGltf::MeshPrimitive& primitive = mesh.primitives[0];
  primitive.mode = CesiumGltf::MeshPrimitive::Mode::TRIANGLES;
  primitive.material = 0;

  // add position buffer to gltf
  const size_t positionBufferId = model.buffers.size();
  model.buffers.emplace_back();
  CesiumGltf::Buffer& positionBuffer = model.buffers[positionBufferId];
  positionBuffer.cesium.data = std::move(outputPositionsBuffer);

  const size_t positionBufferViewId = model.bufferViews.size();
  model.bufferViews.emplace_back();
  CesiumGltf::BufferView& positionBufferView =
      model.bufferViews[positionBufferViewId];
  positionBufferView.buffer = int32_t(positionBufferId);
  positionBufferView.byteOffset = 0;
  positionBufferView.byteStride = 3 * sizeof(float);
  positionBufferView.byteLength = int64_t(positionBuffer.cesium.data.size());
  positionBufferView.target = CesiumGltf::BufferView::Target::ARRAY_BUFFER;

  const size_t positionAccessorId = model.accessors.size();
  model.accessors.emplace_back();
  CesiumGltf::Accessor& positionAccessor = model.accessors[positionAccessorId];
  positionAccessor.bufferView = static_cast<int>(positionBufferViewId);
  positionAccessor.byteOffset = 0;
  positionAccessor.componentType = CesiumGltf::Accessor::ComponentType::FLOAT;
  positionAccessor.count = vertexCount + skirtVertexCount;
  positionAccessor.type = CesiumGltf::Accessor::Type::VEC3;
  positionAccessor.min = {minX, minY, minZ};
  positionAccessor.max = {maxX, maxY, maxZ};

  primitive.attributes.emplace("POSITION", int32_t(positionAccessorId));

  // add normal buffer to gltf if there are any
  if (!outputNormalsBuffer.empty()) {
    const size_t normalBufferId = model.buffers.size();
    model.buffers.emplace_back();
    CesiumGltf::Buffer& normalBuffer = model.buffers[normalBufferId];
    normalBuffer.cesium.data = std::move(outputNormalsBuffer);

    const size_t normalBufferViewId = model.bufferViews.size();
    model.bufferViews.emplace_back();
    CesiumGltf::BufferView& normalBufferView =
        model.bufferViews[normalBufferViewId];
    normalBufferView.buffer = int32_t(normalBufferId);
    normalBufferView.byteOffset = 0;
    normalBufferView.byteStride = 3 * sizeof(float);
    normalBufferView.byteLength = int64_t(normalBuffer.cesium.data.size());
    normalBufferView.target = CesiumGltf::BufferView::Target::ARRAY_BUFFER;

    const size_t normalAccessorId = model.accessors.size();
    model.accessors.emplace_back();
    CesiumGltf::Accessor& normalAccessor = model.accessors[normalAccessorId];
    normalAccessor.bufferView = int32_t(normalBufferViewId);
    normalAccessor.byteOffset = 0;
    normalAccessor.componentType = CesiumGltf::Accessor::ComponentType::FLOAT;
    normalAccessor.count = vertexCount + skirtVertexCount;
    normalAccessor.type = CesiumGltf::Accessor::Type::VEC3;

    primitive.attributes.emplace("NORMAL", static_cast<int>(normalAccessorId));
  }
  //传入地形经纬高
  if (1) {
    const size_t uvsAndHeightsBufferId = model.buffers.size();
    model.buffers.emplace_back();
    CesiumGltf::Buffer& uvsAndHeightsBuffer =
        model.buffers[uvsAndHeightsBufferId];
    uvsAndHeightsBuffer.cesium.data = std::move(outputUvsAndHeightsBuffer);

    const size_t uvsAndHeightsBufferViewId = model.bufferViews.size();
    model.bufferViews.emplace_back();
    CesiumGltf::BufferView& uvsAndHeightsBufferView =
        model.bufferViews[uvsAndHeightsBufferViewId];
    uvsAndHeightsBufferView.buffer = int32_t(uvsAndHeightsBufferViewId);
    uvsAndHeightsBufferView.byteOffset = 0;
    uvsAndHeightsBufferView.byteStride = 3 * sizeof(float);
    uvsAndHeightsBufferView.byteLength =
        int64_t(uvsAndHeightsBuffer.cesium.data.size());
    uvsAndHeightsBufferView.target =
        CesiumGltf::BufferView::Target::ARRAY_BUFFER;

    const size_t uvsAndHeightsAccessorId = model.accessors.size();
    model.accessors.emplace_back();
    CesiumGltf::Accessor& uvsAndHeightsAccessor =
        model.accessors[uvsAndHeightsAccessorId];
    uvsAndHeightsAccessor.bufferView = int32_t(uvsAndHeightsAccessorId);
    uvsAndHeightsAccessor.byteOffset = 0;
    uvsAndHeightsAccessor.componentType =
        CesiumGltf::Accessor::ComponentType::FLOAT;
    uvsAndHeightsAccessor.count = vertexCount + skirtVertexCount;
    uvsAndHeightsAccessor.type = CesiumGltf::Accessor::Type::VEC3;

    primitive.attributes.emplace(
        "UVSANDHEIGHTS",
        static_cast<int>(uvsAndHeightsAccessorId));
  }
  
  //传入高度buffer
  if (heightBufferdata.size() > 0) {
    
    const size_t HeightsBufferId = model.buffers.size();
    model.buffers.emplace_back();
    CesiumGltf::Buffer& HeightsBuffer = model.buffers[HeightsBufferId];
    HeightsBuffer.cesium.data = std::move(heightBufferdata);

    const size_t HeightsBufferViewId = model.bufferViews.size();
    model.bufferViews.emplace_back();
    CesiumGltf::BufferView& HeightsBufferView =
        model.bufferViews[HeightsBufferViewId];
    HeightsBufferView.buffer = int32_t(HeightsBufferViewId);
    HeightsBufferView.byteOffset = 0;
    HeightsBufferView.byteStride =  sizeof(float);
    HeightsBufferView.byteLength =
        int64_t(HeightsBuffer.cesium.data.size());
    HeightsBufferView.target =
        CesiumGltf::BufferView::Target::ARRAY_BUFFER;

    const size_t HeightsAccessorId = model.accessors.size();
    model.accessors.emplace_back();
    CesiumGltf::Accessor& HeightsAccessor =
        model.accessors[HeightsAccessorId];
    HeightsAccessor.bufferView = int32_t(HeightsAccessorId);
    HeightsAccessor.byteOffset = 0;
    HeightsAccessor.componentType =
        CesiumGltf::Accessor::ComponentType::FLOAT;
    HeightsAccessor.count = vertexCount;
    HeightsAccessor.type = CesiumGltf::Accessor::Type::SCALAR;

    primitive.attributes.emplace(
        "HEIGHTS",
        static_cast<int>(HeightsAccessorId));
  }
  // decode normal vertices of the tile as well as its metadata without skirt
  /* std::vector<std::byte> outputNormalsBuffer;
   std::span<float> outputNormals;*/
  // add indices buffer to gltf
  const size_t indicesBufferId = model.buffers.size();
  model.buffers.emplace_back();
  CesiumGltf::Buffer& indicesBuffer = model.buffers[indicesBufferId];
  indicesBuffer.cesium.data = std::move(outputIndicesBuffer);

  const size_t indicesBufferViewId = model.bufferViews.size();
  model.bufferViews.emplace_back();
  CesiumGltf::BufferView& indicesBufferView =
      model.bufferViews[indicesBufferViewId];
  indicesBufferView.buffer = int32_t(indicesBufferId);
  indicesBufferView.byteOffset = 0;
  indicesBufferView.byteLength = int64_t(indicesBuffer.cesium.data.size());
  indicesBufferView.byteStride = indexSizeBytes;
  indicesBufferView.target = CesiumGltf::BufferView::Target::ARRAY_BUFFER;

  const size_t indicesAccessorId = model.accessors.size();
  model.accessors.emplace_back();
  CesiumGltf::Accessor& indicesAccessor = model.accessors[indicesAccessorId];
  indicesAccessor.bufferView = int32_t(indicesBufferViewId);
  indicesAccessor.byteOffset = 0;
  indicesAccessor.type = CesiumGltf::Accessor::Type::SCALAR;
  indicesAccessor.count = indicesCount + skirtIndicesCount;
  indicesAccessor.componentType =
      indexSizeBytes == sizeof(uint32_t)
          ? CesiumGltf::Accessor::ComponentType::UNSIGNED_INT
          : CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT;

  primitive.indices = int32_t(indicesBufferId);

  // add skirts info to primitive extra in case we need to upsample from it
 
  //if (contentOptions.createSkirt) {
  //  CesiumGltfContent::SkirtMeshMetadata skirtMeshMetadata;
  //  skirtMeshMetadata.noSkirtIndicesBegin = 0;
  //  skirtMeshMetadata.noSkirtIndicesCount = indicesCount;
  //  skirtMeshMetadata.noSkirtVerticesBegin = 0;
  //  skirtMeshMetadata.noSkirtVerticesCount = vertexCount;
  //  skirtMeshMetadata.meshCenter = center;
  //  skirtMeshMetadata.skirtWestHeight = skirtHeight;
  //  skirtMeshMetadata.skirtSouthHeight = skirtHeight;
  //  skirtMeshMetadata.skirtEastHeight = skirtHeight;
  //  skirtMeshMetadata.skirtNorthHeight = skirtHeight;
  //  primitive.extras = CesiumGltfContent::SkirtMeshMetadata::createGltfExtras(skirtMeshMetadata);
  //}

  // add only-water and only-land flags to primitive extras
  primitive.extras.emplace("OnlyWater", meshView->onlyWater);
  primitive.extras.emplace("OnlyLand", meshView->onlyLand);

  // TODO: use KHR_texture_transform
  primitive.extras.emplace("WaterMaskTranslationX", 0.0);
  primitive.extras.emplace("WaterMaskTranslationY", 0.0);
  primitive.extras.emplace("WaterMaskScale", 1.0);

  // if there is a combination of water and land, add the full water mask
  if (!meshView->onlyWater && !meshView->onlyLand) {
    // create source image
    const size_t waterMaskImageId = model.images.size();
    model.images.emplace_back();
    CesiumGltf::Image& waterMaskImage = model.images[waterMaskImageId];
    waterMaskImage.pAsset->width = 256;
    waterMaskImage.pAsset->height = 256;
    waterMaskImage.pAsset->channels = 1;
    waterMaskImage.pAsset->bytesPerChannel = 1;
    waterMaskImage.pAsset->pixelData.resize(65536);

    std::memcpy(
        waterMaskImage.pAsset->pixelData.data(),
        meshView->heightBuffer.data(),
        65536);

    // create sampler parameters
    const size_t waterMaskSamplerId = model.samplers.size();
    model.samplers.emplace_back();
    CesiumGltf::Sampler& waterMaskSampler = model.samplers[waterMaskSamplerId];
    waterMaskSampler.magFilter = CesiumGltf::Sampler::MagFilter::LINEAR;
    waterMaskSampler.minFilter =
        CesiumGltf::Sampler::MinFilter::LINEAR_MIPMAP_NEAREST;
    waterMaskSampler.wrapS = CesiumGltf::Sampler::WrapS::CLAMP_TO_EDGE;
    waterMaskSampler.wrapT = CesiumGltf::Sampler::WrapT::CLAMP_TO_EDGE;

    // create texture
    const size_t waterMaskTextureId = model.textures.size();
    model.textures.emplace_back();
    CesiumGltf::Texture& waterMaskTexture = model.textures[waterMaskTextureId];
    waterMaskTexture.sampler = int32_t(waterMaskSamplerId);
    waterMaskTexture.source = int32_t(waterMaskImageId);

    // store the texture id in the extras
    primitive.extras.emplace("WaterMaskTex", int32_t(waterMaskTextureId));
  } else {
    primitive.extras.emplace("WaterMaskTex", int32_t(-1));
  }

  // create node and update bounding volume
  model.nodes.emplace_back();
  CesiumGltf::Node& node = model.nodes[0];
  node.mesh = 0;
  node.matrix = {
      1.0,
      0.0,
      0.0,
      0.0,
      0.0,
      0.0,
      -1.0,
      0.0,
      0.0,
      1.0,
      0.0,
      0.0,
      center.x,
      center.z,
      -center.y,
      1.0};

  result.updatedBoundingVolume =
      BoundingRegion(newBoundingRectangle, minimumHeight, maximumHeight);
  //GltfUtilities::applyGltfUpTransformToNodes(
  //    result.model.value(),
  //    Axis::Y);

  if (result.model) {
    std::string urlname = "L=";
    urlname += std::to_string(id.level);
    urlname += "x";
    urlname += std::to_string(id.x);
    urlname += "y";

    urlname +=std::to_string(id.y);

    result.model.value().extras["Cesium3DTiles_TileUrl"] = urlname;
  }
  result.model.value().extras["Cesium3DTiles_TileType"] = "Terrain";
  result.model.value().extras["Terrain_EditVer"] = std::to_string(maxVersion);
  
  return result;
}

static float mid(const std::vector<std::byte>& from, int index, int stride) {
  const float* left = reinterpret_cast<const float*>(&from[index]);
  const float* right = reinterpret_cast<const float*>(&from[index + stride]);
  float mid = glm::mix(*left, *right, 0.5f);
  return mid;
}
static float interpolate(
    float u_left,
    float d_left,
    float u_right,
    float d_right,
    float row_lambda,
    float col_lambda) {
  float up = glm::mix(u_right, u_left, row_lambda);
  /*u_right * (1.0 - row_lambda) + u_left*row_lambda;*/
  float down = glm::mix(d_right, d_left, row_lambda);
  /*d_right*(1.0 - row_lambda) + d_left* row_lambda; */
  float mid_f = glm::mix(down, up, col_lambda);
  // up*(1.0f - col_lambda) + down* col_lambda;
  return mid_f;
}
static float value(const std::vector<std::byte>& from, int index) {
  return *reinterpret_cast<const float*>(&from[index * 4]); // 4 bytes per float
}
static bool outrange(int x) { return x < 1 || x > 62; }
static float
value(const std::vector<std::byte>& from, int row, int column, int size) {
  return *reinterpret_cast<const float*>(&from[(row * size + column) * 4]);
}
static void add(std::vector<std::byte>& to, int index, float value) {
  std::byte* pByte = reinterpret_cast<std::byte*>(&value);
  to[index * 4] = *pByte;
  to[index * 4 + 1] = *(pByte + 1);
  to[index * 4 + 2] = *(pByte + 2);
  to[index * 4 + 3] = *(pByte + 3);
}
std::shared_ptr<std::vector<std::byte>>
HeightMapMeshLoader::UpsamplingFromParentData(
    const std::vector<std::byte>* parent_data,
    const CesiumGeometry::QuadtreeTileID& pQuadtreeTileID,
    const CesiumGeometry::QuadtreeTilingScheme& tilingScheme) {
  // return std::make_shared<std::vector<std::byte>>();
  
  std::shared_ptr<std::vector<std::byte>> data =
      std::make_shared<std::vector<std::byte>>(std::vector<std::byte>());
  if (parent_data &&
      (parent_data->size() == 16900 || parent_data->size() == 70756)) {
    const std::vector<std::byte>& from = *parent_data;
    std::vector<std::byte>& to = *(data.get());
    // to = std::vector<std::byte>(from);
    // input.data = std::span<const std::byte>(data->data(), data->size());
    // return data;
    if (parent_data->size() == 16900) {
      to.resize(16900);
      // upsampling the parents' dem is still 65*65
      int row_size = 260;
      int index = 0;
      int offset = 0;
      int current_index = 0;
      if (pQuadtreeTileID.x % 2 == 0 && pQuadtreeTileID.y % 2 == 0) {
        offset = 0;
      } else if (pQuadtreeTileID.x % 2 == 0 && pQuadtreeTileID.y % 2 != 0) {
        offset = 8320;
      } else if (pQuadtreeTileID.x % 2 != 0 && pQuadtreeTileID.y % 2 != 0) {
        offset = 128 + 8320;
      } else {
        offset = 128;
      }
      /* for (int i = 0; i < 33; i++) {
        index = i * row_size + offset;
        for (int j = 0; j < 33; j++) {
          current_index = i * row_size * 2 + j * 8;
          to[current_index] = from[index];
          to[current_index + 1] = from[index + 1];
          to[current_index+2] = from[index+2];
          to[current_index + 3] = from[index + 3];
          if (j != 32) {
            float first = *reinterpret_cast<const float*>(&from[index]);
            float second = *reinterpret_cast<const float*>(&from[index+4]);
            float mid = (first + second) * 0.5f;
            const std::byte* ref = reinterpret_cast<const std::byte*>(&mid);
            to[current_index + 4] = *ref;
            to[current_index + 5] = *(ref + 1);
            to[current_index + 6] = *(ref + 2);
            to[current_index + 7] = *(ref + 3);
          }
          index += 4;
        }
      }
      for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 65; j++) {
          current_index = i * row_size * 2 + j * 4 + row_size;
          float first = *reinterpret_cast<const float*>(&to[current_index -
      row_size]); float second = *reinterpret_cast<const
      float*>(&to[current_index + row_size]); float mid = (first + second)*0.5f;
          const std::byte* ref = reinterpret_cast<const std::byte*>(&mid);
          to[current_index] = *ref;
          to[current_index + 1] = *(ref + 1);
          to[current_index + 2] = *(ref + 2);
          to[current_index + 3] = *(ref + 3);
        }
      }*/
      int32_t px = (pQuadtreeTileID.x % 2) * (65) / 2;
      int32_t py =
          (pQuadtreeTileID.computeInvertedY(tilingScheme) % 2) * (65) / 2;
      float z = 0.0f;
      bool out;
      for (int j = 0; j < 65; j++) {
        for (int i = 0; i < 65; i++) {
          if (j % 2 == 0) {
            if (i % 2 == 0) {
              z = value(from, (j / 2 + py), i / 2 + px, 65);
            } else {
              if (outrange(i / 2 + px)) {
                float z1 = value(from, j / 2 + py, i / 2 + px, 65);
                float z2 = value(from, j / 2 + py, i / 2 + px + 1, 65);
                z = (z1 + z2) / 2;
              } else {
                float z0 = value(from, j / 2 + py, i / 2 + px - 1, 65);
                float z1 = value(from, j / 2 + py, i / 2 + px, 65);
                float z2 = value(from, j / 2 + py, i / 2 + px + 1, 65);
                float z3 = value(from, j / 2 + py, i / 2 + px + 2, 65);
                z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
              }
            }
          } else {
            if (i % 2 == 0) {
              if (outrange(j / 2 + py)) {
                float z1 = value(from, j / 2 + py, i / 2 + px, 65);
                float z2 = value(from, j / 2 + py + 1, i / 2 + px, 65);
                z = (z1 + z2) / 2;
              } else {
                float z0 = value(from, j / 2 + py - 1, i / 2 + px, 65);
                float z1 = value(from, j / 2 + py, i / 2 + px, 65);
                float z2 = value(from, j / 2 + py + 1, i / 2 + px, 65);
                float z3 = value(from, j / 2 + py + 2, i / 2 + px, 65);
                z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
              }
            } else {
              if (outrange(i / 2 + px) || outrange(j / 2 + py)) {
                float z1 = value(from, j / 2 + py + 1, i / 2 + px + 1, 65);
                float z2 = value(from, j / 2 + py, i / 2 + px + 1, 65);
                float z3 = value(from, j / 2 + py, i / 2 + px, 65);
                float z4 = value(from, j / 2 + py + 1, i / 2 + px, 65);
                z = (z1 + z2 + z3 + z4) / 4;
              } else {
                int di, dj;
                z = 0.0f;
                for (dj = -1; dj <= 2; ++dj) {
                  float f = dj == -1 || dj == 2 ? -1 / 16.0f : 9 / 16.0f;
                  for (di = -1; di <= 2; ++di) {
                    float g = di == -1 || di == 2 ? -1 / 16.0f : 9 / 16.0f;
                    float v = value(from, j / 2 + dj + py, i / 2 + di + px, 65);
                    z += f * g * v;
                  }
                }
              }
            }
          }
          add(to, i + j * 65, z);
        }
      }
    } else if (parent_data->size() == 70756) {
      int max_index = 0;
      // upsampling the parents' dem is still 133*133
      //const int row_size = 532; // 2*gride_size
      //int index = 0;
      //int data_index = 0;
      //const int data_start = 536;
      // const int bottom_start = 17822; // 67*266
      // const int right_start = 134;//67*2
      // const int bottom_to = 34846;  // 131*266
      // const int right_to = 262;//131*2
      // different data
      //int boundary_offset = 0;
      to.resize(70756);
      //if (pQuadtreeTileID.x % 2 == 0 && pQuadtreeTileID.y % 2 == 0) {
      //  boundary_offset = 0;
      //} else if (pQuadtreeTileID.x % 2 == 0 && pQuadtreeTileID.y % 2 != 0) {
      //  boundary_offset = 34048; // 64*266
      //} else if (pQuadtreeTileID.x % 2 != 0 && pQuadtreeTileID.y % 2 != 0) {
      //  boundary_offset = 256 + 34048;
      //} else {
      //  boundary_offset = 256; // 64*2
      //}
      // from ev
      int32_t px = 1 + (pQuadtreeTileID.x % 2) * (128) / 2;
      int32_t py =
          1 + (pQuadtreeTileID.computeInvertedY(tilingScheme) % 2) * (128) / 2;
      float z = 0.0f;
      for (int j = 0; j < 133; j++) {
        for (int i = 0; i < 133; i++) {
          if (j % 2 == 0) {
            if (i % 2 == 0) {
              z = value(from, i / 2 + px + (j / 2 + py) * 133);
            } else {
              float z0 = value(from, i / 2 + px - 1 + (j / 2 + py) * 133);
              float z1 = value(from, i / 2 + px + (j / 2 + py) * 133);
              float z2 = value(from, i / 2 + px + 1 + (j / 2 + py) * 133);
              float z3 = value(from, i / 2 + px + 2 + (j / 2 + py) * 133);
              z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
            }
          } else {
            if (i % 2 == 0) {
              float z0 = value(from, i / 2 + px + (j / 2 - 1 + py) * 133);
              float z1 = value(from, i / 2 + px + (j / 2 + py) * 133);
              float z2 = value(from, i / 2 + px + (j / 2 + 1 + py) * 133);
              float z3 = value(from, i / 2 + px + (j / 2 + 2 + py) * 133);
              z = ((z1 + z2) * 9 - (z0 + z3)) / 16;
            } else {
              int di, dj;
              z = 0.0f;
              for (dj = -1; dj <= 2; ++dj) {
                float f = dj == -1 || dj == 2 ? -1 / 16.0f : 9 / 16.0f;
                for (di = -1; di <= 2; ++di) {
                  float g = di == -1 || di == 2 ? -1 / 16.0f : 9 / 16.0f;
                  float v =
                      value(from, i / 2 + di + px + (j / 2 + dj + py) * 133);
                  z += f * g * v;
                }
              }
            }
          }
          add(to, i + j * 133, z);
        }
      }
    }
  }
  return data;
}


HeightMapMeshLoadResult HeightMapMeshLoader::upSampleFromParent(
    const CesiumGeometry::QuadtreeTileID& tileID,
    const Cesium3DTilesSelection::BoundingVolume& tileBoundingVolume,
    const std::string& url,
    const std::vector<std::byte>* parent_data,
    const CesiumGeometry::QuadtreeTilingScheme& tilingScheme,
    const Cesium3DTilesSelection::TilesetContentOptions& contentOptions,
    const TerrainLayerOptions& layerOptions) {
  std::shared_ptr<std::vector<std::byte>> data =
      HeightMapMeshLoader::UpsamplingFromParentData(
          parent_data,
          tileID,
          tilingScheme);
  
  const std::span<std::byte> spandata{*data};
  TerrainLayerOptions newlayerOptions = layerOptions;
  DemlayerHeadinfo& newHeadinfo = newlayerOptions.headInfo;
  newHeadinfo.mnDataType = SSC_LayerDataType_DEM_Float;
  return load(
      tileID,
      tileBoundingVolume,
      url,
      spandata,
      contentOptions,
      newlayerOptions);
}




//HeightMapMeshMetadataResult HeightMapMeshLoader::loadMetadata(
//    const std::span<const std::byte>& data,
//    const CesiumGeometry::QuadtreeTileID& tileID) {
//  return HeightMapMeshMetadataResult();
//}
//
//HeightMapMeshMetadataResult HeightMapMeshLoader::loadAvailabilityRectangles(
//    const rapidjson::Document& metadata,
//    uint32_t startingLevel) {
//  return HeightMapMeshMetadataResult();
//}

} // namespace EVScene
