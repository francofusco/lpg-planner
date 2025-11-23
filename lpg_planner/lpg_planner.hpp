#ifndef LPG_PLANNER_HPP
#define LPG_PLANNER_HPP

#include "database_manager.hpp"
#include "lpg_problem.hpp"
#include "lpg_route.hpp"
#include "router_service.hpp"

#include <QGeoCoordinate>
#include <QObject>
#include <QWidget>

/* TODO LIST:
 * - Refactor the code: "split" this object in two components, the first one
 *     will take care of finding stations along a route, the ohter will solve
 *     the actual optimization problem.
 * - To find stations, the code currently checks which ones are the cheapest
 *     along the route. However, there is a better way: check the price first
 *     and break ties according to the distance from the route. Due to how
 *     minCoeff() works, right now ties are solved in order of appearance.
 * - Add the possibility to refine the choice in fueling stations. The reason
 *     being that some stations - while being cheap and near to the route - may
 *     require long detours. A trivial example is the case of stations on the
 *     highway but in the opposite way, requiring to exit the highway, do a
 *     180°, reach the station, refuel, keep driving until the next exit and
 *     then do yet another 180°. This is a difficult case to detect
 *     programmatically, and I would prefer to let the process work as: the
 *     algorithm first finds a bunch of stations and shows them to the user,
 *     who can then "veto" on some stops; the algorithm would then propose new
 *     alternatives, with the process repeating until necessary. It could also
 *     be interesting to allow permanently ignoring a station, by setting some
 *     property into the database.
 * - It would be nice to be able to tho show the final route, including detours
 *     for fueling. Not sure if caching routes would be a good idea, but why
 *     not? Maybe adding a time-stamp and refreshing only every so often.
 * - Change the problem so that we start from the location and not necessarily
 *     from the first station. Maybe add some sort of parameter like "start
 *     from LPG station". Concerning feasibility, it should be easy: we know
 *     the distance from the starting point to the first LPG station, so we can
 *     easiliy calculate the minimum amount of fuel to at least reach this
 *     station. If the initial tank level is below this, then the problem is
 *     bound to be infeasible. Otherwise, just treat the first stop as any stop
 *     and modify the initial conditions to subtract the fuel needed to reach
 *     it from the initial tank level, then run the simplex "as is".
 * - On second thought, infeasibility of a LPP is trivially verifiable by
 *     checking if distance[i][i+1] >= efficiency * tank_capacity. Yeah, this
 *     cannot deal with the minimum refill constraint, but it should prune out
 *     many solutions when a long roadtrip is to be considered!
 */


/// Class that can find optimal LPG stops along a road-trip.
class LpgPlanner : public QObject {
  Q_OBJECT
public:
  /// Create a new planner.
  /** @param router Object to be used for routing and distance queries.
    * @param database Object to be used for accessing the database.
    * @param parent Parent object, needed for Qt's memory management.
    */
  explicit LpgPlanner(
    RouterService* router,
    DatabaseManager* database,
    QObject *parent = nullptr
  );

private:
  RouterService* router_ = nullptr; ///< Used to get driving paths and distances.
  DatabaseManager* database_ = nullptr; ///< Used to access the database.

  /// Send the given list of GPS waypoints to the Map.
  /** Helper method to send a path to a map widget. The list of points is
    * gathered into a QVariantList and new center and zoom level for the map is
    * calculated to focus on the new path. Finally, the pathUpdated() signal is
    * emitted to update the map.
    * @param latitudes List of GPS latidudes of the waypoints in the path.
    * @param longitudes List of GPS latitudes of the waypoints in the path. It
    *   must have the same size as latitudes.
    */
  void exportPath(const QList<double>& latitudes, const QList<double>& longitudes);

  /// Show a set of LPG stations in the map.
  /** This overload will assume that we need to stop at each station.
    * @overload exportStations()
    */
  void exportStations(const QList<double>& latitudes, const QList<double>& longitudes);

  /// Show a set of LPG stations in the map.
  /** @param latitudes List of latitudes of the stations to be diplayed.
    * @param longitudes List of longitudes of the stations to be diplayed. It
    *   must have the samse size as latitudes.
    * @param stop List of boolean telling if we should stop at this station
    *   during the roadtrip. It affects the way the station is shown in the map
    *   (stops have higher visiblity).
    */
  void exportStations(const QList<double>& latitudes, const QList<double>& longitudes, const QList<bool>& stop);

  /// Helper method that can solve a given fueling problem.
  /** @param problem Parameters that define the problem.
    * @param stops List of stations indices where we will be stopping.
    * @param all_prices List of all fuel prices. For the current problem, only
    *   the prices all_prices[stops[0]], all_prices[stops[1]], etc. will be
    *   used.
    * @param all_distances Distance matrix for all stops. Only a subset of
    *   the matrix will be used for the current problem, more precisely those
    *   in the form all_distances[stops[i], stops[j]] (where j>i).
    * @param[out] fuel If the problem is feasible, this list will contain the
    *   amount of fuel to be purchased at each stop.
    * @param[out] tank_level If the problem is feasible, this list will contain
    *   the amount of fuel in the tank when arriving at each station, right
    *   before pumping.
    * @param[out] total_cost Optimal objective function value, i.e., total cost
    *   of the fuel along the roadtrip - if the problem is feasible.
    * @return Boolean flag telling if the problem is feasible. Note that if
    *   false is returned (infeasible problem) the output parameters should be
    *   ignored.
    */
  static bool optimalFueling(
    const LpgProblem& problem,
    const QList<int>& stops,
    const QList<double>& all_prices,
    const QList<QList<double>>& all_distances,
    QList<double>& fuel,
    QList<double>& tank_level,
    double& total_cost
  );

public slots:
  /// Solve the whole routing problem.
  void solve(LpgProblem problem);

signals:
  /// Signal emitted to show a path inside a map.
  void pathUpdated(const QVariantList& path, const QVariantMap& center, int zoom);

  /// Signal emitted to show a set of LPG statons inside a map.
  void stationsUpdated(const QVariantList& stations);

  /// Signal emitted when a routing problem has been completed.
  void solved(LpgRoute solution);

  /// Signal emitted when a routing problem has failed.
  void failed(const QString& why);
};

#endif // LPG_PLANNER_H
