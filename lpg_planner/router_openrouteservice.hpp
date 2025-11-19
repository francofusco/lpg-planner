#ifndef ROUTER_OPENROUTESERVICE_H
#define ROUTER_OPENROUTESERVICE_H

#include "router_service.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>


class RouterOpenRouteService : public RouterService {
public:
  explicit RouterOpenRouteService(QObject *parent = nullptr);

  /// Calculate the path between two locations.
  virtual bool calculatePath(
    double departure_latitude,
    double departure_longitude,
    double arrival_latitude,
    double arrival_longitude,
    QList<double>& latitudes,
    QList<double>& longitudes
  ) override;

  /// Calculate the distance between a set of coordinates.
  /** Calculate driving distances between GPS coordinates by sending HTTPS
   *  requests to OpenRouteService.
   */
  virtual bool calculateDistances(
    const QList<double>& latitudes,
    const QList<double>& longitudes,
    QList<QList<double>>& distances
  ) override;

  static QString key(QWidget* parent=nullptr);

private:
  QWidget* parent_widget_ = nullptr; ///< Parent of this object, as a QWidget - if it is a QWidget!
  QString api_key_; ///< API key used to send requests to OpenRouteService.
  QNetworkAccessManager* network_manager_ = nullptr; ///< Used to send HTTPS requests.

  bool waitForJson(QNetworkReply* reply, QJsonDocument& json);
};

#endif // ROUTER_OPENROUTESERVICE_H
