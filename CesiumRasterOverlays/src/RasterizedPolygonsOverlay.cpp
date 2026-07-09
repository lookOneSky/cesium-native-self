#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumRasterOverlays/Library.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/RasterizedPolygonsOverlay.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <glm/common.hpp>
#include <glm/ext/vector_double2.hpp>
#include <glm/geometric.hpp>
#include <spdlog/fwd.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace CesiumGeometry;
using namespace CesiumGeospatial;
using namespace CesiumUtility;

namespace CesiumRasterOverlays {
namespace {
#pragma region Das
// Check if any polygon in the list is of Edit_Height type
bool hasHeightPolygon(const std::vector<CartographicPolygon>& cartographicPolygons) {
  for (const CartographicPolygon& polygon : cartographicPolygons) {
    if (polygon.eEditType == CesiumGeospatial::EditType::Edit_Height) {
      return true;
    }
  }
  return false;
}

// Check if any polygon in the list is of Edit_Color type
bool hasColorPolygon(const std::vector<CartographicPolygon>& cartographicPolygons) {
  for (const CartographicPolygon& polygon : cartographicPolygons) {
    if (polygon.eEditType == CesiumGeospatial::EditType::Edit_Color) {
      return true;
    }
  }
  return false;
}

// Encode height value into 32-bit integer with lowest bit as color flag
uint32_t encodeHeightValue(float heightCM, bool isInside) {
  uint32_t heightBits = static_cast<uint32_t>(heightCM) & 0x7FFFFFFF; // Mask to 31 bits
  uint32_t colorBit = isInside ? 1 : 0;
  return (heightBits << 1) | colorBit;
}

// Process inner polygons by setting their interior pixels to transparent
void processInnerPolygons(
    CesiumGltf::ImageAsset& image,
    const CesiumGeospatial::GlobeRectangle& rectangle,
    const std::vector<CartographicPolygon>& cartographicPolygons,
    bool useHeightEncoding,
    bool useColorEncoding) {

  const double rectangleWidth = rectangle.computeWidth();
  const double rectangleHeight = rectangle.computeHeight();
  const size_t width = size_t(image.width);
  const size_t height = size_t(image.height);

  for (const CartographicPolygon& polygon : cartographicPolygons) {
    // Only process inner polygons
    if (!polygon.bInnerPolygon) {
      continue;
    }

    const std::vector<glm::dvec2>& vertices = polygon.getVertices();
    const std::vector<uint32_t>& indices = polygon.getIndices();

    for (size_t triangle = 0; triangle < indices.size() / 3; ++triangle) {
      const glm::dvec2& a = vertices[indices[3 * triangle]];
      const glm::dvec2& b = vertices[indices[3 * triangle + 1]];
      const glm::dvec2& c = vertices[indices[3 * triangle + 2]];

      const double minX = glm::min(a.x, glm::min(b.x, c.x));
      const double minY = glm::min(a.y, glm::min(b.y, c.y));
      const double maxX = glm::max(a.x, glm::max(b.x, c.x));
      const double maxY = glm::max(a.y, glm::max(b.y, c.y));

      const CesiumGeospatial::GlobeRectangle triangleBounds(minX, minY, maxX, maxY);

      if (!rectangle.computeIntersection(triangleBounds)) {
        continue;
      }

      const glm::dvec2 ab = b - a;
      const glm::dvec2 ab_perp(-ab.y, ab.x);
      const glm::dvec2 bc = c - b;
      const glm::dvec2 bc_perp(-bc.y, bc.x);
      const glm::dvec2 ca = a - c;
      const glm::dvec2 ca_perp(-ca.y, ca.x);

      for (size_t j = 0; j < height; ++j) {
        const double pixelY = rectangle.getSouth() +
            rectangleHeight * (1.0 - (double(j) + 0.5) / double(height));

        for (size_t i = 0; i < width; ++i) {
          const double pixelX = rectangle.getWest() +
              rectangleWidth * (double(i) + 0.5) / double(width);
          const glm::dvec2 v(pixelX, pixelY);

          const glm::dvec2 av = v - a;
          const glm::dvec2 cv = v - c;

          const double v_proj_ab_perp = glm::dot(av, ab_perp);
          const double v_proj_bc_perp = glm::dot(cv, bc_perp);
          const double v_proj_ca_perp = glm::dot(cv, ca_perp);

          // Check if pixel is inside the inner polygon
          if ((v_proj_ab_perp >= 0.0 && v_proj_ca_perp >= 0.0 && v_proj_bc_perp >= 0.0) ||
              (v_proj_ab_perp <= 0.0 && v_proj_ca_perp <= 0.0 && v_proj_bc_perp <= 0.0)) {
            // Set to transparent for inner polygons
            if (useHeightEncoding || useColorEncoding) {
              size_t pixelIndex = (width * j + i) * 4;
              if (useHeightEncoding) {
                uint32_t encodedValue = encodeHeightValue(0.0f, false);
                image.pixelData[pixelIndex + 0] = static_cast<std::byte>(encodedValue & 0xFF);
                image.pixelData[pixelIndex + 1] = static_cast<std::byte>((encodedValue >> 8) & 0xFF);
                image.pixelData[pixelIndex + 2] = static_cast<std::byte>((encodedValue >> 16) & 0xFF);
                image.pixelData[pixelIndex + 3] = static_cast<std::byte>((encodedValue >> 24) & 0xFF);
              } else {
                image.pixelData[pixelIndex + 0] = static_cast<std::byte>(0);
                image.pixelData[pixelIndex + 1] = static_cast<std::byte>(0);
                image.pixelData[pixelIndex + 2] = static_cast<std::byte>(0);
                image.pixelData[pixelIndex + 3] = static_cast<std::byte>(0);
              }
            } else {
              image.pixelData[width * j + i] = std::byte(0);
            }
          }
        }
      }
    }
  }
}
#pragma endregion

void rasterizePolygons(
    LoadedRasterOverlayImage& loaded,
    const CesiumGeospatial::GlobeRectangle& rectangle,
    const glm::dvec2& textureSize,
    const std::vector<CartographicPolygon>& cartographicPolygons,
    bool invertSelection) {

  CesiumGltf::ImageAsset& image = loaded.pImage.emplace();

#pragma region Das
  // Check if we need to use 32-bit format for height encoding or RGBA for color
  bool useHeightEncoding = hasHeightPolygon(cartographicPolygons);
  bool useColorEncoding = hasColorPolygon(cartographicPolygons);
#pragma endregion

  std::byte insideColor;
  std::byte outsideColor;

  if (invertSelection) {
    insideColor = static_cast<std::byte>(0);
    outsideColor = static_cast<std::byte>(0xff);
  } else {
    insideColor = static_cast<std::byte>(0xff);
    outsideColor = static_cast<std::byte>(0);
  }

  //完全内部
  // create a 1x1 mask if the rectangle is completely inside a polygon
  if (CartographicPolygon::rectangleIsWithinPolygons(
          rectangle,
          cartographicPolygons)) {
    loaded.moreDetailAvailable = false;
    image.width = 1;
    image.height = 1;
#pragma region Das
    if (useHeightEncoding) {
      image.channels = 4;
      image.bytesPerChannel = 1; // 4 channels, 1 byte each for height encoding
      image.pixelData.resize(4);
      // Encode default height value (assuming inside, use first height polygon's value)
      float heightValue = 0.0f;
      for (const CartographicPolygon& polygon : cartographicPolygons) {
        if (polygon.eEditType == CesiumGeospatial::EditType::Edit_Height) {
          heightValue = polygon.fSameHeightCM;
          break;
        }
      }
      uint32_t encodedValue = encodeHeightValue(heightValue, true);
      image.pixelData[0] = static_cast<std::byte>(encodedValue & 0xFF);
      image.pixelData[1] = static_cast<std::byte>((encodedValue >> 8) & 0xFF);
      image.pixelData[2] = static_cast<std::byte>((encodedValue >> 16) & 0xFF);
      image.pixelData[3] = static_cast<std::byte>((encodedValue >> 24) & 0xFF);
    } else if (useColorEncoding) {
      image.channels = 4; // RGBA
      image.bytesPerChannel = 1;
      image.pixelData.resize(4);
      // Use inside color from first color polygon
      glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
      for (const CartographicPolygon& polygon : cartographicPolygons) {
        if (polygon.eEditType == CesiumGeospatial::EditType::Edit_Color) {
          color = polygon.insideColor;
          break;
        }
      }
      image.pixelData[0] = static_cast<std::byte>(color[0] * 255.0f);
      image.pixelData[1] = static_cast<std::byte>(color[1] * 255.0f);
      image.pixelData[2] = static_cast<std::byte>(color[2] * 255.0f);
      image.pixelData[3] = static_cast<std::byte>(color[3] * 255.0f);
    } else {
#pragma endregion
      image.channels = 1;
      image.bytesPerChannel = 1;
      image.pixelData.resize(1, insideColor);
#pragma region Das
    }
    // Check if inside an inner polygon (hole) and set to transparent if needed
    processInnerPolygons(image, rectangle, cartographicPolygons, useHeightEncoding, useColorEncoding);
#pragma endregion
    return;
  }

  // 完全外部
  bool completelyOutsidePolygons = true;
  for (const CartographicPolygon& selection : cartographicPolygons) {
    const std::optional<CesiumGeospatial::GlobeRectangle>& boundingRectangle =
        selection.getBoundingRectangle();

    if (boundingRectangle &&
        rectangle.computeIntersection(*boundingRectangle)) {
      completelyOutsidePolygons = false;
      break;
    }
  }

  // create a 1x1 mask if the rectangle is completely outside all polygons
  if (completelyOutsidePolygons) {
    loaded.moreDetailAvailable = false;
    image.width = 1;
    image.height = 1;
#pragma region Das
    if (useHeightEncoding) {
      image.channels = 4;
      image.bytesPerChannel = 1; // 4 channels, 1 byte each for height encoding
      image.pixelData.resize(4);
      // Encode zero height value for outside pixels
      uint32_t encodedValue = encodeHeightValue(0.0f, false);
      image.pixelData[0] = static_cast<std::byte>(encodedValue & 0xFF);
      image.pixelData[1] = static_cast<std::byte>((encodedValue >> 8) & 0xFF);
      image.pixelData[2] = static_cast<std::byte>((encodedValue >> 16) & 0xFF);
      image.pixelData[3] = static_cast<std::byte>((encodedValue >> 24) & 0xFF);
    } else if (useColorEncoding) {
      image.channels = 4; // RGBA
      image.bytesPerChannel = 1;
      image.pixelData.resize(4);
      // Use transparent color for outside pixels
      glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
      image.pixelData[0] = static_cast<std::byte>(color[0] * 255.0f);
      image.pixelData[1] = static_cast<std::byte>(color[1] * 255.0f);
      image.pixelData[2] = static_cast<std::byte>(color[2] * 255.0f);
      image.pixelData[3] = static_cast<std::byte>(color[3] * 255.0f);
    } else {
#pragma endregion
      image.channels = 1;
      image.bytesPerChannel = 1;
      image.pixelData.resize(1, outsideColor);
#pragma region Das
    }
#pragma endregion
    return;
  }

  const double rectangleWidth = rectangle.computeWidth();
  const double rectangleHeight = rectangle.computeHeight();

  // 先全部填充外部颜色
  // create source image
  loaded.moreDetailAvailable = true;
  image.width = int32_t(glm::round(textureSize.x));
  image.height = int32_t(glm::round(textureSize.y));
#pragma region Das
  if (useHeightEncoding) {
    image.channels = 4;
    image.bytesPerChannel = 1; // 4 channels, 1 byte each for height encoding
    image.pixelData.resize(size_t(image.width * image.height * 4));
    // Initialize with outside color (encoded as zero height, outside bit)
    uint32_t encodedOutside = encodeHeightValue(0.0f, false);
    for (size_t i = 0; i < size_t(image.width * image.height); ++i) {
      image.pixelData[i * 4 + 0] = static_cast<std::byte>(encodedOutside & 0xFF);
      image.pixelData[i * 4 + 1] = static_cast<std::byte>((encodedOutside >> 8) & 0xFF);
      image.pixelData[i * 4 + 2] = static_cast<std::byte>((encodedOutside >> 16) & 0xFF);
      image.pixelData[i * 4 + 3] = static_cast<std::byte>((encodedOutside >> 24) & 0xFF);
    }
  } else if (useColorEncoding) {
    image.channels = 4; // RGBA
    image.bytesPerChannel = 1;
    image.pixelData.resize(size_t(image.width * image.height * 4));
    // Initialize with transparent color for outside pixels
    glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < size_t(image.width * image.height); ++i) {
      image.pixelData[i * 4 + 0] = static_cast<std::byte>(color.x * 255.0f);
      image.pixelData[i * 4 + 1] = static_cast<std::byte>(color.y * 255.0f);
      image.pixelData[i * 4 + 2] = static_cast<std::byte>(color.z * 255.0f);
      image.pixelData[i * 4 + 3] = static_cast<std::byte>(color.w * 255.0f);
    }
  } else {
#pragma endregion
    image.channels = 1;
    image.bytesPerChannel = 1;
    image.pixelData.resize(size_t(image.width * image.height), outsideColor);
#pragma region Das
  }
#pragma endregion

