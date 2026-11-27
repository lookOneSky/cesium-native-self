#include "EmptyRasterOverlayTileProvider.h"

#include <Cesium3DTilesSelection/RasterMappedTo3DTile.h>
#include <Cesium3DTilesSelection/RasterOverlayCollection.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <CesiumAsync/Future.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayLoadFailureDetails.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>
#include <CesiumUtility/Assert.h>
#include <CesiumUtility/IntrusivePointer.h>

#include <fmt/format.h>
#include <glm/ext/vector_double2.hpp>
#include <nonstd/expected.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <utility>
#include <vector>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace CesiumGeometry;
using namespace CesiumGeospatial;
using namespace CesiumRasterOverlays;
using namespace CesiumUtility;

namespace Cesium3DTilesSelection {

namespace {

template <class Function>
void forEachTile(Tile::LoadedLinkedList& list, Function callback) {
  Tile* pCurrent = list.head();
  while (pCurrent) {
    Tile* pNext = list.next(pCurrent);
    callback(*pCurrent);
    pCurrent = pNext;
  }
}

// We use these to avoid a heap allocation just to return empty vectors.
const std::vector<CesiumUtility::IntrusivePointer<RasterOverlay>>
    emptyOverlays{};
const std::vector<CesiumUtility::IntrusivePointer<RasterOverlayTileProvider>>
    emptyTileProviders{};

#pragma region Das 
// 递归收集所有子Tile到列表中
void collectAllTiles(Tile& tile, std::vector<Tile*>& allTiles) {
  allTiles.push_back(&tile);

  // 递归收集所有子Tile
  std::span<Tile> children = tile.getChildren();
  for (Tile& child : children) {
    collectAllTiles(child, allTiles);
  }
}

// 从LoadedLinkedList中获取根Tile，然后收集所有Tile
std::vector<Tile*> getAllTilesFromRoot(Tile::LoadedLinkedList& list) {
  std::vector<Tile*> allTiles;

  Tile* pFirst = list.head();
  if (!pFirst) {
    return allTiles;
  }

  // 从第一个Tile找到根Tile
  Tile* pRoot = pFirst;
  while (pRoot->getParent() != nullptr) {
    pRoot = pRoot->getParent();
  }

  // 从根Tile递归收集所有Tile
  collectAllTiles(*pRoot, allTiles);

  return allTiles;
}
#pragma endregion


} // namespace

RasterOverlayCollection::RasterOverlayCollection(
    Tile::LoadedLinkedList& loadedTiles,
    const TilesetExternals& externals,
    const CesiumGeospatial::Ellipsoid& ellipsoid) noexcept
    : _pLoadedTiles(&loadedTiles),
      _externals{externals},
      _ellipsoid(ellipsoid),
      _pOverlays(nullptr)
{
#pragma region das
  [[maybe_unused]] int a = 0;
#pragma endregion
  a++;
}

RasterOverlayCollection::~RasterOverlayCollection() noexcept {
  if (this->_pOverlays) {
    OverlayList& list = *this->_pOverlays;
    if (!list.overlays.empty()) {
      for (int64_t i = static_cast<int64_t>(list.overlays.size() - 1); i >= 0;
           --i) {
        this->remove(list.overlays[static_cast<size_t>(i)].get());
      }
    }
  }
}

void RasterOverlayCollection::add(
    const CesiumUtility::IntrusivePointer<RasterOverlay>& pOverlay) {
  // CESIUM_TRACE_USE_TRACK_SET(this->_loadingSlots);

  if (!this->_pOverlays)
    this->_pOverlays = new OverlayList();

  IntrusivePointer<OverlayList> pList = this->_pOverlays;

  pList->overlays.emplace_back(pOverlay);

  IntrusivePointer<RasterOverlayTileProvider> pPlaceholder =
      pOverlay->createPlaceholder(
          this->_externals.asyncSystem,
          this->_externals.pAssetAccessor,
          this->_ellipsoid);

  pList->tileProviders.emplace_back(pPlaceholder);
  pList->placeholders.emplace_back(pPlaceholder);

  // CESIUM_TRACE_BEGIN_IN_TRACK("createTileProvider");

  CesiumAsync::Future<RasterOverlay::CreateTileProviderResult> future =
      pOverlay->createTileProvider(
          this->_externals.asyncSystem,
          this->_externals.pAssetAccessor,
          this->_externals.pCreditSystem,
          this->_externals.pPrepareRendererResources,
          this->_externals.pLogger,
          nullptr);

  // Add a placeholder for this overlay to existing geometry tiles.
  forEachTile(*this->_pLoadedTiles, [&](Tile& tile) {
    // The tile rectangle and geometric error don't matter for a placeholder.
    // - When a tile is transitioned from Unloaded to Loading, raster overlay
    // tiles will be mapped to the tile automatically by TilesetContentManager,
    // so we don't need to map the raster tiles to this unloaded or unloading
    // tile now.
    // - When a tile is already failed to load, there is no need to map the
    // raster tiles to the tile as it is not rendered any way
    TileLoadState tileState = tile.getState();
    if (tileState != TileLoadState::Unloaded &&
        tileState != TileLoadState::Unloading &&
        tileState != TileLoadState::Failed) {
      tile.getMappedRasterTiles().emplace_back(
          pPlaceholder->getTile(Rectangle(), glm::dvec2(0.0)),
          -1);
    }
  });

  // This continuation, by capturing pList, keeps the OverlayList from being
  // destroyed. But it does not keep the RasterOverlayCollection itself alive.
  std::move(future)
      .catchInMainThread(
          [](const std::exception& e)
              -> RasterOverlay::CreateTileProviderResult {
            return nonstd::make_unexpected(RasterOverlayLoadFailureDetails{
                RasterOverlayLoadType::Unknown,
                nullptr,
                fmt::format(
                    "Error while creating tile provider: {0}",
                    e.what())});
          })
      .thenInMainThread([pOverlay,
                         pList,
                         pLogger = this->_externals.pLogger,
                         asyncSystem = this->_externals.asyncSystem](
                            RasterOverlay::CreateTileProviderResult&& result) {
        IntrusivePointer<RasterOverlayTileProvider> pProvider = nullptr;
        if (result) {
          pProvider = *result;
        } else {
          // Report error creating the tile provider.
          const RasterOverlayLoadFailureDetails& failureDetails =
              result.error();
          SPDLOG_LOGGER_ERROR(pLogger, failureDetails.message);
          if (pOverlay->getOptions().loadErrorCallback) {
            pOverlay->getOptions().loadErrorCallback(failureDetails);
          }

          // Create a tile provider that does not provide any tiles at all.
          pProvider = new EmptyRasterOverlayTileProvider(pOverlay, asyncSystem);
        }

        auto it =
            std::find(pList->overlays.begin(), pList->overlays.end(), pOverlay);

        // Find the overlay's current location in the list.
        // It's possible it has been removed completely.
        if (it != pList->overlays.end()) {
          std::int64_t index = it - pList->overlays.begin();
          pList->tileProviders[size_t(index)] = pProvider;
        }

        // CESIUM_TRACE_END_IN_TRACK("createTileProvider");
      });
}

void RasterOverlayCollection::remove(
    const CesiumUtility::IntrusivePointer<RasterOverlay>& pOverlay) noexcept {
  if (!this->_pOverlays)
    return;

  // Remove all mappings of this overlay to geometry tiles.
  auto removeCondition = [pOverlay](
                             const RasterMappedTo3DTile& mapped) noexcept {

#pragma region Das
			//3Dtiles raterOverlay PlaceHolder释放
			if (mapped.getLoadingTile() && mapped.getLoadingTile()->getState() == RasterOverlayTile::LoadState::Placeholder && pOverlay == &mapped.getLoadingTile()->getTileProvider().getOwner())
			{
				[[maybe_unused]] int a = 0;
				a++;
				return true;
			}
#pragma endregion
    
    bool bRemove = (
        (mapped.getLoadingTile() &&
         pOverlay == &mapped.getLoadingTile()->getTileProvider().getOwner()) ||
        (mapped.getReadyTile() &&
         pOverlay == &mapped.getReadyTile()->getTileProvider().getOwner()));

    if (!bRemove)
    {
#pragma region das
      [[maybe_unused]] int a = 0;
#pragma endregion
      a++;
    }

    return bRemove;
  };

#pragma region Das
  //// 生成带时间戳的文件名，记录所有遍历到的tile ID
  // auto now = std::chrono::system_clock::now();
  // auto time_t_now = std::chrono::system_clock::to_time_t(now);
  // std::tm tm_now;
  // localtime_s(&tm_now, &time_t_now);

  // std::ostringstream ossFileName;
  // ossFileName << "TileIDs_"
  //             << std::put_time(&tm_now, "%Y%m%d_%H%M%S")
  //             << ".txt";

  // std::ofstream logFile(ossFileName.str());
  // if (logFile.is_open()) {
  //   logFile << "Tile IDs traversed at: "
  //           << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S")
  //           << "\n\n";
  // }
#pragma endregion

  auto pPrepareRenderResources =
      this->_externals.pPrepareRendererResources.get();

#pragma region Das
  // 获取所有Tile（包括未加载的）而不仅仅是LoadedTiles
  std::vector<Tile*> allTiles = getAllTilesFromRoot(*this->_pLoadedTiles);
  for (Tile* pTile : allTiles) {
    Tile& tile = *pTile;

        //// 记录当前tile的ID
        //if (logFile.is_open()) {
        //  const TileID& tileID = tile.getTileID();
        //  if (const std::string* pStrID = std::get_if<std::string>(&tileID)) {
        //    logFile << "Tile ID (string): " << *pStrID << "\n";
        //  }
        //}
#pragma endregion

        std::vector<RasterMappedTo3DTile>& mapped = tile.getMappedRasterTiles();

        for (RasterMappedTo3DTile& rasterTile : mapped) {
          if (removeCondition(rasterTile)) {
            rasterTile.detachFromTile(*pPrepareRenderResources, tile);
          }
#pragma region Das
          else {
            [[maybe_unused]] int a = 0;
            a++;
          }
#pragma endregion
        }

    auto firstToRemove =
        std::remove_if(mapped.begin(), mapped.end(), removeCondition);
    mapped.erase(firstToRemove, mapped.end());
  }
#pragma endregion

  OverlayList& list = *this->_pOverlays;

  CESIUM_ASSERT(list.overlays.size() == list.tileProviders.size());
  CESIUM_ASSERT(list.overlays.size() == list.placeholders.size());

  auto it = std::find_if(
      list.overlays.begin(),
      list.overlays.end(),
      [pOverlay](const IntrusivePointer<RasterOverlay>& pCheck) noexcept {
        return pCheck == pOverlay;
      });
  if (it == list.overlays.end()) {
    return;
  }

  int64_t index = it - list.overlays.begin();
  list.overlays.erase(list.overlays.begin() + index);
  list.tileProviders.erase(list.tileProviders.begin() + index);
  list.placeholders.erase(list.placeholders.begin() + index);
}

const std::vector<CesiumUtility::IntrusivePointer<RasterOverlay>>&
RasterOverlayCollection::getOverlays() const {
  if (!this->_pOverlays)
    return emptyOverlays;

  return this->_pOverlays->overlays;
}

/**
 * @brief Gets the tile providers in this collection. Each tile provider
 * corresponds with the overlay at the same position in the collection
 * returned by {@link getOverlays}.
 */
const std::vector<CesiumUtility::IntrusivePointer<RasterOverlayTileProvider>>&
RasterOverlayCollection::getTileProviders() const {
  if (!this->_pOverlays)
    return emptyTileProviders;

  return this->_pOverlays->tileProviders;
}

const std::vector<CesiumUtility::IntrusivePointer<RasterOverlayTileProvider>>&
RasterOverlayCollection::getPlaceholderTileProviders() const {
  if (!this->_pOverlays)
    return emptyTileProviders;

  return this->_pOverlays->placeholders;
}

RasterOverlayTileProvider* RasterOverlayCollection::findTileProviderForOverlay(
    RasterOverlay& overlay) noexcept {
  // Call the const version
  const RasterOverlayTileProvider* pResult = this->findTileProviderForOverlay(
      const_cast<const RasterOverlay&>(overlay));
  return const_cast<RasterOverlayTileProvider*>(pResult);
}

const RasterOverlayTileProvider*
RasterOverlayCollection::findTileProviderForOverlay(
    const RasterOverlay& overlay) const noexcept {
  if (!this->_pOverlays)
    return nullptr;

  const auto& overlays = this->_pOverlays->overlays;
  const auto& tileProviders = this->_pOverlays->tileProviders;

  CESIUM_ASSERT(overlays.size() == tileProviders.size());

  for (size_t i = 0; i < overlays.size() && i < tileProviders.size(); ++i) {
    if (overlays[i].get() == &overlay)
      return tileProviders[i].get();
  }

  return nullptr;
}

RasterOverlayTileProvider*
RasterOverlayCollection::findPlaceholderTileProviderForOverlay(
    RasterOverlay& overlay) noexcept {
  // Call the const version
  const RasterOverlayTileProvider* pResult =
      this->findPlaceholderTileProviderForOverlay(
          const_cast<const RasterOverlay&>(overlay));
  return const_cast<RasterOverlayTileProvider*>(pResult);
}

const RasterOverlayTileProvider*
RasterOverlayCollection::findPlaceholderTileProviderForOverlay(
    const RasterOverlay& overlay) const noexcept {
  if (!this->_pOverlays)
    return nullptr;

  const auto& overlays = this->_pOverlays->overlays;
  const auto& placeholders = this->_pOverlays->placeholders;

  CESIUM_ASSERT(overlays.size() == placeholders.size());

  for (size_t i = 0; i < overlays.size() && i < placeholders.size(); ++i) {
    if (overlays[i].get() == &overlay)
      return placeholders[i].get();
  }

  return nullptr;
}

RasterOverlayCollection::const_iterator
RasterOverlayCollection::begin() const noexcept {
  if (!this->_pOverlays)
    return emptyOverlays.begin();

  return this->_pOverlays->overlays.begin();
}

RasterOverlayCollection::const_iterator
RasterOverlayCollection::end() const noexcept {
  if (!this->_pOverlays)
    return emptyOverlays.end();

  return this->_pOverlays->overlays.end();
}

size_t RasterOverlayCollection::size() const noexcept {
  if (!this->_pOverlays)
    return 0;

  return this->_pOverlays->overlays.size();
}

} // namespace Cesium3DTilesSelection
