#ifndef DISTANCE_CALCULATOR_H
#define DISTANCE_CALCULATOR_H

#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>


/// Base class for calculating distances between GPS coordinates.
class RouterService : public QObject {
  Q_OBJECT
public:
  /// Create a new object with a given parent.
  explicit RouterService(QObject *parent = nullptr) : QObject{parent} {}

  /// Calculate the path from departure to destination.
  virtual bool calculatePath(
    double departure_latitude,
    double departure_longitude,
    double arrival_latitude,
    double arrival_longitude,
    QList<double>& latitudes,
    QList<double>& longitudes
  );

  /// Calculate the distance between a set of coordinates.
  /** This method uses the Haversine formula to calculate a "straight-line"
   *  distance between pairs of locations. It should be overridden in
   *  sub-classes to allow different methods of distance calculations, e.g.,
   *  using some service like Google Maps.
   */
  virtual bool calculateDistances(
    const QList<double>& latitudes,
    const QList<double>& longitudes,
    QList<QList<double>>& distances
  );
};

#endif // DISTANCE_CALCULATOR_H