  //相交判断 - 第一阶段：处理非 innerPolygon 的多边形
  // TODO: this is naive approach, use line-triangle
  // intersections to rasterize one row at a time
  // NOTE: also completely ignores antimeridian (really these
  // calculations should be normalized to the first vertex)
  for (const CartographicPolygon& polygon : cartographicPolygons) {
    // Skip inner polygons in first pass
    if (polygon.bInnerPolygon) {
      continue;
    }
    const std::vector<glm::dvec2>& vertices = polygon.getVertices();
    const std::vector<uint32_t>& indices = polygon.getIndices();
    for (size_t triangle = 0; triangle < indices.size() / 3; ++triangle) {
      const glm::dvec2& a = vertices[indices[3 * triangle]];
      const glm::dvec2& b = vertices[indices[3 * triangle + 1]];
      const glm::dvec2& c = vertices[indices[3 * triangle + 2]];

      // TODO: deal with the corner cases here
      const double minX = glm::min(a.x, glm::min(b.x, c.x));
      const double minY = glm::min(a.y, glm::min(b.y, c.y));
      const double maxX = glm::max(a.x, glm::max(b.x, c.x));
      const double maxY = glm::max(a.y, glm::max(b.y, c.y));

      const CesiumGeospatial::GlobeRectangle triangleBounds(
          minX,
          minY,
          maxX,
          maxY);

      // skip this triangle if it is entirely outside the tile bounds
      if (!rectangle.computeIntersection(triangleBounds)) {
        continue;
      }

      const glm::dvec2 ab = b - a;
      const glm::dvec2 ab_perp(-ab.y, ab.x);
      const glm::dvec2 bc = c - b;
      const glm::dvec2 bc_perp(-bc.y, bc.x);
      const glm::dvec2 ca = a - c;
      const glm::dvec2 ca_perp(-ca.y, ca.x);

      size_t width = size_t(image.width);
      size_t height = size_t(image.height);

      for (size_t j = 0; j < height; ++j) {
        const double pixelY =
            rectangle.getSouth() +
            rectangleHeight * (1.0 - (double(j) + 0.5) / double(height));
        for (size_t i = 0; i < width; ++i) {
          const double pixelX = rectangle.getWest() + rectangleWidth *
                                                          (double(i) + 0.5) /
                                                          double(width);
          const glm::dvec2 v(pixelX, pixelY);

          const glm::dvec2 av = v - a;
          const glm::dvec2 cv = v - c;

          const double v_proj_ab_perp = glm::dot(av, ab_perp);
          const double v_proj_bc_perp = glm::dot(cv, bc_perp);
          const double v_proj_ca_perp = glm::dot(cv, ca_perp);

          // will determine in or out, irrespective of winding
          if ((v_proj_ab_perp >= 0.0 && v_proj_ca_perp >= 0.0 &&
               v_proj_bc_perp >= 0.0) ||
              (v_proj_ab_perp <= 0.0 && v_proj_ca_perp <= 0.0 &&
               v_proj_bc_perp <= 0.0)) {
#pragma region Das
            if (useHeightEncoding && polygon.eEditType == CesiumGeospatial::EditType::Edit_Height) {
              // Encode height value for Edit_Height type polygons
              uint32_t encodedValue = encodeHeightValue(polygon.fSameHeightCM, true);
              size_t pixelIndex = (width * j + i) * 4;
              image.pixelData[pixelIndex + 0] = static_cast<std::byte>(encodedValue & 0xFF);
              image.pixelData[pixelIndex + 1] = static_cast<std::byte>((encodedValue >> 8) & 0xFF);
              image.pixelData[pixelIndex + 2] = static_cast<std::byte>((encodedValue >> 16) & 0xFF);
              image.pixelData[pixelIndex + 3] = static_cast<std::byte>((encodedValue >> 24) & 0xFF);
            } else if (useColorEncoding && polygon.eEditType == CesiumGeospatial::EditType::Edit_Color) {
              // Set RGBA color for Edit_Color type polygons
              size_t pixelIndex = (width * j + i) * 4;
              image.pixelData[pixelIndex + 0] = static_cast<std::byte>(polygon.insideColor[0] * 255.0f);
              image.pixelData[pixelIndex + 1] = static_cast<std::byte>(polygon.insideColor[1] * 255.0f);
              image.pixelData[pixelIndex + 2] = static_cast<std::byte>(polygon.insideColor[2] * 255.0f);
              image.pixelData[pixelIndex + 3] = static_cast<std::byte>(polygon.insideColor[3] * 255.0f);
            } else {
#pragma endregion
              image.pixelData[width * j + i] = insideColor;
#pragma region Das
            }
#pragma endregion
          }
        }
      }
    }
  }

