#include "router_service.hpp"

#include <QGeoCoordinate>


bool RouterService::calculatePath(
  double departure_latitude,
  double departure_longitude,
  double arrival_latitude,
  double arrival_longitude,
  QList<double>& latitudes,
  QList<double>& longitudes
)
{
  constexpr double RESOLUTION = 5.0;
  const double distance = 1e-3 * QGeoCoordinate({departure_latitude, departure_longitude}).distanceTo(QGeoCoordinate({arrival_latitude, arrival_longitude}));
  const unsigned int n_points = 1 + (unsigned int)(std::ceil(distance / RESOLUTION));
  latitudes.resize(0);
  longitudes.resize(0);
  latitudes.reserve(n_points);
  longitudes.reserve(n_points);
  for(unsigned int i=0; i<=n_points; i++) {
    double rho = ((double)i) / n_points;
    latitudes.append(departure_latitude * (1-rho) + arrival_latitude * rho);
    longitudes.append(departure_longitude * (1-rho) + arrival_longitude * rho);
  }
  return true;
}


bool RouterService::calculateDistances(
  const QList<double>& latitudes,
  const QList<double>& longitudes,
  QList<QList<double>>& distances
)
{
  if(latitudes.size() != longitudes.size() || latitudes.size() == 0) {
    return false;
  }

  distances.resize(latitudes.size());
  for(unsigned int i=0; i<latitudes.size(); i++) {
    distances[i].resize(latitudes.size());
    distances[i][i] = 0;
  }

  for(unsigned int i=0; i<latitudes.size()-1; i++) {
    for(unsigned int j=i+1; j<latitudes.size(); j++) {
      const auto d = 1e-3 * QGeoCoordinate({latitudes[i], longitudes[i]}).distanceTo(QGeoCoordinate({latitudes[j], longitudes[j]}));
      distances[i][j] = d;
      distances[j][i] = d;
    }
  }
  return true;
}
