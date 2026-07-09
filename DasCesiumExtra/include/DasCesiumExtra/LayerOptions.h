#pragma once
#include "Library.h"
#include "EVUtility/layerheadtype.h"
#include"glm/ext/vector_double3.hpp"
#include <CesiumGeospatial/GlobeRectangle.h>
#include <string>
using namespace EVUtility::LayerHeadType;

namespace EVScene {

//TilesetOptions的地形参数同步到这里
struct TerrainTilesetOptions
{
	int maxTileLevel = 10;//手动设置的最大层级
};

struct EVScene_API DemlayerHeadinfo {
public:
  // EVSSCCacheDataType mnFileType;
  std::string mstrLayerName;
  EVSSCFileFormat mnFileFormat; // ѹ����ʽ
  std::string mstrDataTime;
  std::string mstrUpdateTime;
  std::string mstrName;
  uint32_t mnLevelNum;       // �㼶
  uint32_t mnSamplesPerTile; // ��Ƭ��С
  double mdfSpanForZeroLevel;
  double rectanglewest;          // ��Χ��
  double rectangleeast;          // ��Χ��
  double rectanglesouth;         // ��Χ��
  double rectanglenorth;         // ��Χ��
  EVSSCLayerDataType mnDataType; // ���ݸ�ʽ���縡����
  EVSSCSRS mnSRS;
  int32_t mnMaxDataLevel;
  int32_t mnMinDataLevel;
  // EVSSCCacheMode mnCacheMode;
  CesiumGeospatial::GlobeRectangle region;
  DemlayerHeadinfo() : region(CesiumGeospatial::GlobeRectangle::EMPTY) {
    mnMaxDataLevel = 0;
    mnMinDataLevel = 0;
    mnSamplesPerTile = 133;
    mnDataType = EVSSCLayerDataType::SSC_LayerDataType_Unknown;
    rectanglewest = 0.0;
    rectangleeast = 0.0;
    rectanglesouth = 0.0;
    rectanglenorth = 0.0;
  }
};
enum class EVScene_API LayerDataType {
  LDT_LocalFile,
  LDT_EVServer,
  LDT_TianDiTuServer
};

enum class EVScene_API LayerSchemeType {
  OGC_WebMercator,
  CGCS2000,
  LDT_TianDiTu
};

struct EVScene_API LayerOptions {
  std::string ID;
  std::string name;
  bool visible;
  LayerOptions() : ID(""), name(""), visible(true) {}
  LayerOptions(std::string _ID, std::string _name, bool _visible)
      : ID(_ID), name(_name), visible(_visible) {}
};

struct EVScene_API TerrainLayerOptions : public LayerOptions {
  TerrainLayerOptions()
      : LayerOptions(),
        url(""),
        dataType(LayerDataType::LDT_LocalFile),
        exaggerationScale(1.0),
        raiseHeight(0.0) {}
  TerrainLayerOptions(
      std::string _ID,
      std::string _name,
      std::string _url,
      bool _visible,
      LayerDataType _dataType)
      : LayerOptions(_ID, _name, _visible),
        url(_url),
        dataType(_dataType),
        exaggerationScale(1.0),
        raiseHeight(0.0) {}
  std::string url;
  LayerDataType dataType;
  DemlayerHeadinfo headInfo;
  std::string serviceName; //for web service
  float exaggerationScale
  float raiseHeight = 0;//抬高的高度
  bool bGeodeticHeight = false;//高程的类型：false标准高、true大地高
};
struct EVScene_API ImageLayerOptions : public LayerOptions {
  ImageLayerOptions()
      : LayerOptions(), url(""), dataType(LayerDataType::LDT_LocalFile) {}
  std::string url;
  LayerDataType dataType;
};
struct EVScene_API MvtVectorLayerOptions : public LayerOptions {
  MvtVectorLayerOptions()
      : LayerOptions(), url(""), dataType(LayerDataType::LDT_LocalFile) {}
  std::string url;
  LayerDataType dataType;
};
struct EVScene_API ModelLayerOptions : public LayerOptions {
  ModelLayerOptions()
      : LayerOptions(),
        url(""),
        serviceName(""),
        height(0),
        cullDistance(0),
        dataType(LayerDataType::LDT_LocalFile),
        layeroffset(glm::dvec3(0.0)),
        firstlevelshow(false),
        forceopaque(false) {}
  std::string url;
  std::string serviceName;
  float height;
  float cullDistance;
  LayerDataType dataType;
  glm::dvec3 layeroffset;
  bool firstlevelshow;
  bool forceopaque;
};
struct EVScene_API PlantLayerOptions : public LayerOptions {
  PlantLayerOptions()
      : LayerOptions(),
        url(""),
        serviceName(""),
        dataType(LayerDataType::LDT_LocalFile)
         {}
  std::string url;
  std::string serviceName;
  LayerDataType dataType;
};
}
