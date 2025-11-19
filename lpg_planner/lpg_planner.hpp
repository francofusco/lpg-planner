#ifndef LPG_PLANNER_HPP
#define LPG_PLANNER_HPP

#include "database_manager.hpp"
#include "lpg_problem.hpp"
#include "lpg_route.hpp"
#include "router_service.hpp"

#include <QGeoCoordinate>
#include <QObject>
#include <QWidget>


class LpgPlanner : public QObject {
  Q_OBJECT
public:
  explicit LpgPlanner(
    RouterService* router,
    DatabaseManager* database,
    QObject *parent = nullptr
  );

private:
  RouterService* router_ = nullptr;
  DatabaseManager* database_ = nullptr;

  void exportPath(const QList<double>& latitudes, const QList<double>& longitudes);
  void exportStations(const QList<double>& latitudes, const QList<double>& longitudes);
  void exportStations(const QList<double>& latitudes, const QList<double>& longitudes, const QList<bool>& stop);
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
  void solve(LpgProblem problem);
signals:
  void pathUpdated(const QVariantList& path, const QVariantMap& center, int zoom);
  void stationsUpdated(const QVariantList& stations);
  void solved(LpgRoute solution);
  void failed(const QString& why);
};

#endif // LPG_PLANNER_H
