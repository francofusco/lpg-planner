#ifndef LPG_PLANNER_HPP
#define LPG_PLANNER_HPP

#include "database_manager.hpp"
#include "lpg_problem.hpp"
#include "lpg_route.hpp"
#include "router_service.hpp"

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>


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

#endif // LPG_PLANNER_HPP
