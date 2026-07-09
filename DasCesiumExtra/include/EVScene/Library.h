#pragma once

/**
 * @brief Classes that implement the 3D Tiles standard
 */
namespace EVScene {}

#if defined(_WIN32) && defined(CESIUM_SHARED)
#ifdef EVScene_BUILDING
#define EVScene_API __declspec(dllexport)
#else
#define EVScene_API __declspec(dllimport)
#endif
#else
#define EVScene_API
#endif
