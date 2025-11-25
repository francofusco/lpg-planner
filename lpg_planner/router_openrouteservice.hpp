#ifndef ROUTER_OPENROUTESERVICE_HPP
#define ROUTER_OPENROUTESERVICE_HPP

#include "router_service.hpp"
#include "database_manager.hpp"

#include <QJsonDocument>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QWidget>


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
    * requests to OpenRouteService.
    */
  virtual bool distanceMatrix(
    const QList<double>& latitudes,
    const QList<double>& longitudes,
    QList<QList<double>>& distances
  ) override;

  /// Read again the API key (usually in response to external edits).
  void reloadKey();

  /// Fetch and return the API key for OpenRouteService.
  /** @return The key, if successfully read from a file. If any issues occurred
    *   (such as missing or corrupt file) return an empty string.
    */
  static QString key();

  /// Allows to update the API key for OpenRouteService.
  /** Creates a QInputDialog that allows the user to add or modify the API key
    * to be used when sending requests to OpenRouteService.
    * @param parent Widget to be used to setup a QInputDialog to ask the user
    *   for a string (the API key).
    */
  static void manageKey(QWidget* parent);

private:
  static const QString API_KEY_FILENAME; ///< Name of the file where to locate the API key.
  QWidget* parent_widget_ = nullptr; ///< @todo remove it and use a signal/slot
  QString api_key_; ///< API key used to send requests to OpenRouteService.
  QNetworkAccessManager* network_manager_ = nullptr; ///< Used to send HTTPS requests.

  bool waitForJson(QNetworkReply* reply, QJsonDocument& json);
};

#endif // ROUTER_OPENROUTESERVICE_HPP
