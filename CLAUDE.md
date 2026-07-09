# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

cesium-native uses CMake with VCPKG for dependency management. **When building for use with cesium-unreal, always build from the parent `extern/` directory, not this `cesium-native/` subdirectory.**

### Windows (x64) Build

From the `extern/` directory:
```bash
cmake -B build -S . -G "Visual Studio 17 2022" -T v142 \
  -DUNREAL_ENGINE_ROOT="C:\Soft\UnrealEngine\UE_5.3" \
  -DCMAKE_CXX_FLAGS="/MP /WX-"

cmake --build build --config Release --target install
cmake --build build --config Debug --target install
```

### Android Build (Cross-compile from Windows)

Prerequisites: Android Studio + NDK installed, Ninja installed (`choco install ninja`).

```bash
SET ANDROID_NDK_ROOT=C:/Android/ndk/r25c   # Use forward slashes

# From extern/ directory:
cmake -B build-android -S . -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="unreal-android-toolchain.cmake" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-android --config Release --target install
```

Use `Debug` or `RelWithDebInfo` instead of `Release` for debug builds.

### Code Generation (Node.js tools)

```bash
npm run generate-gltf                    # Regenerate glTF classes
npm run generate-3d-tiles                # Regenerate 3D Tiles classes
npm run generate-quantized-mesh-terrain  # Regenerate terrain classes
npm run format                           # Format all Cesium* source files
npm run check-format                     # Check formatting
npm run conform-includes                 # Validate include structure
```

## Architecture Overview

### Module Dependency Order

```
CesiumUtility → CesiumGltf → CesiumGeometry → CesiumGeospatial
CesiumJsonReader → CesiumJsonWriter
CesiumGltfContent → CesiumGltfReader → CesiumGltfWriter
CesiumAsync
Cesium3DTiles → Cesium3DTilesReader → Cesium3DTilesWriter → Cesium3DTilesContent
CesiumRasterOverlays → Cesium3DTilesSelection
CesiumClientCommon → CesiumIonClient → CesiumITwinClient
CesiumQuantizedMeshTerrain
DasCesium (custom extension)
```

### Key Module Roles

- **Cesium3DTilesSelection**: Core runtime tile streaming, LOD selection, frustum culling. The `TilesetContentLoader` interface is the primary extension point for custom loaders.
- **CesiumAsync**: `Future<T>` based multi-threaded task system — all async operations flow through here.
- **CesiumGeospatial**: ECEF coordinate system, ellipsoid math, cartographic conversions. Foundation for all geospatial positioning.
- **CesiumRasterOverlays**: Drapes 2D textures (WMS/TMS/WMTS) over 3D tile geometry.
- **CesiumQuantizedMeshTerrain** + **EllipsoidTilesetLoader**: Procedural terrain mesh generation.

### DasCesium Custom Extension

Located in `DasCesium/`, this is the project-specific terrain loader extending `EllipsoidTilesetLoader`:

- **`DasTerrainLoader`**: Overrides `createGeometry()`, `createBoundingRegion()`, and `loadTileContent()` to apply a global height offset (`DasTerrainOptionsExtra::dAddHeightMeter`) to terrain meshes.
- Factory method: `DasTerrainLoader::createTileset()` for easy instantiation.
- Generates 24×24 vertex grids per tile with indexed triangle meshes.
- Custom modifications in the base Cesium code are marked with `#pragma region Das` for easy identification.
- The base `EllipsoidTilesetLoader` was modified only to make key methods `protected virtual` to enable inheritance.

### VCPKG Platform Triplets

Triplets are auto-detected by `cmake/detect-vcpkg-triplet.cmake`:

| Platform | Triplet |
|---|---|
| Windows x64 | `x64-windows-static-md` |
| Android arm64 | `arm64-android` |
| Android x64 | `x64-android` |
| iOS | `arm64-ios` |
| macOS arm64 | `arm64-osx` |
| Linux x64 | `x64-linux` |

Custom Unreal-specific triplets are in `../vcpkg-overlays/triplets/` (e.g., `arm64-android-unreal.cmake`).

## Running Tests

Tests use the [doctest](https://github.com/doctest/doctest) framework in `CesiumNativeTests/`.

```bash
# Build and run tests (from build directory)
cmake --build build --target cesium-native-tests
./build/cesium-native/CesiumNativeTests/cesium-native-tests
```

## Code Generation Pattern

glTF, 3D Tiles, and quantized-mesh classes are auto-generated from JSON schemas via `tools/generate-classes/`. Do not manually edit generated files — run the appropriate `npm run generate-*` script instead and commit the output.
