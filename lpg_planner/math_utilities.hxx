#pragma once

#include "math_utilities.hpp"
#include <fstream>


namespace math_utilities {

// Constants, to avoid magic numbers.
constexpr double EARTH_RADIUS_KM = 6371.0;
constexpr double TO_DEG = (180 / M_PI);
constexpr double TO_RAD = (M_PI / 180);


double latitude_variation(
  double distance_km
)
{
  return TO_DEG * (distance_km / EARTH_RADIUS_KM);
}


double longitude_variation(
  double distance_km,
  double latitude
)
{
  return 2 * TO_DEG * std::asin( std::sin(distance_km/(2*EARTH_RADIUS_KM)) / std::cos(TO_RAD * latitude) );
}


Eigen::ArrayXXd loadArray(const std::string& filename)
{
  // Open the file.
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }

  // Prepare to read data.
  std::vector<std::array<double, 2>> rows;
  std::string line;

  // Read the file line by line.
  while(std::getline(file, line)) {
    std::istringstream iss(line);
    double a, b;
    if (!(iss >> a >> b)) {
      throw std::runtime_error("Invalid line in file: " + line);
    }
    rows.push_back({a, b});
  }

  // Copy the data into an Eigen::Array.
  Eigen::ArrayXXd result(rows.size(), 2);
  for (size_t i = 0; i < rows.size(); ++i) {
    result(i, 0) = rows[i][0];
    result(i, 1) = rows[i][1];
  }
  return result;
}


template <class D1, class D2, class D3, class D4>
auto haversineDistance(
  const Eigen::ArrayBase<D1>& lat1,
  const Eigen::ArrayBase<D2>& lon1,
  const Eigen::ArrayBase<D3>& lat2,
  const Eigen::ArrayBase<D4>& lon2
  )
{
  // Convert to radians.
  auto lat1r = TO_RAD * lat1.array();
  auto lat2r = TO_RAD * lat2.array();

  // Store differences.
  auto dlat = lat1r - lat2r;
  auto dlon = TO_RAD * (lon1.array() - lon2.array());

  // Calculate the haversine.
  auto a =
    ( (dlat / 2).sin().square() ) +
    ( lat1r.cos() * lat2r.cos() *
      ( (dlon / 2).sin().square() ) );

  // Return the distance from the haversine.
  return 2.0 * EARTH_RADIUS_KM * a.sqrt().asin();
}


// Scalar version of the above.
template <class D1, class D2>
auto haversineDistance(
  const Eigen::ArrayBase<D1>& lat1,
  const Eigen::ArrayBase<D2>& lon1,
  double lat2,
  double lon2
  )
{
  // Use a little cheat: if X is an array, then (X*0 + d) is an expression that
  // has the same dimension as X and represents an array filled with the value
  // 'd'. However, thanks to lazy evaluation, it should not require any memory
  // allocation!
  return haversineDistance(
    lat1,
    lon1,
    (lat1*0 + lat2),
    (lon1*0 + lon2)
  );
}


template<class Derived>
std::vector<Eigen::Index> argsort(
  const Eigen::ArrayBase<Derived>& array
)
{
  // Create the sorting vector.
  std::vector<Eigen::Index> sorted_idx(array.size());
  for(Eigen::Index i=0; i<sorted_idx.size(); ++i) {
    sorted_idx[i] = i;
  }

  // Define sorting based on the content of the array.
  std::sort(
    sorted_idx.begin(),
    sorted_idx.end(),
    [&](Eigen::Index i1, Eigen::Index i2) { return array(i1) < array(i2); }
  );

  return sorted_idx;
}


template<class Derived>
void sortBy(
  Eigen::ArrayBase<Derived>& source,
  const std::vector<Eigen::Index>& idx
)
{
  eigen_assert(idx.size() == source.rows() || "sortBy(source, idx): CANNOT SORT ARRAY WITH source.rows() != idx.size()");
  Eigen::Array<typename Derived::Scalar, Eigen::Dynamic, Derived::ColsAtCompileTime> copy(source);

  for(unsigned int i=0; i<idx.size(); ++i) {
    source.row(i) = copy.row(idx[i]);
  }
}

} // namespace math_utilities