  // Process inner polygons to create holes/cutouts
  processInnerPolygons(image, rectangle, cartographicPolygons, useHeightEncoding, useColorEncoding);
}
} // namespace

class CESIUMRASTEROVERLAYS_API RasterizedPolygonsTileProvider final
    : public RasterOverlayTileProvider {

private:
  std::vector<CartographicPolygon> _polygons;
  bool _invertSelection;

public:
  RasterizedPolygonsTileProvider(
      const IntrusivePointer<const RasterOverlay>& pOwner,
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
      const std::shared_ptr<IPrepareRasterOverlayRendererResources>&
          pPrepareRendererResources,
      const std::shared_ptr<spdlog::logger>& pLogger,
      const CesiumGeospatial::Projection& projection,
      const std::vector<CartographicPolygon>& polygons,
      bool invertSelection)
      : RasterOverlayTileProvider(
            pOwner,
            asyncSystem,
            pAssetAccessor,
            std::nullopt,
            pPrepareRendererResources,
            pLogger,
            projection,
            // computeCoverageRectangle(projection, polygons)),
            projectRectangleSimple(
                projection,
                CesiumGeospatial::GlobeRectangle::MAXIMUM)),
        _polygons(polygons),
        _invertSelection(invertSelection) {}

  virtual CesiumAsync::Future<LoadedRasterOverlayImage>
  loadTileImage(RasterOverlayTile& overlayTile) override {
    // Choose the texture size according to the geometry screen size and raster
    // SSE, but no larger than the maximum texture size.
    const RasterOverlayOptions& options = this->getOwner().getOptions();
    glm::dvec2 textureSize = glm::min(
        overlayTile.getTargetScreenPixels() / options.maximumScreenSpaceError,
        glm::dvec2(options.maximumTextureSize));

    return this->getAsyncSystem().runInWorkerThread(
        [&polygons = this->_polygons,
         invertSelection = this->_invertSelection,
         projection = this->getProjection(),
         rectangle = overlayTile.getRectangle(),
         textureSize]() -> LoadedRasterOverlayImage {
          const CesiumGeospatial::GlobeRectangle tileRectangle =
              CesiumGeospatial::unprojectRectangleSimple(projection, rectangle);

          LoadedRasterOverlayImage result;
          result.rectangle = rectangle;

          rasterizePolygons(
              result,
              tileRectangle,
              textureSize,
              polygons,
              invertSelection);

          return result;
        });
  }
};

