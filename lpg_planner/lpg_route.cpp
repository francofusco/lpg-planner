#include "lpg_route.hpp"

LpgRoute::LpgRoute(
  double cost,
  const QList<int>& stops,
  const QList<double>& fuel,
  const QList<double> tank_levels_before
  ) : cost(cost)
{
  // Create the stops one-by-one and add them to the route.
  for(unsigned int i=0; i<stops.size(); i++) {
    this->stops.append(
      LpgStop(stops[i], fuel[i], tank_levels_before[i])
    );
  }
}
