#pragma once

namespace DasCesium {

/**
 * @brief Das自定义地形选项扩展
 */
struct DasTerrainOptionsExtra {
  /**
   * @brief 地形高度偏移量（米）
   * 在生成地形几何体时，会在基础椭球表面上增加此高度值
   */
  double dAddHeightMeter = 0.0;
};

} // namespace DasCesium