RasterizedPolygonsOverlay::RasterizedPolygonsOverlay(
    const std::string& name,
    const std::vector<CartographicPolygon>& polygons,
    bool invertSelection,
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const CesiumGeospatial::Projection& projection,
    const RasterOverlayOptions& overlayOptions)
    : RasterOverlay(name, overlayOptions),
      _polygons(polygons),
      _invertSelection(invertSelection),
      _ellipsoid(ellipsoid),
      _projection(projection) {}

RasterizedPolygonsOverlay::~RasterizedPolygonsOverlay() = default;

CesiumAsync::Future<RasterOverlay::CreateTileProviderResult>
RasterizedPolygonsOverlay::createTileProvider(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
    const std::shared_ptr<CreditSystem>& /*pCreditSystem*/,
    const std::shared_ptr<IPrepareRasterOverlayRendererResources>&
        pPrepareRendererResources,
    const std::shared_ptr<spdlog::logger>& pLogger,
    CesiumUtility::IntrusivePointer<const RasterOverlay> pOwner) const {

  pOwner = pOwner ? pOwner : this;

  return asyncSystem.createResolvedFuture<CreateTileProviderResult>(
      IntrusivePointer<RasterOverlayTileProvider>(
          new RasterizedPolygonsTileProvider(
              pOwner,
              asyncSystem,
              pAssetAccessor,
              pPrepareRendererResources,
              pLogger,
              this->_projection,
              this->_polygons,
              this->_invertSelection)));
}

} // namespace CesiumRasterOverlays
