#include <Cesium3DTilesSelection/RasterMappedTo3DTile.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <CesiumGltf/Buffer.h>
#include <CesiumGltf/BufferView.h>
#include <CesiumGltf/Image.h>
#include <CesiumGltf/Model.h>
#include <CesiumUtility/Assert.h>
#include <CesiumUtility/Math.h>

#pragma region Das
#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <Cesium3DTilesSelection/BoundingVolume.h>
#pragma endregion

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef CESIUM_DEBUG_TILE_UNLOADING
#include <unordered_map>
#include "Tile.h"
#endif

using namespace CesiumGeometry;
using namespace CesiumGeospatial;
using namespace CesiumUtility;
using namespace std::string_literals;

namespace Cesium3DTilesSelection {
#ifdef CESIUM_DEBUG_TILE_UNLOADING
std::unordered_map<
    std::string,
    std::vector<TileDoNotUnloadSubtreeCountTracker::Entry>>
    TileDoNotUnloadSubtreeCountTracker::_entries;

void TileDoNotUnloadSubtreeCountTracker::addEntry(
    uint64_t id,
    bool increment,
    const std::string& reason,
    int32_t newCount) {
  const std::string idString = fmt::format("{:x}", id);
  const auto foundIt =
      TileDoNotUnloadSubtreeCountTracker::_entries.find(idString);
  if (foundIt != TileDoNotUnloadSubtreeCountTracker::_entries.end()) {
    foundIt->second.push_back(Entry{reason, increment, newCount});
  } else {
    std::vector<Entry> entries{Entry{reason, increment, newCount}};

    TileDoNotUnloadSubtreeCountTracker::_entries.insert(
        {idString, std::move(entries)});
  }
}
#endif

Tile::Tile(TilesetContentLoader* pLoader) noexcept
    : Tile(TileConstructorImpl{}, TileLoadState::Unloaded, pLoader) {}

Tile::Tile(
    TilesetContentLoader* pLoader,
    std::unique_ptr<TileExternalContent>&& externalContent) noexcept
    : Tile(
          TileConstructorImpl{},
          TileLoadState::ContentLoaded,
          pLoader,
          TileContent(std::move(externalContent))) {}

Tile::Tile(
    TilesetContentLoader* pLoader,
    TileEmptyContent emptyContent) noexcept
    : Tile(
          TileConstructorImpl{},
          TileLoadState::ContentLoaded,
          pLoader,
          emptyContent) {}

template <typename... TileContentArgs, typename TileContentEnable>
Tile::Tile(
    TileConstructorImpl,
    TileLoadState loadState,
    TilesetContentLoader* pLoader,
    TileContentArgs&&... args)
    : _pParent(nullptr),
      _children(),
      _id(""s),
      _boundingVolume(OrientedBoundingBox(glm::dvec3(), glm::dmat3())),
      _viewerRequestVolume(),
      _contentBoundingVolume(),
      _geometricError(0.0),
      _refine(TileRefine::Replace),
      _transform(1.0),
      _lastSelectionState(),
      _loadedTilesLinks(),
      _content{std::forward<TileContentArgs>(args)...},
      _pLoader{pLoader},
      _loadState{loadState},
      _mightHaveLatentChildren{true} {}

Tile::Tile(Tile&& rhs) noexcept
    : _pParent(rhs._pParent),
      _children(std::move(rhs._children)),
      _id(std::move(rhs._id)),
      _boundingVolume(rhs._boundingVolume),
      _viewerRequestVolume(rhs._viewerRequestVolume),
      _contentBoundingVolume(rhs._contentBoundingVolume),
      _geometricError(rhs._geometricError),
      _refine(rhs._refine),
      _transform(rhs._transform),
      _lastSelectionState(rhs._lastSelectionState),
      _loadedTilesLinks(),
      _content(std::move(rhs._content)),
      _pLoader{rhs._pLoader},
      _loadState{rhs._loadState},
      _mightHaveLatentChildren{rhs._mightHaveLatentChildren},
      _rasterTiles(std::move(rhs._rasterTiles)),
      // See the move assignment operator for an explanation of why we copy
      // `_doNotUnloadSubtreeCount` here.
      _doNotUnloadSubtreeCount(rhs._doNotUnloadSubtreeCount) {
  // since children of rhs will have the parent pointed to rhs,
  // we will reparent them to this tile as rhs will be destroyed after this
  for (Tile& tile : this->_children) {
    tile.setParent(this);
  }
}

Tile& Tile::operator=(Tile&& rhs) noexcept {
  if (this != &rhs) {
    this->_loadedTilesLinks = rhs._loadedTilesLinks;

    // since children of rhs will have the parent pointed to rhs,
    // we will reparent them to this tile as rhs will be destroyed after this
    this->_pParent = rhs._pParent;
    this->_children = std::move(rhs._children);
    for (Tile& tile : this->_children) {
      tile.setParent(this);
    }

    this->_id = std::move(rhs._id);
    this->_boundingVolume = rhs._boundingVolume;
    this->_viewerRequestVolume = rhs._viewerRequestVolume;
    this->_contentBoundingVolume = rhs._contentBoundingVolume;
    this->_geometricError = rhs._geometricError;
    this->_refine = rhs._refine;
    this->_transform = rhs._transform;
    this->_lastSelectionState = rhs._lastSelectionState;
    this->_content = std::move(rhs._content);
    this->_pLoader = rhs._pLoader;
    this->_loadState = rhs._loadState;
    this->_rasterTiles = std::move(rhs._rasterTiles);
    this->_mightHaveLatentChildren = rhs._mightHaveLatentChildren;

    // A "count" in the `rhs` could, in theory, represent an external
    // pointer that references that Tile. In that case, we wouldn't want to copy
    // that "count" to this tile because the target of that pointer is not going
    // to change over to this Tile.

    // However, when a "count" represents loaded content in this tile's subtree,
    // that _will_ move over, and so it's essential we copy that count over to
    // the target.

    // There's no way to tell the difference between these two cases. However,
    // as a practical matter, we take pains to avoid having pointers to Tiles
    // that we're moving out of, and so we can safely assume that all "counts"
    // refer to loaded subtree content instead of pointers.
    this->_doNotUnloadSubtreeCount = rhs._doNotUnloadSubtreeCount;
  }

  return *this;
}

void Tile::createChildTiles(std::vector<Tile>&& children) {
  if (!this->_children.empty()) {
    throw std::runtime_error("Children already created.");
  }

  const int32_t prevDoNotUnloadSubtreeCount = this->_doNotUnloadSubtreeCount;
  this->_children = std::move(children);
  for (Tile& tile : this->_children) {
    tile.setParent(this);
    // If a tile is created with children that are already ContentLoaded, we
    // bypassed the normal route that _doNotUnloadSubtreeCount would be
    // incremented by. We have to manually increment it or else we will see a
    // mismatch when trying to unload the tile and fail the assertion.
    if (tile.getState() == TileLoadState::ContentLoaded) {
      ++this->_doNotUnloadSubtreeCount;
    }

    // Add the child's count to our count, as it might represent a tile lower
    // down on the tree that's loaded that we can't see from here. None of the
    // children should have other references to their tile pointer at this
    // moment so this count should just represent loaded children.
    this->_doNotUnloadSubtreeCount += tile._doNotUnloadSubtreeCount;
  }

  const int32_t addedDoNotUnloadSubtreeCount =
      this->_doNotUnloadSubtreeCount - prevDoNotUnloadSubtreeCount;
  if (addedDoNotUnloadSubtreeCount > 0) {
    Tile* pParent = this->getParent();
    while (pParent != nullptr) {
      pParent->_doNotUnloadSubtreeCount += addedDoNotUnloadSubtreeCount;
      pParent = pParent->getParent();
    }
  }
}

double Tile::getNonZeroGeometricError() const noexcept {
  double geometricError = this->getGeometricError();
  if (geometricError > Math::Epsilon5) {
    // Tile's geometric error is good.
    return geometricError;
  }

  const Tile* pParent = this->getParent();
  double divisor = 1.0;

  while (pParent) {
    if (!pParent->getUnconditionallyRefine()) {
      divisor *= 2.0;
      double ancestorError = pParent->getGeometricError();
      if (ancestorError > Math::Epsilon5) {
        return ancestorError / divisor;
      }
    }

    pParent = pParent->getParent();
  }

  // No sensible geometric error all the way to the root of the tile tree.
  // So just use a tiny geometric error and raster selection will be limited by
  // quadtree tile count or texture resolution size.
  return Math::Epsilon5;
}

int64_t Tile::computeByteSize() const noexcept {
  int64_t bytes = 0;

  const TileContent& content = this->getContent();
  const TileRenderContent* pRenderContent = content.getRenderContent();
  if (pRenderContent) {
    const CesiumGltf::Model& model = pRenderContent->getModel();

    // Add up the glTF buffers
    for (const CesiumGltf::Buffer& buffer : model.buffers) {
      bytes += int64_t(buffer.cesium.data.size());
    }

    const std::vector<CesiumGltf::BufferView>& bufferViews = model.bufferViews;
    for (const CesiumGltf::Image& image : model.images) {
      const int32_t bufferView = image.bufferView;
      // For images loaded from buffers, subtract the buffer size before adding
      // the decoded image size.
      if (bufferView >= 0 &&
          bufferView < static_cast<int32_t>(bufferViews.size())) {
        bytes -= bufferViews[size_t(bufferView)].byteLength;
      }

      // sizeBytes is set in TilesetContentManager::ContentKindSetter, if not
      // sooner (e.g., by the renderer implementation).
      if (image.pAsset) {
        bytes += image.pAsset->sizeBytes;
      }
    }
  }

  return bytes;
}

bool Tile::isRenderable() const noexcept {
  if (getState() == TileLoadState::Failed) {
    // Explicitly treat failed tiles as "renderable" - we just treat them like
    // empty tiles.
    return true;
  }

  if (getState() == TileLoadState::Done) {
    // An unconditionally-refined tile is never renderable... UNLESS it has no
    // children, in which case waiting longer will be futile.
    if (!getUnconditionallyRefine() || this->_children.empty()) {
      return std::all_of(
          this->_rasterTiles.begin(),
          this->_rasterTiles.end(),
          [](const RasterMappedTo3DTile& rasterTile) noexcept {
#pragma region jiangs
            if(rasterTile.getReadyTile() == nullptr)
            {
              if (!rasterTile.mbNeedWaitForLoad)
              {
                return true;
              }
            }
#pragma endregion jiangs

            return rasterTile.getReadyTile() != nullptr;
          });
    }
  }

  return false;
}

bool Tile::isRenderContent() const noexcept {
  return this->_content.isRenderContent();
}

bool Tile::isExternalContent() const noexcept {
  return this->_content.isExternalContent();
}

bool Tile::isEmptyContent() const noexcept {
  return this->_content.isEmptyContent();
}

TilesetContentLoader* Tile::getLoader() const noexcept {
  return this->_pLoader;
}

TileLoadState Tile::getState() const noexcept { return this->_loadState; }

void Tile::setParent(Tile* pParent) noexcept { this->_pParent = pParent; }

void Tile::setState(TileLoadState state) noexcept { this->_loadState = state; }

bool Tile::getMightHaveLatentChildren() const noexcept {
  return this->_mightHaveLatentChildren;
}

void Tile::setMightHaveLatentChildren(bool mightHaveLatentChildren) noexcept {
  this->_mightHaveLatentChildren = mightHaveLatentChildren;
}

#pragma region Das
void Tile::setSlowLoad(bool slowLoad) const
{
	this->mbSlowLoad = slowLoad;
}

// 获取 slowLoad 标记
bool Tile::isSlowLoad() const
{
	return this->mbSlowLoad;
}
int Tile::calLevel()
{
  Tile* pTile = this;
	int nLevel = 0;
  while (pTile->getParent())
  {
    nLevel++;
    pTile = pTile->getParent();
  }

  return nLevel;
}
#pragma endregion



void Tile::clearChildren() noexcept {
  CESIUM_ASSERT(this->_doNotUnloadSubtreeCount == 0);
  this->_children.clear();
}

void Tile::incrementDoNotUnloadSubtreeCount(
    [[maybe_unused]] const char* reason) noexcept {
#ifdef CESIUM_DEBUG_TILE_UNLOADING
  const std::string reasonStr = fmt::format(
      "Initiator ID: {:x}, {}",
      reinterpret_cast<uint64_t>(this),
      reason);
  this->incrementDoNotUnloadSubtreeCount(reasonStr);
#else
  this->incrementDoNotUnloadSubtreeCount(std::string());
#endif
}

void Tile::decrementDoNotUnloadSubtreeCount(
    [[maybe_unused]] const char* reason) noexcept {
#ifdef CESIUM_DEBUG_TILE_UNLOADING
  const std::string reasonStr = fmt::format(
      "Initiator ID: {:x}, {}",
      reinterpret_cast<uint64_t>(this),
      reason);
  this->decrementDoNotUnloadSubtreeCount(reasonStr);
#else
  this->decrementDoNotUnloadSubtreeCount(std::string());
#endif
}

void Tile::incrementDoNotUnloadSubtreeCountOnParent(
    const char* reason) noexcept {
  if (this->getParent() != nullptr) {
    this->getParent()->incrementDoNotUnloadSubtreeCount(reason);
  }
}

void Tile::decrementDoNotUnloadSubtreeCountOnParent(
    const char* reason) noexcept {
  if (this->getParent() != nullptr) {
    this->getParent()->decrementDoNotUnloadSubtreeCount(reason);
  }
}

void Tile::incrementDoNotUnloadSubtreeCount(
    [[maybe_unused]] const std::string& reason) noexcept {
  Tile* pTile = this;
  while (pTile != nullptr) {
    ++pTile->_doNotUnloadSubtreeCount;
#ifdef CESIUM_DEBUG_TILE_UNLOADING
    TileDoNotUnloadSubtreeCountTracker::addEntry(
        reinterpret_cast<uint64_t>(pTile),
        true,
        std::string(reason),
        pTile->_doNotUnloadSubtreeCount);
#endif
    pTile = pTile->getParent();
  }
}

void Tile::decrementDoNotUnloadSubtreeCount(
    [[maybe_unused]] const std::string& reason) noexcept {
  CESIUM_ASSERT(this->_doNotUnloadSubtreeCount > 0);
  Tile* pTile = this;
  while (pTile != nullptr) {
    --pTile->_doNotUnloadSubtreeCount;
#ifdef CESIUM_DEBUG_TILE_UNLOADING
    TileDoNotUnloadSubtreeCountTracker::addEntry(
        reinterpret_cast<uint64_t>(pTile),
        false,
        std::string(reason),
        pTile->_doNotUnloadSubtreeCount);
#endif
    pTile = pTile->getParent();
  }
}

#pragma region Das
bool Tile::getUnconditionallyRefine() const{

#pragma region Das
    // 检查是否有TileContent且是RenderContent（Model类型）
    const TileContent& content = this->getContent();
    if (content.isRenderContent()) {
        const TileRenderContent* pRenderContent = content.getRenderContent();
        if (pRenderContent) {
            const CesiumGltf::Model& model = pRenderContent->getModel();
            // 如果Model有ignoreLoad标记，返回true
            if (model.ignoreLoad) {
                return true;
            }
        }
    }

    //有的数据的_geometricError给错了。
    //空数据_geometricError比parent 小。显示但是没内容
    if (content.isEmptyContent())
    {
      return true;
    }
#pragma endregion

    // 原有逻辑：检查geometricError是否为无穷大
    bool bRefine = glm::isinf(this->_geometricError);
    return bRefine;
}

bool Tile::checkPolygonIntersection(std::shared_ptr<const CesiumGeospatial::CartographicPolygon> polygon) const {
    // 如果多边形为空，认为不相交
    if (!polygon) {
        mbIntersectsPolygon = false;
        mCachedPolygon = polygon;
        return false;
    }
    
    // 如果缓存的多边形指针与当前多边形指针一致，直接返回缓存结果
    if (mCachedPolygon == polygon) {
        return mbIntersectsPolygon;
    }
    
    // 需要重新计算相交状态
    mbIntersectsPolygon = false;
    mCachedPolygon = polygon;
    
    // 获取瓦块的包围盒
    const BoundingVolume& boundingVolume = this->getBoundingVolume();
    
    // 使用BoundingVolume.h中的estimateGlobeRectangle函数将boundingVolume转换为GlobeRectangle
    std::optional<CesiumGeospatial::GlobeRectangle> optionalRect = 
        estimateGlobeRectangle(boundingVolume, CesiumGeospatial::Ellipsoid::WGS84);
    
    if (optionalRect.has_value()) {
        const CesiumGeospatial::GlobeRectangle& rect = optionalRect.value();
        const std::vector<glm::dvec2>& polygonVertices = polygon->getVertices();
        
        if (polygonVertices.size() >= 3) {
            // 计算多边形的包围盒
            double minLon = polygonVertices[0].x, maxLon = polygonVertices[0].x;
            double minLat = polygonVertices[0].y, maxLat = polygonVertices[0].y;
            
            for (const auto& vertex : polygonVertices) {
                minLon = std::min(minLon, vertex.x);
                maxLon = std::max(maxLon, vertex.x);
                minLat = std::min(minLat, vertex.y);
                maxLat = std::max(maxLat, vertex.y);
            }
            
            // 检查矩形相交
            mbIntersectsPolygon = !(rect.getEast() < minLon || rect.getWest() > maxLon ||
                                   rect.getNorth() < minLat || rect.getSouth() > maxLat);
        }
    }
    
    return mbIntersectsPolygon;
}
#pragma endregion

} // namespace Cesium3DTilesSelection
