#include <CesiumGeospatial/Das/DasCartographicPolygon.h>

namespace CesiumGeospatial {

DasHeightCartographicPolygon::DasHeightCartographicPolygon()
    : _polygon(std::vector<glm::dvec2>()),
      mdHeight(0.0) {
  this->_polygon.eEditType = EditType::Edit_Height;
}

DasHeightCartographicPolygon::DasHeightCartographicPolygon(
    const std::vector<glm::dvec2>& polygon,
    double height)
    : _polygon(polygon),
      mdHeight(height) {
  this->_polygon.eEditType = EditType::Edit_Height;
  // Convert height from meters to centimeters for internal storage
  this->_polygon.fSameHeightCM = static_cast<float>(height * 100.0);
}

void DasHeightCartographicPolygon::setPolygon(const std::vector<glm::dvec2>& polygon) {
  this->_polygon = CartographicPolygon(polygon);
  this->_polygon.eEditType = EditType::Edit_Height;
  // Update the height in centimeters
  this->_polygon.fSameHeightCM = static_cast<float>(this->mdHeight * 100.0);
}

} // namespace CesiumGeospatial
