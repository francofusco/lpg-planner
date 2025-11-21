#include "lpg_planner.hpp"

#include "math_utilities.hpp"

#include <Eigen/Dense>
#include <EigenOpt/simplex.hpp>
#include <iostream>


LpgPlanner::LpgPlanner(
  RouterService* router,
  DatabaseManager* database,
  QObject *parent
) : QObject{parent}
  , router_(router)
  , database_(database)
{

}


void LpgPlanner::exportPath(
  const QList<double>& latitudes,
  const QList<double>& longitudes
)
{
  if(latitudes.isEmpty() || longitudes.isEmpty() || latitudes.size() != longitudes.size()) {
    qDebug() << "ERROR: latitudes and longitudes are not ok";
    return;
  }

  // Compute bounding box.
  double min_lat = 90.0;
  double max_lat = -90.0;
  double min_lon = 180.0;
  double max_lon = -180.0;

  for(unsigned int i=0; i<latitudes.size(); i++) {
    min_lat = qMin(min_lat, latitudes[i]);
    max_lat = qMax(max_lat, latitudes[i]);
    min_lon = qMin(min_lon, longitudes[i]);
    max_lon = qMax(max_lon, longitudes[i]);
  }

  // Approximate zoom level.
  constexpr double zoom_margin = 2.0;
  int zoom_latitude = static_cast<int>(qFloor(std::log2(zoom_margin * 360.0 / (max_lat - min_lat))));
  int zoom_longitude = static_cast<int>(qFloor(std::log2(zoom_margin * 360.0 / (max_lon - min_lon))));
  int zoom = qMin(zoom_latitude, zoom_longitude);
  zoom = qBound(0, zoom, 18);

  // Save path as QVariantList for QML.
  QVariantList path_variant;
  for(unsigned int i=0; i<latitudes.size(); i++) {
    QVariantMap map;
    map["latitude"] = latitudes[i];
    map["longitude"] = longitudes[i];
    path_variant.append(map);
  }

  // New center of the map.
  QVariantMap center_map{
    {"latitude", (min_lat + max_lat)/2.0},
    {"longitude", (min_lon + max_lon)/2.0}
  };

  // Notify other components.
  emit pathUpdated(path_variant, center_map, zoom);
}


void LpgPlanner::exportStations(
  const QList<double>& latitudes,
  const QList<double>& longitudes
)
{
  QList<bool> stop(latitudes.size());
  for(unsigned int i=0; i<latitudes.size(); i++)
    stop[i] = true;
  exportStations(latitudes, longitudes, stop);
}


void LpgPlanner::exportStations(
  const QList<double>& latitudes,
  const QList<double>& longitudes,
  const QList<bool>& stop
)
{
  if(latitudes.isEmpty() || longitudes.isEmpty() || latitudes.size() != longitudes.size()) {
    qDebug() << "ERROR: latitudes and longitudes are not ok";
    return;
  }

  // Save coordinates as QVariantList for QML.
  QVariantList coordinates;
  for(unsigned int i=0; i<latitudes.size(); i++) {
    coordinates.append(
      QVariantMap{
        {"latitude", latitudes[i]},
        {"longitude", longitudes[i]},
        {"stop", stop[i]}
      }
    );
  }

  // Notify other components.
  emit stationsUpdated(coordinates);
}


