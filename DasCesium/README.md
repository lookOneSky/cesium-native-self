# DasCesium

Das自定义Cesium扩展库，提供增强的地形加载功能。

## 主要功能

### DasTerrainLoader

继承自 `EllipsoidTilesetLoader`，支持自定义地形选项。

#### 功能特性

- **高度偏移**：在生成椭球地形几何体时应用全局高度偏移
- **最小侵入**：通过继承方式扩展，不修改 Cesium 核心代码

#### 使用示例

```cpp
#include <DasCesium/DasTerrainLoader.h>
#include <DasCesium/DasOptions.h>

// 配置Das地形选项
DasCesium::DasTerrainOptionsExtra dasOptions;
dasOptions.dAddHeightMeter = 100.0; // 地形整体抬高100米

// 创建自定义地形Tileset
auto tileset = DasCesium::DasTerrainLoader::createTileset(
    externals,
    tilesetOptions,
    dasOptions
);
```

## 项目结构

```
DasCesium/
├── include/DasCesium/
│   ├── DasOptions.h          # Das自定义选项定义
│   └── DasTerrainLoader.h    # Das地形加载器头文件
├── src/
│   └── DasTerrainLoader.cpp  # Das地形加载器实现
└── CMakeLists.txt            # CMake配置文件
```

## API 文档

### DasTerrainOptionsExtra

```cpp
struct DasTerrainOptionsExtra {
  double dAddHeightMeter = 0.0; // 地形高度偏移量（米）
};
```

### DasTerrainLoader

```cpp
class DasTerrainLoader : public Cesium3DTilesSelection::EllipsoidTilesetLoader {
public:
  // 构造函数
  DasTerrainLoader(
      const CesiumGeospatial::Ellipsoid& ellipsoid,
      const DasTerrainOptionsExtra& dasOptions = DasTerrainOptionsExtra{});

  // 创建Tileset
  static std::unique_ptr<Cesium3DTilesSelection::Tileset> createTileset(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& options,
      const DasTerrainOptionsExtra& dasOptions = DasTerrainOptionsExtra{});

protected:
  // 重写几何体创建方法
  Geometry createGeometry(const Cesium3DTilesSelection::Tile& tile) const override;
};
```

## 对 Cesium 源码的修改

为了支持继承扩展，对 `EllipsoidTilesetLoader` 进行了最小化修改（已用 `#pragma region Das` 标注）：

1. 将关键方法从 `private` 改为 `protected` 和 `virtual`
2. 将 `Geometry` 结构体改为 `protected`

详见：`Cesium3DTilesSelection/include/Cesium3DTilesSelection/EllipsoidTilesetLoader.h`

## 编译

DasCesium 已集成到 cesium-unreal 的 CMake 构建系统中，无需额外配置。

## 许可证

本项目遵循 cesium-unreal 的许可证。
