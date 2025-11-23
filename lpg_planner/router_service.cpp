#include "router_service.hpp"

#include <QGeoCoordinate>


RouterService::RouterService(
  DatabaseManager* database,
  QObject* parent
) : QObject(parent)
  , database_(database)
{
  // Nothing to do here.
}


bool RouterService::path(
  const QList<double>& waypoints_latitudes,
  const QList<double>& waypoints_longitudes,
  QList<double>& path_latitudes,
  QList<double>& path_longitudes
)
{
  // The input coordinates must have the same length.
  if(waypoints_latitudes.size() != waypoints_longitudes.size()) {
    qDebug() << "Bad inputs passed to RouterService::path()";
    return false;
  }

  // If there are less than two points, there is no path to be added.
  if(waypoints_latitudes.size() < 2) {
    return true;
  }

  // The path will be created by defining small segments whose length is at
  // most RESOLUTION_KM.
  constexpr double RESOLUTION_KM = 5.0;

  // Perform linear interpolation on consecutive waypoints.
  for(unsigned int i=0; i<waypoints_latitudes.size()-1; i++) {
    // Calculate the distance between a pair of consecutive waypoints.
    double distance = 1e-3 * QGeoCoordinate(
      {waypoints_latitudes[i], waypoints_longitudes[i]}
    ).distanceTo(QGeoCoordinate(
      {waypoints_latitudes[i+1], waypoints_longitudes[i+1]}
    ));

    // Evaluate how many points need to be generated in between.
    unsigned int n_points = 1 + (unsigned int)(std::ceil(distance / RESOLUTION_KM));

    // Using linear interpolation, add the intermediate points. Skip the last
    // one (k=n_points), since it corresponds to point i+1, which will be
    // added at the next iteration.
    for(unsigned int k=0; k<n_points; k++) {
      double rho = ((double)k) / n_points;
      path_latitudes.append(waypoints_latitudes[i] * (1-rho) + waypoints_latitudes[i+1] * rho);
      path_longitudes.append(waypoints_longitudes[i] * (1-rho) + waypoints_longitudes[i+1] * rho);
    }
  }

  // Add the very last waypoint, since during linear interpolation we always
  // skip the last point of the segment.
  path_latitudes.append(waypoints_latitudes.back());
  path_longitudes.append(waypoints_longitudes.back());
  return true;
}


bool RouterService::path(
  const QList<int>& waypoints_ids,
  QList<double>& path_latitudes,
  QList<double>& path_longitudes
)
{
  // Try to fetch the coordinates from their IDs.
  QList<double> waypoints_latitudes, waypoints_longitudes;
  if(!database_->stationsFromIds(waypoints_ids, nullptr, &waypoints_latitudes, &waypoints_longitudes, nullptr, nullptr)) {
    qDebug() << "Failed to retrive coordinates from the database";
    return false;
  }

  // Forward the call to the other overload.
  return path(
    waypoints_latitudes,
    waypoints_longitudes,
    path_latitudes,
    path_longitudes
  );
}


bool RouterService::distanceMatrix(
  const QList<double>& latitudes,
  const QList<double>& longitudes,
  QList<QList<double>>& distances
)
{
  // The input coordinates must have the same length.
  if(latitudes.size() != longitudes.size()) {
    return false;
  }

  // No need to do anything unless we have two or more locations!
  if(latitudes.size() == 0) {
    distances.resize(0);
    return true;
  }

  // If there is just one point, the distance matrix is trivial.
  if(latitudes.size() == 1) {
    distances.resize(1);
    distances[0] = {};
    return true;
  }

  // Prepare the distance matrix.
  distances.resize(latitudes.size());
  for(unsigned int i=0; i<latitudes.size(); i++) {
    distances[i].resize(latitudes.size());
    distances[i][i] = 0;
  }

  // Fill the matrix with distances.
  for(unsigned int i=0; i<latitudes.size()-1; i++) {
    for(unsigned int j=i+1; j<latitudes.size(); j++) {
      double d = 1e-3 * QGeoCoordinate({latitudes[i], longitudes[i]}).distanceTo(QGeoCoordinate({latitudes[j], longitudes[j]}));
      distances[i][j] = d;
      distances[j][i] = d;
    }
  }
  return true;
}