void LpgPlanner::solve(
  LpgProblem problem
)
{
  qDebug() << "Received request to find route from (" << problem.departure_latitude << "," << problem.departure_longitude << ")  to  (" << problem.arrival_latitude << "," << problem.arrival_longitude << ")";

  if(!problem.isValid()) {
    QString err;
    problem.isValid(err);
    emit failed(QString("Cannot solve invalid problem: %1").arg(err));
    return;
  }

  // Calculate the path from departure to arrival.
  QList<double> path_latitudes_qlist, path_longitudes_qlist;
  bool ok = router_->calculatePath(
    problem.departure_latitude,
    problem.departure_longitude,
    problem.arrival_latitude,
    problem.arrival_longitude,
    path_latitudes_qlist,
    path_longitudes_qlist
  );

  if(!ok) {
    emit failed(QString("Failed to find path from departure to arrival"));
    return;
  }

  // Show the path on a map.
  exportPath(path_latitudes_qlist, path_longitudes_qlist);

  // qDebug() << "WARNING: LOADING PATH FROM TEXT FOR DEBUGGING PURPOSES!";
  // Eigen::ArrayXXd path_coordinates = math_utilities::loadArray("C:/Users/franc/AppData/Roaming/lpg_planner/sample-path.txt");
  // auto path_latitudes = path_coordinates.col(0);
  // auto path_longitudes = path_coordinates.col(1);
  Eigen::Map<const Eigen::VectorXd> path_latitudes_map(path_latitudes_qlist.data(), path_latitudes_qlist.size());
  Eigen::Map<const Eigen::VectorXd> path_longitudes_map(path_longitudes_qlist.data(), path_longitudes_qlist.size());
  auto path_latitudes = path_latitudes_map.array();
  auto path_longitudes = path_longitudes_map.array();

  // Find stations that are in a selected area of interest.
  double min_latitude = path_latitudes.minCoeff();
  double max_latitude = path_latitudes.maxCoeff();
  double min_longitude = path_longitudes.minCoeff();
  double max_longitude = path_longitudes.maxCoeff();
  double latitude_margin = math_utilities::latitude_variation(problem.search_distance);
  double longitude_margin = math_utilities::longitude_variation(
    problem.search_distance,
    std::max(std::abs(min_latitude), std::abs(max_latitude))
  );

  // Create a filter to select only a subset of all possible stations.
  DatabaseManager::Filter db_filter;
  db_filter.setGPSRange(
    min_latitude - 2*latitude_margin,
    max_latitude + 2*latitude_margin,
    min_longitude - 2*longitude_margin,
    max_longitude + 2*longitude_margin
  );
  db_filter.setPriceRange(0.1, 2.0); // TODO: Make these a parameter!
  // TODO: select only stations that have a recent update.

  QList<int> stations_ids;
  QList<double> stations_prices, stations_latitudes, stations_longitudes;
  ok = database_->findStations(
    db_filter,
    &stations_ids,
    &stations_prices,
    &stations_latitudes,
    &stations_longitudes,
    nullptr,
    nullptr
  );

  if(!ok) {
    emit failed("Failed to access database");
    return;
  }

  if(stations_ids.size() == 0) {
    emit failed("Could not find any station between the departure and the arrival");
    return;
  }

  qDebug() << "Selected " << stations_ids.size() << "stations 'near' path";

  qDebug() << "Calculating path distances";
  auto path_distances = math_utilities::haversineDistance(
    path_latitudes.head(path_latitudes.rows()-1),
    path_longitudes.head(path_longitudes.rows()-1),
    path_latitudes.tail(path_latitudes.rows()-1),
    path_longitudes.tail(path_longitudes.rows()-1)
  );

  qDebug() << "Calculating arclengths";
  Eigen::ArrayXd path_arclength(path_distances.size() + 1);
  path_arclength(0) = 0;
  for(unsigned int i=1; i<path_arclength.size(); i++) {
    path_arclength(i) = path_arclength(i-1) + path_distances(i-1);
  }
  std::cout << "Distances: " << path_distances.head(10).transpose() << std::endl;
  std::cout << "Arclength: " << path_arclength.head(10).transpose() << std::endl;

  qDebug() << "Looking for candidate stations (within 'search_distance' from the path)";
  Eigen::ArrayXi stations_on_path(stations_ids.size());
  Eigen::Index count = 0;
  for(unsigned int i=0; i<stations_ids.size(); i++) {
    if(stations_prices[i] < 0.4) {
      qDebug() << "FOUND BAD STATION";
      continue;
    }

    auto dlat = (path_latitudes - stations_latitudes[i]).abs();
    auto dlon = (path_longitudes - stations_longitudes[i]).abs();
    if(((dlat < latitude_margin) && (dlon < longitude_margin)).any()) {
      stations_on_path(count) = i;
      count++;
    }
  }
  stations_on_path.conservativeResize(count);
  qDebug() << "Found" << stations_on_path.size() << "candidates";

  if(stations_on_path.size() == 0) {
    emit failed("Could not find any station along the path");
    return;
  }

  qDebug() << "Calculating closest point on path";
  Eigen::ArrayXi closest_point_on_path(stations_on_path.size());
  for(unsigned int i=0; i<stations_on_path.size(); i++) {
    math_utilities::haversineDistance(
      path_latitudes,
      path_longitudes,
      stations_latitudes[stations_on_path[i]],
      stations_longitudes[stations_on_path[i]]
    ).minCoeff(&closest_point_on_path(i));
  }

  qDebug() << "Sorting stations along path";
  auto sorted_idx = math_utilities::argsort(closest_point_on_path);
  math_utilities::sortBy(closest_point_on_path, sorted_idx);
  math_utilities::sortBy(stations_on_path, sorted_idx);

  qDebug() << "Divinding path into segments";
  const unsigned int N_CUTS = static_cast<unsigned int>(std::ceil(2*path_arclength(path_arclength.size()-1) / problem.segment_length));
  Eigen::ArrayXi segments = (Eigen::ArrayXd::LinSpaced(N_CUTS+1, 0, N_CUTS) * (static_cast<double>(path_arclength.size()) / N_CUTS)).round().cast<int>();
  std::cout << "Cuts: " << N_CUTS << "; Segments: " << segments.transpose() << std::endl;

  qDebug() << "Copying prices and coordinates for candidate stations";
  Eigen::ArrayXd prices_on_path(stations_on_path.size());
  for(unsigned int i=0; i<stations_on_path.size(); i++) {
    prices_on_path(i) = stations_prices[stations_on_path(i)];
  }

  qDebug() << "Choosing cheapest stations in each segment";
  std::vector<int> cheapest_stations;
  cheapest_stations.reserve(N_CUTS);

  // This is a bit convoluted, but here it is:
  // The path is being cut into segments, so that we consider portions from
  // path(a) to path(b) at each iteration. The cuts are made at regular
  // intervals and gathered as: [s1, s2, s3, s4, s5, ...], but we consider
  // as extremities the pairs (s1, s3), (s2, s4), (s3, s5), etc.
  // At the same time, we want to find all elements of closest_point_on_path
  // that are contained in a segment. As an example, if the cuts are:
  // [0, 20, 40, 60, 80] and the content of closest_point_on_path is
  // [2, 12, 21, 38, 50, 67, 75, 79], we want to consider the following
  // segments:
  //   1. From 0 to 40; the points are [2, 12, 21, 38].
  //   2. From 20 to 60; the points are [21, 38, 50].
  //   3. From 40 to 80; the points are [50, 67, 75, 79].
  // To achieve the partition above, the following variables are used:
  // - i0 tells where the interval begins, inside closest_point_on_path. In the
  //     example, it would be 0, 2 & 4 (the indices of 2, 21 and 50
  //     respectively).
  // - i1 is just used as a 'memory variable'. At the end of an iteration, its
  //     value is copied into i0 and it gets the value of i2.
  // - i2 tells where the interval stops, inside closest_point_on_path. In the
  //     example, it would be 4, 5 & 8 (one plus the indices of 38, 50 and 79).
  // Using these, it is a bit easier and more efficient to perform the loop:
  // at each iteration, we can copy copy i1 into i0 and i2 into i1, and then
  // find the new value of i2.
  unsigned int i0 = 0;
  unsigned int i1 = 0;
  unsigned int i2 = std::distance(
    closest_point_on_path.data(),
    std::upper_bound(
      closest_point_on_path.data(),
      closest_point_on_path.data() + closest_point_on_path.size(),
      segments(1)
    )
  );

  for(unsigned int s=2; s<segments.size() && i2<closest_point_on_path.size(); s++) {
    i0 = i1;
    i1 = i2;

    if(s == segments.size()-1) {
      i2 = closest_point_on_path.size();
    }
    else {
      i2 = std::distance(
        closest_point_on_path.data(),
        std::upper_bound(
          closest_point_on_path.data() + i1,
          closest_point_on_path.data() + closest_point_on_path.size(),
          segments(s)
        )
      );
    }

    if(i0 == i2) {
      // Empty segment: skip it!
      continue;
    }

    // Find the cheapest of the stations in this segment.
    // TODO: it would be nice to handle ties byu selecting the closest one to the path.
    // std::cout << "Looking for cheapest station in set: " << prices_on_path.segment(i0, i2-i0).transpose() << std::endl;
    Eigen::Index i_cheapest;
    prices_on_path.segment(i0, i2-i0).minCoeff(&i_cheapest);
    i_cheapest += i0;

    // Add this station if it was not in the list already.
    if (cheapest_stations.size() == 0 || cheapest_stations.back() != stations_on_path[i_cheapest]) {
      cheapest_stations.push_back(stations_on_path[i_cheapest]);
    }
  }

  qDebug() << "Reduced options to a set of" << cheapest_stations.size() << "stations";

  Eigen::ArrayXi stations(cheapest_stations.size());
  Eigen::ArrayXd prices(cheapest_stations.size());
  Eigen::ArrayXd latitudes(cheapest_stations.size());
  Eigen::ArrayXd longitudes(cheapest_stations.size());

  for(unsigned int i=0; i<cheapest_stations.size(); i++) {
    const auto& station_idx = cheapest_stations[i];
    stations[i] = stations_ids[station_idx];
    prices[i] = stations_prices[station_idx];
    latitudes[i] = stations_latitudes[station_idx];
    longitudes[i] = stations_longitudes[station_idx];
  }

  // Add departure station.
  double departure_latitude_margin = math_utilities::latitude_variation(2*problem.search_distance);
  double departure_longitude_margin = math_utilities::longitude_variation(2*problem.search_distance, problem.departure_latitude);

  // Create a filter to select only a subset of all possible stations.
  db_filter.setGPSRange(
    problem.departure_latitude - departure_latitude_margin,
    problem.departure_latitude + departure_latitude_margin,
    problem.departure_longitude - departure_longitude_margin,
    problem.departure_longitude + departure_longitude_margin
  );

  QList<int> departure_stations_ids;
  QList<double> departure_stations_prices, departure_stations_latitudes, departure_stations_longitudes;
  ok = database_->findStations(
    db_filter,
    &departure_stations_ids,
    &departure_stations_prices,
    &departure_stations_latitudes,
    &departure_stations_longitudes,
    nullptr,
    nullptr
  );

  if(ok && departure_stations_ids.size() > 0) {
    int idx;
    Eigen::Map<const Eigen::VectorXd>(departure_stations_prices.data(), departure_stations_prices.size()).minCoeff(&idx);
    if(departure_stations_ids[idx] != stations[0]) {
      qDebug() << "Adding departure station ID =" << departure_stations_ids[idx];
      stations.conservativeResize(stations.size()+1);
      prices.conservativeResize(prices.size()+1);
      latitudes.conservativeResize(latitudes.size()+1);
      longitudes.conservativeResize(longitudes.size()+1);
      for(int i=stations.size()-1; i>0; i--) {
        stations(i) = stations(i-1);
        prices(i) = prices(i-1);
        latitudes(i) = latitudes(i-1);
        longitudes(i) = longitudes(i-1);
      }
      stations(0) = departure_stations_ids[idx];
      prices(0) = departure_stations_prices[idx];
      latitudes(0) = departure_stations_latitudes[idx];
      longitudes(0) = departure_stations_longitudes[idx];
    }
  }

  // Add arrival station.
  double arrival_latitude_margin = math_utilities::latitude_variation(2*problem.search_distance);
  double arrival_longitude_margin = math_utilities::longitude_variation(2*problem.search_distance, problem.arrival_latitude);

  // Create a filter to select only a subset of all possible stations.
  db_filter.setGPSRange(
    problem.arrival_latitude - arrival_latitude_margin,
    problem.arrival_latitude + arrival_latitude_margin,
    problem.arrival_longitude - arrival_longitude_margin,
    problem.arrival_longitude + arrival_longitude_margin
  );

  QList<int> arrival_stations_ids;
  QList<double> arrival_stations_prices, arrival_stations_latitudes, arrival_stations_longitudes;
  ok = database_->findStations(
    db_filter,
    &arrival_stations_ids,
    &arrival_stations_prices,
    &arrival_stations_latitudes,
    &arrival_stations_longitudes,
    nullptr,
    nullptr
  );

  if(ok && arrival_stations_ids.size() > 0) {
    int idx;
    Eigen::Map<const Eigen::VectorXd>(arrival_stations_prices.data(), arrival_stations_prices.size()).minCoeff(&idx);
    if(arrival_stations_ids[idx] != stations[stations.size()-1]) {
      qDebug() << "Adding arrival station ID =" << arrival_stations_ids[idx];
      stations.conservativeResize(stations.size()+1);
      prices.conservativeResize(prices.size()+1);
      latitudes.conservativeResize(latitudes.size()+1);
      longitudes.conservativeResize(longitudes.size()+1);
      stations(stations.size()-1) = arrival_stations_ids[idx];
      prices(prices.size()-1) = arrival_stations_prices[idx];
      latitudes(latitudes.size()-1) = arrival_stations_latitudes[idx];
      longitudes(longitudes.size()-1) = arrival_stations_longitudes[idx];
    }
  }

  // Show the stations on map.
  QList<double> stations_latitudes_for_map, stations_longitudes_for_map;
  for(unsigned int i=0; i<stations.size(); i++) {
    stations_latitudes_for_map.append(latitudes(i));
    stations_longitudes_for_map.append(longitudes(i));
  }
  qDebug() << "Adding stations to map";
  exportStations(stations_latitudes_for_map, stations_longitudes_for_map);

  std::cout << "IDs: " << stations.transpose() << std::endl;
  std::cout << "Prices: " << prices.transpose() << std::endl;
  std::cout << "Latitudes: " << latitudes.transpose() << std::endl;
  std::cout << "Longitudes: " << longitudes.transpose() << std::endl;

  // Request the distance matrix for the given stations.
  QList<int> stations_as_list(stations.size());
  Eigen::Map<Eigen::VectorXi>(stations_as_list.data(), stations.size()) = stations;
  QList<QList<double>> distance_matrix;
  qDebug() << "Requesting distance matrix for" << stations_as_list.size() << "stations";
  if(!database_->distanceMatrix(stations_as_list, distance_matrix)) {
    emit failed("Failed to obtain distance matrix");
    return;
  }

  // Time to solve the optimization.
  // Vector that will store all results.
  QList<LpgRoute> routes;

  // Result variables.
  double total_cost;
  QList<double> fuel, tank_level;

  // Convert the prices into a QList.
  QList<double> prices_list(prices.data(), prices.data()+prices.size());

  // Maximum number of combinations: 2**(N-2), where N is the number of
  // waypoints. We use N-2 because the first and last waypoints are fixed.
  const unsigned int max_combinations = 1 << (prices.size()-2);

  // Scan all combinations from 000...000 to 111...111 (in binary).
  for(unsigned int combination=0; combination<max_combinations; combination++) {
    // The first stop is always the first waypoint.
    QList<int> stops;
    stops.push_back(0);

    // Isolate the single bits from the current binary string, and add a stop
    // for every '1' that is found.
    for(unsigned int c_counter=combination, k=1; c_counter>0; c_counter>>=1, k++) {
      if(c_counter % 2 > 0)
        stops.push_back(k);
    }

    // The last stop is always the last waypoint.
    stops.push_back(prices.size()-1);

    // Try to solve the optimal fueling problem; if successful, store the
    // result for later.
    if(optimalFueling(problem, stops, prices_list, distance_matrix, fuel, tank_level, total_cost)) {
      QList<int> stops_ids(stops.size());
      for(unsigned int i=0; i<stops.size(); i++) {
        stops_ids[i] = stations[stops[i]];
      }
      routes.push_back(LpgRoute(total_cost, stops_ids, fuel, tank_level));
    }
  }

  // If no route has been found, exit.
  if(routes.size() == 0) {
    emit failed("Could not find any feasible solution to the optimization problems");
    return;
  }

  // Sort solutions by total cost.
  std::sort(routes.begin(), routes.end(), [&](const auto& a, const auto& b) { return a.cost < b.cost; });

  qDebug() << "Showing stops on map";
  QList<bool> stop_here;
  auto s = routes[0].stops.begin();
  for(unsigned int i=0; i<stations.size(); i++) {
    if(s->idx == stations[i]) {
      stop_here.append(true);
      qDebug() << "Stop at" << i;
      s++;
    }
    else {
      stop_here.append(false);
    }
  }
  exportStations(stations_latitudes_for_map, stations_longitudes_for_map, stop_here);

  qDebug() << "Sending solution to other components";
  emit solved(routes[0]);

  // Last step: check how much we are saving!
  double cost = 0;
  double tank = 0;

  for(unsigned int i=0; i<stations.size()-1; i++) {
    double fuel_to_next = distance_matrix[i][i+1] / problem.fuel_efficiency;
    if(fuel_to_next > tank) {
      // Refill!
      cost += (problem.tank_capacity - tank) * prices(i);
      tank = problem.tank_capacity;
    }
    tank -= fuel_to_next;
  }
  cost += (problem.tank_capacity - tank) * prices(prices.size()-1);
  qDebug() << "Optimal cost:" << routes[0].cost;
  qDebug() << "Unoptimized:" << cost;
}


