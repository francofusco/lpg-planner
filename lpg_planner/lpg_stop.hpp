#ifndef LPG_STOP_HPP
#define LPG_STOP_HPP

#include <QMetaType>


/// Auxiliary structure containing information about a stop.
struct LpgStop {
  int idx = 0; ///< Index of the stop.
  double fuel = 0.0; ///< Amount of fuel purchased at this stop, in L.
  double tank_level_before = 0.0; ///< Fuel in the tank right before pumping.
  double tank_level_after = 0.0; ///< Fuel in the tank right after pumping.

  // Default constructor needed by Qt's metatype system.
  LpgStop() = default;

  /// Create a new fueling stop.
  /** The amount of fuel after pumping is calculated from the amount before
     *  the stop and the quantity pumped at the station.
     */
  LpgStop(int idx, double fuel, double tank_level_before) : idx(idx), fuel(fuel), tank_level_before(tank_level_before), tank_level_after(tank_level_before+fuel) {}
};

Q_DECLARE_METATYPE(LpgStop);

#endif // LPG_STOP_HPP
