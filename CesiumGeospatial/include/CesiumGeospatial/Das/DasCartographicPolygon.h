#pragma once

#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/Library.h>

#include <glm/vec2.hpp>

#include <vector>

namespace CesiumGeospatial {

/**
 * @brief Extended cartographic polygon with height information for terrain editing.
 *
 * This class wraps a CartographicPolygon and adds height-based editing capabilities.
 * It can be used to modify terrain elevation within the polygon boundaries.
 */
class CESIUMGEOSPATIAL_API DasHeightCartographicPolygon {
public:
  /**
   * @brief Default constructor.
   */
  DasHeightCartographicPolygon();

  /**
   * @brief Constructs a height-based cartographic polygon.
   *
   * @param polygon An array of longitude-latitude points in radians defining
   * the perimeter of the 2D polygon.
   * @param height The height value in meters to apply within the polygon.
   */
  DasHeightCartographicPolygon(
      const std::vector<glm::dvec2>& polygon,
      double height);

  /**
   * @brief Returns the underlying CartographicPolygon.
   *
   * @return Reference to the CartographicPolygon.
   */
  constexpr const CartographicPolygon& getPolygon() const {
    return this->_polygon;
  }

  /**
   * @brief Returns the underlying CartographicPolygon (non-const).
   *
   * @return Reference to the CartographicPolygon.
   */
  constexpr CartographicPolygon& getPolygon() {
    return this->_polygon;
  }

  /**
   * @brief Sets the polygon vertices.
   *
   * @param polygon An array of longitude-latitude points in radians defining
   * the perimeter of the 2D polygon.
   */
  void setPolygon(const std::vector<glm::dvec2>& polygon);

  /**
   * @brief Returns the height value in meters.
   *
   * @return The height value.
   */
  constexpr double getHeight() const {
    return this->mdHeight;
  }

  /**
   * @brief Sets the height value in meters.
   *
   * @param height The new height value.
   */
  void setHeight(double height) {
    this->mdHeight = height;
    // Convert height from meters to centimeters for internal storage
    this->_polygon.fSameHeightCM = static_cast<float>(height * 100.0);
  }

  /**
   * @brief Returns the edit type.
   *
   * @return The edit type.
   */
  constexpr EditType getEditType() const {
    return this->_polygon.eEditType;
  }

  /**
   * @brief Sets the edit type.
   *
   * @param editType The new edit type.
   */
  void setEditType(EditType editType) {
    this->_polygon.eEditType = editType;
  }

private:
  CartographicPolygon _polygon; // The underlying polygon
  double mdHeight = 0.0; // Height in meters
};

} // namespace CesiumGeospatial