bool LpgPlanner::optimalFueling(
  const LpgProblem& problem,
  const QList<int>& stops,
  const QList<double>& all_prices,
  const QList<QList<double>>& all_distances,
  QList<double>& fuel,
  QList<double>& tank_level,
  double& total_cost
)
{
  // Obtain n, that is the index of the last stop, and
  // make sure we are making at least two stops!
  int n = stops.size() - 1;
  if(n < 1) {
    qDebug() << "Not enough stops to calculate best route";
    return false;
  }

  // Setup vector of objective coefficients.
  Eigen::VectorXd prices(n+1);
  for(unsigned int i=0; i<n+1; i++)
    prices(i) = all_prices[stops[i]];
  Eigen::VectorXd f(2*n+1);
  f.topRows(n+1) = prices;
  f.bottomRows(n).setZero();

  // Setup equality constraints matrix.
  Eigen::MatrixXd A = Eigen::MatrixXd::Zero(n+1, 2*n+1);
  A.leftCols(n+1).setIdentity();
  for(unsigned int c=0; c<n; c++) {
    A(c, n+1+c) = -1;
    A(c+1, n+1+c) = 1;
  }

  // Setup equality constraints vector.
  Eigen::VectorXd b = Eigen::VectorXd::Zero(A.rows());
  b(0) = all_distances[stops[0]][stops[1]]/problem.fuel_efficiency - problem.initial_fuel;
  for(unsigned int i=1; i<n; i++)
    b(i) = all_distances[stops[i]][stops[i+1]]/problem.fuel_efficiency;
  b(n) = problem.tank_capacity;

  // Setup inequality constraints matrix.
  Eigen::MatrixXd C = Eigen::MatrixXd::Zero(3*n+1, 2*n+1);
  C.topRightCorner(n, n).diagonal() = Eigen::VectorXd::Constant(n, -1);
  C.block(n, 0, n, n).setIdentity();
  C.block(n+1, n+1, n-1, n-1).setIdentity();
  C.bottomLeftCorner(n+1, n+1).diagonal() = -prices;

  // Setup inequality constraints vector.
  Eigen::VectorXd d = Eigen::VectorXd::Zero(C.rows());
  d.topRows(n) = Eigen::VectorXd::Constant(n, -problem.autonomy_margin / problem.fuel_efficiency);
  d(n) = problem.tank_capacity - problem.initial_fuel;
  d.segment(n+1, n-1) = Eigen::VectorXd::Constant(n-1, problem.tank_capacity);
  d.segment(2*n+1, n-1) = Eigen::VectorXd::Constant(n-1, -problem.minimum_purchase);

  // Solve the optimization using the Simplex method.
  Eigen::VectorXd x = Eigen::VectorXd::Zero(2*n+1);
  std::string error_message;
  bool ok = EigenOpt::simplex::minimize(f, A, b, C, d, x, error_message, 1e-6);

  // Exit on failure.
  if(!ok) {
    return false;
  }

  // Copy results in the output variables.
  fuel = QList<double>(x.data(), x.data()+n+1);
  tank_level.resize(n+1);
  tank_level[0] = problem.initial_fuel;
  Eigen::VectorXd::Map(tank_level.data()+1, n) = x.bottomRows(n);
  total_cost = f.dot(x);
  return true;
}
