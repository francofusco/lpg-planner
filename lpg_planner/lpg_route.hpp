#ifndef LPG_ROUTE_HPP
#define LPG_ROUTE_HPP

#include "lpg_stop.hpp"
#include <QList>
#include <QMetaType>
#include <QString>


/// Auxiliary structure containing information about a sequence of stops.
struct LpgRoute {
  double cost = 0.0; ///< Total cost of the route.
  QList<LpgStop> stops; ///< Stops along the route.

  // Default constructor needed by Qt's metatype system.
  LpgRoute() = default;

  /// Create a route, given the cost and the stops.
  LpgRoute(double cost, const QList<LpgStop>& stops) : cost(cost), stops(stops) {}

  /// Create a set of stops and store them in this route.
  LpgRoute(double cost, const QList<int>& stops, const QList<double>& fuel, const QList<double> tank_levels_before);
};

Q_DECLARE_METATYPE(LpgRoute);

#endif // LPG_ROUTE_HPP
