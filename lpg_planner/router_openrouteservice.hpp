#ifndef ROUTER_OPENROUTESERVICE_H
#define ROUTER_OPENROUTESERVICE_H

#include "router_service.hpp"
#include "database_manager.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>


class RouterOpenRouteService : public RouterService {
public:
  explicit RouterOpenRouteService(
    DatabaseManager* database,
    QObject *parent = nullptr
  );

  /// Calculate the path between two locations.
  virtual bool path(
    const QList<double>& waypoints_latitudes,
    const QList<double>& waypoints_longitudes,
    QList<double>& path_latitudes,
    QList<double>& path_longitudes
  ) override;

  /// Calculate the distance between a set of coordinates.
  /** Calculate driving distances between GPS coordinates by sending HTTPS
   *  requests to OpenRouteService.
   */
  virtual bool distanceMatrix(
    const QList<double>& latitudes,
    const QList<double>& longitudes,
    QList<QList<double>>& distances
  ) override;

  static QString key(QWidget* parent=nullptr);

private:
  QWidget* parent_widget_ = nullptr; ///< @todo remove it and use a signal/slot
  QString api_key_; ///< API key used to send requests to OpenRouteService.
  QNetworkAccessManager* network_manager_ = nullptr; ///< Used to send HTTPS requests.

  bool waitForJson(QNetworkReply* reply, QJsonDocument& json);
};

#endif // ROUTER_OPENROUTESERVICE_H
