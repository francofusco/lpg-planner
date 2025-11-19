#ifndef MATH_UTILITIES_H
#define MATH_UTILITIES_H


#include <Eigen/Dense>

namespace math_utilities {

/// Transform a metric distance into a latitude difference.
/** Given a distance, return the change in latitude corresponding to it.
  * @param distance_km A sitance, in km.
  * @return A latitude variation that corresponds to the given distance.
  */
double latitude_variation(double distance_km);

/// Transform a metric distance into a longitude difference.
/** Given a distance and a latitude, return the change in longitude
  * corresponding to it.
  * @param distance_km A sitance, in km.
  * @param latitude Latitude at which the longitude variation is to be
  *   calculated.
  * @return A longitude variation that corresponds to the given distance.
  */
double longitude_variation(double distance_km, double latitude);

/// Load an array saved using NumPy's savetxt() function.
/** Note that this function expects the shape to be (n, 2), where n will be
  * "deduced", but the 2 is hard-coded.
  * @param filename Path to the file containing the array.
  * @return The array contained in the file.
  */
Eigen::ArrayXXd loadArray(const std::string& filename);


// Calculate the distance between GPS coordinates.
/** This function calculates the distance between the given GPS coordinates.
  * It leverages Eigen's parallelization to allow computing multiple distances
  * at once.
  * @param lat1 1D array of latitudes.
  * @param lon1 1D array of longitudes.
  * @param lat2 1D array of latitudes.
  * @param lon2 1D array of longitudes.
  * @return An array with the same shape as the inputs, such that the i-th
  *   entry is the distance between the points defined by (lat1(i), lon1(i))
  *   and (lat2(i), lon2(i)).
  */
template <class D1, class D2, class D3, class D4>
auto haversineDistance(
  const Eigen::ArrayBase<D1>& lat1,
  const Eigen::ArrayBase<D2>& lon1,
  const Eigen::ArrayBase<D3>& lat2,
  const Eigen::ArrayBase<D4>& lon2
);


/// Calculate the distance between GPS coordinates.
/** This overloaded version allows to calculate the distance between a set of
  * points from a single point.
  * @see haversineDistance()
  * @param lat1 1D array of latitudes.
  * @param lon1 1D array of longitudes.
  * @param lat2 A latitude.
  * @param lon2 A longitude.
  * @return An array with the same shape as the first inputs, such that the
  *   i-th entry is the distance between the points defined by
  *   (lat1(i), lon1(i)) and (lat2, lon2).
  */
template <class D1, class D2>
auto haversineDistance(
  const Eigen::ArrayBase<D1>& lat1,
  const Eigen::ArrayBase<D2>& lon1,
  double lat2,
  double lon2
);



/// Return the array that would order the input.
/** Given an input array, return the sequence s = (s0, s1, s2, ...) such that
  * the sequence (array(s0), array(s1), array(s2), ...) is sorted in ascending
  * order.
  *
  * To sort the input array, use:
  * ```
  * auto order = argsort(array);
  * sortBy(array, order);
  * ```
  * @see sortBy()
  * @param array A 1D array.
  * @return A list of indices which would sort the input.
  */
template<class Derived>
std::vector<Eigen::Index> argsort(
  const Eigen::ArrayBase<Derived>& array
);

/// Sort an array according to the given ordering.
/** Given an input array, and the sequence idx = (i0, i1, i2, ...), reorder the
  * input array as (array(i0), array(i1), array(i2), ...).
  * @see argsort()
  * @param[in, out] array A 1D or 2D array. It is modified in-place.
  * @param[in] idx List of indices telling how to sort the input.
  */
template<class Derived>
void sortBy(
  Eigen::ArrayBase<Derived>& source,
  const std::vector<Eigen::Index>& idx
);


} // namespace eigen_utilities

#endif // MATH_UTILITIES_H

#include "math_utilities.hxx"
