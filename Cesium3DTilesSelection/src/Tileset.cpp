#include "TilesetContentManager.h"
#include "TilesetHeightQuery.h"

#include <Cesium3DTilesSelection/ITileExcluder.h>
#include <Cesium3DTilesSelection/LoadedTileEnumerator.h>
#include <Cesium3DTilesSelection/RasterMappedTo3DTile.h>
#include <Cesium3DTilesSelection/RasterOverlayCollection.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileLoadTask.h>
#include <Cesium3DTilesSelection/TileOcclusionRendererProxy.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetContentLoaderFactory.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetFrameState.h>
#include <Cesium3DTilesSelection/TilesetMetadata.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/TilesetSelection.h>
#include <Cesium3DTilesSelection/TilesetViewGroup.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Promise.h>
#include <CesiumAsync/SharedFuture.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumRasterOverlays/ActivatedRasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumUtility/Assert.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/IntrusivePointer.h>
#include <CesiumUtility/Tracing.h>

#include <glm/common.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace CesiumAsync;
using namespace CesiumGeometry;
using namespace CesiumGeospatial;
using namespace CesiumRasterOverlays;
using namespace CesiumUtility;

namespace Cesium3DTilesSelection {

Tileset::Tileset(
    const TilesetExternals& externals,
    std::unique_ptr<TilesetContentLoader>&& pCustomLoader,
    std::unique_ptr<Tile>&& pRootTile,
    const TilesetOptions& options)
    : _externals(externals),
      _asyncSystem(externals.asyncSystem),
      _options(options),
      _distances(),
      _childOcclusionProxies(),
      _pTilesetContentManager{
          TilesetContentManager::createFromLoader(
              _externals,
              _options,
              std::move(pCustomLoader),
              std::move(pRootTile)),
      },
      _heightRequests(),
      _defaultViewGroup() {}

Tileset::Tileset(
    const TilesetExternals& externals,
    const std::string& url,
#pragma region das
    const TilesetOptions& options,
    const DasTilesetExtra& dasExtra)
#pragma endregion
    : _externals(externals),
      _asyncSystem(externals.asyncSystem),
      _options(options),
      _distances(),
      _childOcclusionProxies(),
      _pTilesetContentManager{
          TilesetContentManager::createFromUrl(
              this->_externals,
              this->_options,
#pragma region das
              url,
              dasExtra),
#pragma endregion
      },
      _heightRequests(),
      _defaultViewGroup() {}

Tileset::Tileset(
    const TilesetExternals& externals,
    int64_t ionAssetID,
    const std::string& ionAccessToken,
    const TilesetOptions& options,
    const std::string& ionAssetEndpointUrl)
    : _externals(externals),
      _asyncSystem(externals.asyncSystem),
      _options(options),
      _distances(),
      _childOcclusionProxies(),
      _pTilesetContentManager{TilesetContentManager::createFromCesiumIon(
          this->_externals,
          this->_options,
          ionAssetID,
          ionAccessToken,
          ionAssetEndpointUrl)},
      _heightRequests(),
      _defaultViewGroup() {}

Tileset::Tileset(
    const TilesetExternals& externals,
    TilesetContentLoaderFactory&& loaderFactory,
    const TilesetOptions& options)
    : _externals(externals),
      _asyncSystem(externals.asyncSystem),
      _options(options),
      _distances(),
      _childOcclusionProxies(),
      _pTilesetContentManager{TilesetContentManager::createFromLoaderFactory(
          _externals,
          _options,
          std::move(loaderFactory))} {}

Tileset::~Tileset() noexcept {
  TilesetHeightRequest::failHeightRequests(
      this->_heightRequests,
      "Tileset is being destroyed.");

  this->_pTilesetContentManager->markTilesetDestroyed();
  this->_pTilesetContentManager->unloadAll();
  if (this->_externals.pTileOcclusionProxyPool) {
    this->_externals.pTileOcclusionProxyPool->destroyPool();
  }
}

CesiumAsync::SharedFuture<void>& Tileset::getAsyncDestructionCompleteEvent() {
  return this->_pTilesetContentManager->getAsyncDestructionCompleteEvent();
}

CesiumAsync::SharedFuture<void>& Tileset::getRootTileAvailableEvent() {
  return this->_pTilesetContentManager->getRootTileAvailableEvent();
}

std::optional<CesiumUtility::Credit> Tileset::getUserCredit() const noexcept {
  const CesiumUtility::Credit* pCredit =
      this->_pTilesetContentManager->getUserCredit();
  return pCredit ? std::make_optional(*pCredit) : std::nullopt;
}

const std::vector<Credit>& Tileset::getTilesetCredits() const noexcept {
  return this->_pTilesetContentManager->getTilesetCredits();
}

void Tileset::setShowCreditsOnScreen(bool showCreditsOnScreen) noexcept {
  this->_options.showCreditsOnScreen = showCreditsOnScreen;

  const std::vector<Credit>& credits = this->getTilesetCredits();
  auto pCreditSystem = this->_externals.pCreditSystem;
  for (auto credit : credits) {
    pCreditSystem->setShowOnScreen(credit, showCreditsOnScreen);
  }
}

const CesiumUtility::CreditSource& Tileset::getCreditSource() const noexcept {
  return this->_pTilesetContentManager->getCreditSource();
}

const Tile* Tileset::getRootTile() const noexcept {
  return this->_pTilesetContentManager->getRootTile();
}

Tile* Tileset::getRootTile() noexcept {
  return this->_pTilesetContentManager->getRootTile();
}

RasterOverlayCollection& Tileset::getOverlays() noexcept {
  return this->_pTilesetContentManager->getRasterOverlayCollection();
}

const RasterOverlayCollection& Tileset::getOverlays() const noexcept {
  return this->_pTilesetContentManager->getRasterOverlayCollection();
}

TilesetSharedAssetSystem& Tileset::getSharedAssetSystem() noexcept {
  return *this->_pTilesetContentManager->getSharedAssetSystem();
}

const TilesetSharedAssetSystem& Tileset::getSharedAssetSystem() const noexcept {
  return *this->_pTilesetContentManager->getSharedAssetSystem();
}

// NOLINTBEGIN(misc-use-anonymous-namespace)
static bool
operator<(const FogDensityAtHeight& fogDensity, double height) noexcept {
  return fogDensity.cameraHeight < height;
}
// NOLINTEND(misc-use-anonymous-namespace)

namespace {

double computeFogDensity(
    const std::vector<FogDensityAtHeight>& fogDensityTable,
    const ViewState& viewState) {
  const double height = viewState.getPositionCartographic()
                            .value_or(Cartographic(0.0, 0.0, 0.0))
                            .height;

  // Find the entry that is for >= this camera height.
  auto nextIt =
      std::lower_bound(fogDensityTable.begin(), fogDensityTable.end(), height);

  if (nextIt == fogDensityTable.end()) {
    return fogDensityTable.back().fogDensity;
  }
  if (nextIt == fogDensityTable.begin()) {
    return nextIt->fogDensity;
  }

  auto prevIt = nextIt - 1;

  const double heightA = prevIt->cameraHeight;
  const double densityA = prevIt->fogDensity;

  const double heightB = nextIt->cameraHeight;
  const double densityB = nextIt->fogDensity;

  const double t =
      glm::clamp((height - heightA) / (heightB - heightA), 0.0, 1.0);

  const double density = glm::mix(densityA, densityB, t);

  // CesiumJS will also fade out the fog based on the camera angle,
  // so when we're looking straight down there's no fog. This is unfortunate
  // because it prevents the fog culling from being used in place of horizon
  // culling. Horizon culling is the only thing in CesiumJS that prevents
  // tiles on the back side of the globe from being rendered.
  // Since we're not actually _rendering_ the fog in cesium-native (that's on
  // the renderer), we don't need to worry about the fog making the globe
  // looked washed out in straight down views. So here we don't fade by
  // angle at all.

  return density;
}

} // namespace

void Tileset::_updateLodTransitions(
    const TilesetFrameState& /* frameState */,
    float deltaTime,
    ViewUpdateResult& result) const noexcept {
  if (_options.enableLodTransitionPeriod) {
    // We always fade tiles from 0.0 --> 1.0. Whether the tile is fading in or
    // out is determined by whether the tile is in the tilesToRenderThisFrame
    // or tilesFadingOut list.
    float deltaTransitionPercentage =
        deltaTime / this->_options.lodTransitionLength;

    // Update fade out
    for (auto tileIt = result.tilesFadingOut.begin();
         tileIt != result.tilesFadingOut.end();) {
      TileRenderContent* pRenderContent = const_cast<TileRenderContent*>(
          (*tileIt)->getContent().getRenderContent());

      if (!pRenderContent) {
        // This tile is done fading out and was immediately kicked from the
        // cache.
        tileIt = result.tilesFadingOut.erase(tileIt);
        continue;
      }

      float currentPercentage =
          pRenderContent->getLodTransitionFadePercentage();
      if (currentPercentage >= 1.0f) {
        // Remove this tile from the fading out list if it is already done.
        // The client will already have had a chance to stop rendering the tile
        // last frame.
        pRenderContent->setLodTransitionFadePercentage(0.0f);
        tileIt = result.tilesFadingOut.erase(tileIt);
        continue;
      }

      float newPercentage =
          glm::min(currentPercentage + deltaTransitionPercentage, 1.0f);
      pRenderContent->setLodTransitionFadePercentage(newPercentage);
      ++tileIt;
    }

    // Update fade in
    for (const Tile::ConstPointer& pTile : result.tilesToRenderThisFrame) {
      TileRenderContent* pRenderContent = const_cast<TileRenderContent*>(
          pTile->getContent().getRenderContent());
      if (pRenderContent) {
        float transitionPercentage =
            pRenderContent->getLodTransitionFadePercentage();
        float newTransitionPercentage =
            glm::min(transitionPercentage + deltaTransitionPercentage, 1.0f);
        pRenderContent->setLodTransitionFadePercentage(newTransitionPercentage);
      }

      // Remove a tile from fade-out list if it is back on the render list.
      if (result.tilesFadingOut.erase(pTile) > 0) {
        if (pRenderContent)
          pRenderContent->setLodTransitionFadePercentage(0.0f);
      }
    }
  } else {
    // If there are any tiles still fading in, set them to fully visible right
    // away.
    for (const Tile::ConstPointer& pTile : result.tilesToRenderThisFrame) {
      TileRenderContent* pRenderContent = const_cast<TileRenderContent*>(
          pTile->getContent().getRenderContent());
      if (pRenderContent) {
        pRenderContent->setLodTransitionFadePercentage(1.0f);
      }
    }
  }
}

const ViewUpdateResult& Tileset::updateViewGroupOffline(
    TilesetViewGroup& viewGroup,
    const std::vector<ViewState>& frustums) {
  ViewUpdateResult& updateResult = viewGroup.getViewUpdateResult();
  std::vector<Tile::ConstPointer> tilesSelectedPrevFrame =
      updateResult.tilesToRenderThisFrame;

  // TODO: fix the fading for offline case
  // (https://github.com/CesiumGS/cesium-native/issues/549)
  this->_asyncSystem.dispatchMainThreadTasks();
  this->updateViewGroup(viewGroup, frustums, 0.0f);
  while (viewGroup.getPreviousLoadProgressPercentage() < 100.0f) {
    this->_externals.pAssetAccessor->tick();
    this->_asyncSystem.dispatchMainThreadTasks();
    this->loadTiles();

    // If there are no frustums, we'll never make any progress. So break here to
    // avoid getting stuck in an endless loop.
    if (frustums.empty())
      break;

    this->_asyncSystem.dispatchMainThreadTasks();
    this->updateViewGroup(viewGroup, frustums, 0.0f);
  }

  updateResult.tilesFadingOut.clear();

  std::unordered_set<const Tile*> uniqueTilesToRenderThisFrame;
  uniqueTilesToRenderThisFrame.reserve(
      updateResult.tilesToRenderThisFrame.size());
  for (const Tile::ConstPointer& pTile : updateResult.tilesToRenderThisFrame) {
    uniqueTilesToRenderThisFrame.insert(pTile.get());
  }

  for (const Tile::ConstPointer& pTile : tilesSelectedPrevFrame) {
    if (uniqueTilesToRenderThisFrame.find(pTile.get()) ==
        uniqueTilesToRenderThisFrame.end()) {
      TileRenderContent* pRenderContent = const_cast<TileRenderContent*>(
          pTile->getContent().getRenderContent());
      if (pRenderContent) {
        pRenderContent->setLodTransitionFadePercentage(1.0f);
        updateResult.tilesFadingOut.insert(pTile);
      }
    }
  }

  return updateResult;
}

const ViewUpdateResult&

Tileset::updateView(const std::vector<ViewState>& frustums, float deltaTime) {
  this->_asyncSystem.dispatchMainThreadTasks();
  const ViewUpdateResult& result =
      this->updateViewGroup(this->_defaultViewGroup, frustums, deltaTime);
  this->_asyncSystem.dispatchMainThreadTasks();
  this->loadTiles();
  return result;
}

const ViewUpdateResult&
Tileset::updateViewOffline(const std::vector<ViewState>& frustums) {
  return this->updateViewGroupOffline(this->_defaultViewGroup, frustums);
}

const ViewUpdateResult& Tileset::updateViewGroup(
    TilesetViewGroup& viewGroup,
    const std::vector<ViewState>& frustums,
    float deltaTime) {
  CESIUM_TRACE("Tileset::updateViewGroup");

  this->registerLoadRequester(viewGroup);

  // Fixup TilesetOptions to ensure lod transitions works correctly.
  _options.enableFrustumCulling =
      _options.enableFrustumCulling && !_options.enableLodTransitionPeriod;
  _options.enableFogCulling =
      _options.enableFogCulling && !_options.enableLodTransitionPeriod;
  ViewUpdateResult& result = viewGroup.getViewUpdateResult();

  Tile* pRootTile = this->_pTilesetContentManager->getRootTile();
  if (!pRootTile) {
    return result;
  }

  for (const std::shared_ptr<ITileExcluder>& pExcluder :
       this->_options.excluders) {
    pExcluder->startNewFrame();
  }

  std::vector<double> fogDensities(frustums.size());
  std::transform(
      frustums.begin(),
      frustums.end(),
      fogDensities.begin(),
      [&fogDensityTable =
           this->_options.fogDensityTable](const ViewState& frustum) -> double {
        return computeFogDensity(fogDensityTable, frustum);
      });

  TilesetFrameState frameState{
      viewGroup,
      frustums,
      std::move(fogDensities),
      [this](Tile& tile) {
        this->_pTilesetContentManager->updateTileContent(tile, this->_options);
      }};

  if (!frustums.empty()) {
    viewGroup.startNewFrame(*this, frameState);
    TileSelectionContext ctx{
        this->_options,
        this->_externals,
        this->_distances,
#pragma region das
        this->_childOcclusionProxies,
        this->_boundingVolumeBottomExtension};
#pragma endregion
    selectTiles(ctx, frameState, *pRootTile, result);
    viewGroup.finishFrame(*this, frameState);
  } else {
    result = ViewUpdateResult();
  }

  const std::shared_ptr<TileOcclusionRendererProxyPool>& pOcclusionPool =
      this->_externals.pTileOcclusionProxyPool;
  if (pOcclusionPool) {
    pOcclusionPool->pruneOcclusionProxyMappings();
  }

  this->_updateLodTransitions(frameState, deltaTime, result);

  return result;
}

void Tileset::loadTiles() {
  CESIUM_TRACE("Tileset::loadTiles");
  Tile* pRootTile = this->_pTilesetContentManager->getRootTile();
  if (!pRootTile) {
    // If the root tile is marked as ready, but doesn't actually exist, then
    // the tileset couldn't load. Fail any outstanding height requests.
    if (!this->_heightRequests.empty() && this->_pTilesetContentManager &&
        this->_pTilesetContentManager->getRootTileAvailableEvent().isReady()) {
      TilesetHeightRequest::failHeightRequests(
          this->_heightRequests,
          "Height requests could not complete because the tileset failed to "
          "load.");
    }
  } else {
    TilesetHeightRequest::processHeightRequests(
        this->getAsyncSystem(),
        *this->_pTilesetContentManager,
        this->_options,
#pragma region das
        this->_heightRequests,
        this->mbSampleHeightSmallestDetailed);
#pragma endregion
  }

#pragma region das
  this->mnCreatedTiles = 0;
  this->mnDestoryTiles = 0;
#pragma endregion
  this->_pTilesetContentManager->unloadCachedBytes(
      this->_options.maximumCachedBytes,
#pragma region das
      this->_options.tileCacheUnloadTimeLimit,
      this);
  if (this->mbEanbleLoad)
  {
    this->_pTilesetContentManager->processWorkerThreadLoadRequests(this->_options, this);
  }
  this->_pTilesetContentManager->processMainThreadLoadRequests(this->_options, this);
#pragma endregion
  for (const IntrusivePointer<ActivatedRasterOverlay>& pOverlay :
       this->_pTilesetContentManager->getRasterOverlayCollection()
           .getActivatedOverlays()) {
    pOverlay->tick();
  }
}

void Tileset::updateTileContent(Tile& tile) {
  this->_pTilesetContentManager->updateTileContent(tile, this->_options);
}

void Tileset::registerLoadRequester(TileLoadRequester& requester) {
  this->_pTilesetContentManager->registerTileRequester(requester);
}

bool Tileset::waitForAllLoadsToComplete(double maximumWaitTimeInMilliseconds) {
  return this->_pTilesetContentManager->waitUntilIdle(
      maximumWaitTimeInMilliseconds);
}

int32_t Tileset::getNumberOfTilesLoaded() const {
  return this->_pTilesetContentManager->getNumberOfTilesLoaded();
}

float Tileset::computeLoadProgress() noexcept {
  return this->_defaultViewGroup.getPreviousLoadProgressPercentage();
}

LoadedConstTileEnumerator Tileset::loadedTiles() const {
  return LoadedConstTileEnumerator(
      this->_pTilesetContentManager
          ? this->_pTilesetContentManager->getRootTile()
          : nullptr);
}

void Tileset::forEachLoadedTile(
    const std::function<void(const Tile& tile)>& callback) const {
  for (const Tile& tile : this->loadedTiles()) {
    callback(tile);
  }
}

int64_t Tileset::getTotalDataBytes() const noexcept {
  return this->_pTilesetContentManager->getTotalDataUsed();
}

const TilesetMetadata* Tileset::getMetadata(const Tile* pTile) const {
  if (pTile == nullptr) {
    pTile = this->getRootTile();
  }

  while (pTile != nullptr) {
    const TileExternalContent* pExternal =
        pTile->getContent().getExternalContent();
    if (pExternal)
      return &pExternal->metadata;
    pTile = pTile->getParent();
  }

  return nullptr;
}

CesiumAsync::Future<const TilesetMetadata*> Tileset::loadMetadata() {
  return this->getRootTileAvailableEvent().thenInMainThread(
      [pManager = this->_pTilesetContentManager,
       pAssetAccessor = this->_externals.pAssetAccessor,
       asyncSystem =
           this->getAsyncSystem()]() -> Future<const TilesetMetadata*> {
        Tile* pRoot = pManager->getRootTile();

        TileExternalContent* pExternal =
            pRoot ? pRoot->getContent().getExternalContent() : nullptr;
        if (!pExternal) {
          // Something went wrong while loading the root tile, so exit early.
          return asyncSystem.createResolvedFuture<const TilesetMetadata*>(
              nullptr);
        }

        TilesetMetadata& metadata = pExternal->metadata;
        if (!metadata.schemaUri) {
          // No schema URI, so the metadata is ready to go.
          return asyncSystem.createResolvedFuture<const TilesetMetadata*>(
              &metadata);
        }

        return metadata.loadSchemaUri(asyncSystem, pAssetAccessor)
            .thenInMainThread(
                [pManager, pAssetAccessor]() -> const TilesetMetadata* {
                  Tile* pRoot = pManager->getRootTile();
                  CESIUM_ASSERT(pRoot);

                  TileExternalContent* pExternal =
                      pRoot->getContent().getExternalContent();
                  if (!pExternal) {
                    return nullptr;
                  }
                  return &pExternal->metadata;
                });
      });
}

CesiumAsync::Future<SampleHeightResult>
Tileset::sampleHeightMostDetailed(const std::vector<Cartographic>& positions) {
  if (positions.empty()) {
    return this->_asyncSystem.createResolvedFuture<SampleHeightResult>({});
  }

  Promise promise = this->_asyncSystem.createPromise<SampleHeightResult>();

  std::vector<TilesetHeightQuery> queries;
  queries.reserve(positions.size());

  for (const CesiumGeospatial::Cartographic& position : positions) {
    queries.emplace_back(position, this->_options.ellipsoid);
  }

  this->_heightRequests.emplace_back(std::move(queries), promise);
  this->registerLoadRequester(this->_heightRequests.back());

  return promise.getFuture();
}

TilesetViewGroup& Tileset::getDefaultViewGroup() {
  return this->_defaultViewGroup;
}

const TilesetViewGroup& Tileset::getDefaultViewGroup() const {
  return this->_defaultViewGroup;
}

#pragma region das
int Tileset::getTileInLoadedQueue()
{
  int nTileLoading = this->_pTilesetContentManager->getNumberOfTilesLoading();
  return nTileLoading;

}

const ViewUpdateResult& Tileset::getViewUpdateResult()
{
  return this->_defaultViewGroup.getViewUpdateResult();
}

int Tileset::numCreateTile() {
  return mnCreatedTiles;
}

int Tileset::numDestoryTile() {
  return mnDestoryTiles; }

void Tileset::enableLoad(bool bEnable)
{ mbEanbleLoad = bEnable; }

void Tileset::processLoad() { this->_pTilesetContentManager->processWorkerThreadLoadRequests(this->_options, this); }

std::vector<Tileset::TileLoadTask>* Tileset::getLoadQueuePtr() {
  return &this->_defaultViewGroup._workerThreadLoadQueue;
}

std::vector<Tile*>* Tileset::getHeightQueryLoadQueuePtr() {
  this->mvecHeightQueryLoadQueue.clear();
  for (const TilesetHeightRequest& request : this->_heightRequests)
  {
    this->mvecHeightQueryLoadQueue.insert(this->mvecHeightQueryLoadQueue.end(), request.tilesToLoad.begin(), request.tilesToLoad.end());
  }
  return &this->mvecHeightQueryLoadQueue;
}

void Tileset::beginLoadAllTilesOnce()
{
  mbBeginLoad = true;
  msetLoadedTiles.clear();
}

void Tileset::updateLoadAllTilesOnce()
{
  if (!mbBeginLoad)
  {
    return;
  }

	//获取rootTile
	Tile* pRootTile = this->getRootTile();
  if (!pRootTile)
  {
    return;
  }

  std::vector<Tile*> vecTiles;
  int nMaxLoad = 8;
  int nNum = nMaxLoad - _pTilesetContentManager->getNumberOfTilesLoading();
  if (nNum <= 0)
  {
    return;
  }

	collectNotLoadTile(vecTiles, pRootTile, nNum);
	for (Tile* pTile : vecTiles)
	{
    Tile::Pointer pKeepAlive(pTile);
    _pTilesetContentManager->loadTileContent(*pTile, _options);
	}
}


bool Tileset::allChildLoaded(Tile* pTile)
{
  if (!pTile)
  {
    return true;
  }

	std::span<Tile> childs = pTile->getChildren();
  for (Tile& item : childs)
  {
    if (!allChildLoaded(&item))
    {
      return false;
    }
  }

	const TileID& id = pTile->getTileID();
	const std::string* pString = std::get_if<std::string>(&id);
  if (pString && msetLoadedTiles.find(*pString) == msetLoadedTiles.end())
  {
    return false;
  }

  return true;
}

bool Tileset::addNotLoadTileToLoad(std::vector<Tile*>& vecTiles, Tile* pTile, int nNeed)
{
  const TileID& id = pTile->getTileID();
  const std::string* pString = std::get_if<std::string>(&id);
  if (!pString)
  {
    return true;
  }

	if (msetLoadedTiles.find(*pString) != msetLoadedTiles.end())
	{
    //加载后直接卸载
    if (allChildLoaded(pTile))
    {
			_pTilesetContentManager->unloadTileContent(*pTile);
    }

    return true;
	}

  if (vecTiles.size() < static_cast<size_t>(nNeed))
  {
    vecTiles.push_back(pTile);
    msetLoadedTiles.insert(*pString);
    return true;
  }

  return false;
}

bool Tileset::collectNotLoadTile(std::vector<Tile*>& vecTiles, Tile* pRoot, int nNeed)
{
	//左、中、右的方式递归，push未加载的Tile到vecTiles
	if (pRoot == nullptr)
	{
		return false;
	}

  //pRoot获取所有child
  std::span<Tile> childs = pRoot->getChildren();
  for (Tile& item : childs)
  {
    collectNotLoadTile(vecTiles, &item, nNeed);
    if (vecTiles.size() == static_cast<size_t>(nNeed))
    {
      return true;
    }
  }

  addNotLoadTileToLoad(vecTiles, pRoot, nNeed);

  return true;
}

void Tileset::setBoundingVolumeBottomExtension(double extension)
{
	_boundingVolumeBottomExtension = extension;
}

double Tileset::getBoundingVolumeBottomExtension() const
{
	return _boundingVolumeBottomExtension;
}

bool Tileset::getLoadedTiles(std::vector<std::string>& vecTiles)
{
  if (!_pTilesetContentManager)
  {
    return false;
  }

  vecTiles = _pTilesetContentManager->mloadingTiles;

  return true;
}

void Tileset::clearLoadedTiles()
{
  _pTilesetContentManager->mloadingTiles.clear();
}

void Tileset::setPolygonProtection(const CesiumGeospatial::CartographicPolygon& polygon)
{
  // 进行值拷贝，使用原始多边形的顶点数据创建新的 CartographicPolygon 对象
  _polygonProtection = std::make_shared<CesiumGeospatial::CartographicPolygon>(polygon.getVertices());
}

void Tileset::clearPolygonProtection()
{
  _polygonProtection.reset();
}

std::shared_ptr<const CesiumGeospatial::CartographicPolygon> Tileset::getPolygonProtection() const
{
  return _polygonProtection;
}


void Tileset::addHeightPolygon(const CesiumGeospatial::DasHeightCartographicPolygon& polygon)
{
  // 添加多边形到数组
  if (_pTilesetContentManager)
  {
    _pTilesetContentManager->addHeightPolygon(polygon);
  }
}

void Tileset::clearHeightPolygons()
{
  if (_pTilesetContentManager)
  {
    _pTilesetContentManager->clearHeightPolygons();
  }
}

std::vector<CesiumGeospatial::DasHeightCartographicPolygon> Tileset::getHeightPolygons() const
{
  std::vector<CesiumGeospatial::DasHeightCartographicPolygon> vecPolygons;
  if (_pTilesetContentManager)
  {
    vecPolygons = _pTilesetContentManager->getHeightPolygons();
  }

  return vecPolygons;
}


#pragma endregion
} // namespace Cesium3DTilesSelection
