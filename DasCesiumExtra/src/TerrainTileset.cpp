#include "EVScene/TerrainTileset.h"
#include "TilesetContentManager.h"
#include "Cesium3DTilesSelection/RasterOverlayCollection.h"
#include "EVScene/TerrainTilesetContentManager.h"
#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>

namespace EVScene {
/**
* 地形tileset
*/
TerrainTileset::TerrainTileset(
    const Cesium3DTilesSelection::TilesetExternals& externals,
    std::vector<TerrainLayerOptions>& layerOptions,
    const Cesium3DTilesSelection::TilesetOptions& options,
    const TerrainTilesetOptions& terrainTilesetOption)
    : Cesium3DTilesSelection::Tileset(externals, "", options) {
  _pTilesetContentManager = new TerrainTilesetContentManager(
      externals,
      options,
      Cesium3DTilesSelection::RasterOverlayCollection{
          this->_loadedTiles,
          externals},
      layerOptions,
      terrainTilesetOption);
}

TerrainTileset::~TerrainTileset() {
  
}

CesiumGeospatial::GlobeRectangle
TerrainTileset::getLayRectangle(std::string layername) {
  TerrainTilesetContentManager* terrainLoder =
      dynamic_cast<TerrainTilesetContentManager*>(
          _pTilesetContentManager.get());
  return terrainLoder->getLayRectangle(layername);
}

//CesiumAsync::Future<std::vector<CesiumGeospatial::Cartographic>>
//TerrainTileset::sampleTerrainMostDetailed(
//    std::vector<CesiumGeospatial::Cartographic>& positons) {
//  TerrainTilesetContentManager* terrainLoder =
//      dynamic_cast<TerrainTilesetContentManager*>(
//          _pTilesetContentManager.get());
//
//  return terrainLoder->sampleTerrainMostDetailed(positons);
//}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>
TerrainTileset::getTileContent(
    const Cesium3DTilesSelection::TileLoadInput& loadInput) {
  TerrainTilesetContentManager* terrainLoder =
      dynamic_cast<TerrainTilesetContentManager*>(
          _pTilesetContentManager.get());
  return terrainLoder->getTileContent(loadInput);
}

bool TerrainTileset::IsLoaderCreated()
{
  auto pContentManger = (TerrainTilesetContentManager*)(_pTilesetContentManager.get());
  return pContentManger->IsLoaderCreated();
}

bool TerrainTileset::IsImageLayerAdded() {
  for (auto provider : this->getOverlays().getTileProviders()) {
    provider->isPlaceholder();
  }
  return std::all_of(
      this->getOverlays().getTileProviders().begin(),
      this->getOverlays().getTileProviders().end(),
      [](const CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::RasterOverlayTileProvider>&
             pProvider) {
        return !pProvider->isPlaceholder();
    });
           
}

CesiumAsync::Future<std::vector<float>>
TerrainTileset::getTileHeightData(const Cesium3DTilesSelection::Tile& tile) {
  TerrainTilesetContentManager* pTerrainTilesetContentManager =
      dynamic_cast<TerrainTilesetContentManager*>(
          _pTilesetContentManager.get());
  return pTerrainTilesetContentManager->getTileHeightData(tile,this->getOptions());
}

CesiumAsync::Future<bool>
TerrainTileset::addLayer(TerrainLayerOptions& layerOptions) {

  TerrainTilesetContentManager* pTerrainTilesetContentManager =
      dynamic_cast<TerrainTilesetContentManager*>(
          _pTilesetContentManager.get());
  return pTerrainTilesetContentManager->addLayer(
      this->getExternals(),
      this->getOptions(),
      layerOptions);
}

void TerrainTileset::removeLayer(const std::string& layername) {
  TerrainTilesetContentManager* pTerrainTilesetContentManager =
      dynamic_cast<TerrainTilesetContentManager*>(
          _pTilesetContentManager.get());
  return pTerrainTilesetContentManager->removeLayer(
      this->getExternals(),
      this->getOptions(),
      layername);
}

void TerrainTileset::setTerrainAddHeight(float fAddHeight)
{
  TerrainTilesetContentManager* pContentManger = dynamic_cast<TerrainTilesetContentManager*>(_pTilesetContentManager.get());
	if (!pContentManger)
	{
		return;
	}
	pContentManger->mfAddHeight = fAddHeight;
}

float TerrainTileset::getTerrainAddHeight()
{
	TerrainTilesetContentManager* pContentManger = dynamic_cast<TerrainTilesetContentManager*>(_pTilesetContentManager.get());
	if (!pContentManger)
	{
		return 0;
	}
	return pContentManger->mfAddHeight;
}

} // namespace EVScene