bool RouterService::distanceMatrix(
  const QList<int>& ids,
  QList<QList<double>>& distances
)
{
  // No need to do anything unless we have two or more locations!
  if(ids.size() == 0) {
    distances.resize(0);
    return true;
  }

  // If there is just one point, the distance matrix is trivial.
  if(ids.size() == 1) {
    distances.resize(1);
    distances[0] = {};
    return true;
  }

  // Resize the distance matrix, so it is ready for use.
  distances.resize(ids.size());
  for(unsigned int i=0; i<ids.size(); i++) {
    distances[i].resize(ids.size());
    distances[i][i] = 0;
    for(unsigned int j=0; j<ids.size(); j++) {
      distances[i][j] = i==j ? 0.0 : -1.0;
    }
  }

  // Retrieve all existing distance pairs from the database.
  QMap<QPair<int,int>, double> cached_distances;
  if(!database_->distancePairs(ids, cached_distances)) {
    qDebug() << "Cannot calculate distance matrix: failed to fetch distance pairs from the database";
    return false;
  }

  // Create an inverse map from IDs to indices.
  QMap<int,int> idx;
  for(unsigned int i=0; i<ids.size(); i++) {
    idx[ids[i]] = i;
  }

  // Fill the matrix with existig distances.
  for(const auto& [p, d] : cached_distances.asKeyValueRange()) {
    distances[idx[p.first]][idx[p.second]] = d;
  }

  // Create a list of missing distance pairs (i,j) - using the indices of the
  // missing elements in the distance matrix. Also build the set of all indices
  // that appear at least in one pair.
  QList<QPair<int, int>> missing_pairs;
  QSet<int> missing_set;
  for(unsigned int i=0; i<ids.size(); i++) {
    for(unsigned int j=0; j<ids.size(); j++) {
      if(distances[i][j] < 0) {
        missing_pairs.append({i, j});
        missing_set.insert(i);
        missing_set.insert(j);
      }
    }
  }

  // If we found every entry, we can leave.
  if(missing_set.size() == 0) {
    qDebug() << "Distances were all cached in the database";
    return true;
  }

  qDebug() << "Missing distances for" << missing_pairs.size() << "pairs";

  // Transform the set into a list and obtain the inverse map.
  QList<int> missing_list = missing_set.values();
  QList<int> missing_ids(missing_list.size());
  idx.clear();
  for(unsigned int i=0; i<missing_list.size(); i++) {
    idx[missing_list[i]] = i;
    missing_ids[i] = ids[missing_list[i]];
  }

  // Given the missing IDs, obtain their coordinates.
  QList<double> latitudes, longitudes;
  if(!database_->stationsFromIds(missing_ids, nullptr, &latitudes, &longitudes, nullptr, nullptr)) {
    qDebug() << "Cannot calculate distance matrix: failed to fetch coordinates from the database";
    return false;
  }

  // Calculate the distance matrix for the missing pairs.
  QList<QList<double>> missing_distances;
  if(!distanceMatrix(latitudes, longitudes, missing_distances)) {
    qDebug() << "Cannot calculate distance matrix: failed to calculate distances for missing pairs";
    return false;
  }

  // Copy missing values in the distance matrix and in a map.
  cached_distances.clear();
  for(const auto& p : missing_pairs) {
    const auto& d = missing_distances[idx[p.first]][idx[p.second]];
    distances[p.first][p.second] = d;
    cached_distances[{ids[p.first], ids[p.second]}] = d;
  }

  // Cache the missing values for future use.
  if(!database_->insertPairs(cached_distances)) {
    qDebug() << "Failed to save distance pairs into the database";
    return false;
  }

  return true;
}
