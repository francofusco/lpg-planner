#ifndef ROUTER_SERVICE_HPP
#define ROUTER_SERVICE_HPP

#include "database_manager.hpp"

#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>


/// Base class for calculating distances between GPS coordinates.
class RouterService : public QObject {
  Q_OBJECT
public:
  /// Create a new object with a given parent.
  explicit RouterService(
    DatabaseManager* database,
    QObject* parent=nullptr
  );

  /// Calculate a path passing through some waypoints.
  /** This method uses the Haversine formula and linear interpolation to
    * calculate piece-wise linear paths. It should be overridden in sub-classes
    * ti allow different methods of path calculations, e.g., using some service
    * like GooGle Maps.
    */
  virtual bool path(
    const QList<double>& waypoints_latitudes,
    const QList<double>& waypoints_longitudes,
    QList<double>& path_latitudes,
    QList<double>& path_longitudes
  );

  bool path(
    const QList<int>& waypoints_ids,
    QList<double>& path_latitudes,
    QList<double>& path_longitudes
  );

  /// Calculate the distance between a set of coordinates.
  /** This method uses the Haversine formula to calculate a "straight-line"
   *  distance between pairs of locations. It should be overridden in
   *  sub-classes to allow different methods of distance calculations, e.g.,
   *  using some service like Google Maps.
   */
  virtual bool distanceMatrix(
    const QList<double>& latitudes,
    const QList<double>& longitudes,
    QList<QList<double>>& distances
    );

  /// Calculate the distance between a set of coordinates.
  /** This method uses the Haversine formula to calculate a "straight-line"
   *  distance between pairs of locations. It should be overridden in
   *  sub-classes to allow different methods of distance calculations, e.g.,
   *  using some service like Google Maps.
   */
  virtual bool distanceMatrix(
    const QList<int>& ids,
    QList<QList<double>>& distances
  );

private:
  DatabaseManager* database_ = nullptr;
};

#endif // DISTANCE_CALCULATOR_HPP
