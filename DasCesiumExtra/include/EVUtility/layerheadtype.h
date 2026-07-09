#pragma once
// #include <iostream>
namespace EVUtility {
namespace LayerHeadType{
/// <summary>
/// EarthView����֧�ֵ�ר��ͼ����
/// </summary>
enum EVSSCThemeType {
  /// <summary>
  /// ����δ֪
  /// </summary>
  SSC_ThemeType_Unknown = 0,
  /// <summary>
  /// ��ֵר��ͼ
  /// </summary>
  SSC_ThemeType_Unique = 1,
  /// <summary>
  /// ��Χר��ͼ
  /// </summary>
  SSC_ThemeType_Range = 2
};
/// <summary>
/// EarthView����֧�ֵ�ʸ�����ݼ�������
/// </summary>
enum EVSSCGeometryType {
  /// <summary>
  /// ����δ֪
  /// </summary>
  SSC_GeometryType_Unknown = 0,
  /// <summary>
  /// ��
  /// </summary>
  SSC_GeometryType_Point = 1,
  /// <summary>
  /// ��
  /// </summary>
  SSC_GeometryType_Line = 2,
  /// <summary>
  /// ��
  /// </summary>
  SSC_GeometryType_Ring = 3,
  /// <summary>
  /// ���
  /// </summary>
  SSC_GeometryType_MultiPoint = 11,
  /// <summary>
  /// ����
  /// </summary>
  SSC_GeometryType_Polyline = 12,
  /// <summary>
  /// ����
  /// </summary>
  SSC_GeometryType_Polygon = 13,
  /// <summary>
  /// ���
  /// </summary>
  SSC_GeometryType_Collection = 20
};
/// <summary>
/// EarthView����֧�ֵ�ͼ����������
/// </summary>
enum EVSSCLayerDataType {
  /// <summary>
  /// ����δ֪
  /// </summary>
  SSC_LayerDataType_Unknown = 0,
  /// <summary>
  /// �ನ��Ӱ��
  /// </summary>
  SSC_LayerDataType_Image_Multi = 1,
  /// <summary>
  /// ������Ӱ��
  /// </summary>
  SSC_LayerDataType_Image_Single = 2,
  /// <summary>
  /// ����DEM
  /// </summary>
  SSC_LayerDataType_DEM_int = 11,
  /// <summary>
  /// ������DEM
  /// </summary>
  SSC_LayerDataType_DEM_Float = 12,
  /// <summary>
  /// ����8λDEM
  /// </summary>
  SSC_LayerDataType_DEM_int8 = 13,
  /// <summary>
  /// ����16λDEM
  /// </summary>
  SSC_LayerDataType_DEM_int16 = 14,
  /// <summary>
  /// ����ʸ��ͼ��
  /// </summary>
  SSC_LayerDataType_Vector_Point = 21,
  /// <summary>
  /// ����ʸ��ͼ��
  /// </summary>
  SSC_LayerDataType_Vector_Polyline = 22,
  /// <summary>
  /// ����ʸ��ͼ��
  /// </summary>
  SSC_LayerDataType_Vector_Polygon = 23
};
/// <summary>
/// EarthView����֧�ֵ�����ϵ����
/// </summary>
enum EVSSCSRS {
  /// <summary>
  /// δ֪
  /// </summary>
  SSC_SRS_Unknown = 0,
  /// <summary>
  /// WGS84����ϵ
  /// </summary>
  SSC_SRS_WGS_1984 = 4326,
  /// <summary>
  /// ����54����ϵ
  /// </summary>
  SSC_SRS_Beijing_1954 = 4214,
  /// <summary>
  /// ����80����ϵ
  /// </summary>
  SSC_SRS_Xian_1980 = 4610,
  /// <summary>
  /// �й�2000����ϵ
  /// </summary>
  SSC_SRS_China_2000 = 4490,
  /// <summary>
  /// MercatorͶӰ
  /// </summary>
  SSC_SRS_Mercator = 3395,
  /// <summary>
  /// Mercator_WebͶӰ
  /// </summary>
  SSC_SRS_Mercator_Web = 102113
};
/// <summary>
/// EarthView����֧�ֵĻ���ģʽ
/// </summary>
enum EVSSCCacheMode {
  /// <summary>
  /// δ֪
  /// </summary>
  SSC_CacheMode_Unknown = 0,
  /// <summary>
  /// ��������Ͻ�
  /// </summary>
  SSC_CacheMode_LeftTop = 1,
  /// <summary>
  /// ��������½�
  /// </summary>
  SSC_CacheMode_LeftBottom = 2,
  /// <summary>
  /// ��������Ͻ�
  /// </summary>
  SSC_CacheMode_RightTop = 3,
  /// <summary>
  /// ��������½�
  /// </summary>
  SSC_CacheMode_RightBottom = 4,
  /// <summary>
  /// ���������
  /// </summary>
  SSC_CacheMode_Center = 5
};
/// <summary>
/// EarthView����֧�ֵĻ�����������
/// </summary>
enum EVSSCCacheDataType {
  /// <summary>
  /// Ӱ������
  /// </summary>
  SSC_CacheDataType_Image = 0,
  /// <summary>
  /// DEM����
  /// </summary>
  SSC_CacheDataType_DEM = 1,
  /// <summary>
  /// ʸ������
  /// </summary>
  SSC_CacheDataType_Vector = 2,
  /// <summary>
  /// ��ͼ����
  /// </summary>
  SSC_CacheDataType_Chart = 4,
  /// <summary>
  /// ����δ֪
  /// </summary>
  SSC_CacheDataType_Unknown = 100
};
/// <summary>
/// EarthView����֧�ֵ��ļ���ʽ
/// </summary>
enum EVSSCFileFormat {
  /// <summary>
  /// ��ʽδ֪
  /// </summary>
  SSC_FileFormat_Unknown = 0,
  /// <summary>
  /// JPG��ʽ
  /// </summary>
  SSC_FileFormat_JPG = 1,
  /// <summary>
  /// EVG��ʽ
  /// </summary>
  SSC_FileFormat_EVG = 2,
  /// <summary>
  /// DDS��ʽ
  /// </summary>
  SSC_FileFormat_DDS = 3,
  /// <summary>
  /// BMP��7zѹ����ʽ
  /// </summary>
  SSC_FileFormat_BMP_7Z = 100,
  /// <summary>
  /// ʸ����ʽ
  /// </summary>
  SSC_FileFormat_EVV = 101,
  /// <summary>
  /// ʸ����7zѹ����ʽ
  /// </summary>
  SSC_FileFormat_EVV_7Z = 102,
  /// <summary>
  /// PNG��ʽ
  /// </summary>
  SSC_FileFormat_PNG = 121,
  /// <summary>
  /// ʸ����Ƭ��ʽ
  /// </summary>
  SSC_FileFormat_EVP = 151,
  /// <summary>
  /// ʸ����Ƭ��7zѹ����ʽ
  /// </summary>
  SSC_FileFormat_EVP_7Z = 152,
  /// <summary>
  /// BIL��ʽ
  /// </summary>
  SSC_FileFormat_BIL = 201,
  /// <summary>
  /// BIL��7zѹ����ʽ
  /// </summary>
  SSC_FileFormat_BIL_7Z = 202,
  /// <summary>
  /// BIL��GZIPѹ����ʽ
  /// </summary>
  SSC_FileFormat_BIL_GZIP = 203,
  /// <summary>
  /// MVT��GZIPѹ����ʽ
  /// </summary>
  SSC_FileFormat_MVT_GZIP = 301
};
/// <summary>
/// EarthView����֧�ֵļ��ζ����ʽ
/// </summary>
enum EVSSCGeometryFormat {
  /// <summary>
  /// WKB��ʽ
  /// </summary>
  SSC_GeometryFomat_WKB = 0,
  /// <summary>
  /// WKT��ʽ
  /// </summary>
  SSC_GeometryFomat_WKT = 1
};

}
} // namespace Cesium3DTiles
