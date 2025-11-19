#include "router_openrouteservice.hpp"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>


QString RouterOpenRouteService::key(QWidget* parent) {
  QString api_key;

  // Try to load the API key from a file.
  QString api_key_filename("open_route_service_api_key");
  QString api_key_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, api_key_filename);
  if(api_key_path.isEmpty()) {
    QMessageBox::warning(
      parent,
      "Missing API key file",
      QString("Could not retrieve API key for OpenRouteService from file '%1' - expected to be in one of the following locations:\n%2").arg(
        api_key_filename,
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).join("\n")
      )
    );
  }
  else {
    QFile api_file(api_key_path);
    if(!api_file.open(QIODevice::ReadOnly)) {
      QMessageBox::critical(parent, "Failed reading API key", "Could not open file " + api_key_path);
    }
    else {
      QTextStream in(&api_file);
      if(in.atEnd()) {
        QMessageBox::critical(parent, "Failed reading API key", "File " + api_key_path + " appears to be empty.");
      }
      else {
        api_key = in.readLine();
      }
      api_file.close();
    }
  }

  qDebug() << "Loaded API key:" << api_key;
  return api_key;
}


RouterOpenRouteService::RouterOpenRouteService(QObject *parent) :
  RouterService{parent}
{
  // If the parent is a Widget, store an explicit reference to it so that it
  // can be used as the parent for all QMessageBox dialogs.
  parent_widget_ = qobject_cast<QWidget*>(parent);

  // Create a new Network Manager to send HTTPS requests.
  network_manager_ = new QNetworkAccessManager(this);

  // Load the API key for OpenRouteService.
  api_key_ = key(parent_widget_);
}


bool RouterOpenRouteService::waitForJson(
  QNetworkReply* reply,
  QJsonDocument& json
)
{
  // Sometimes it takes a bit for the reply to be ready, so it is probably a
  // good idea to at least show the user that the app is not frozen, just
  // waiting for a reply from OpenRouteService.
  QProgressDialog* progress_dialog = nullptr;
  if(parent_widget_ != nullptr) {
    progress_dialog = new QProgressDialog(
      "Waiting for response from OpenRouteService",
      "A request has been sent to OpenRouteService. Waiting for a reply, please wait...",
      0,
      0,
      parent_widget_
    );
    progress_dialog->setWindowModality(Qt::ApplicationModal);
    progress_dialog->setCancelButton(nullptr);
    progress_dialog->show();
  }

  // After the HTTPS request has been sent we need to wait for its response.
  // One way to wait would be to connect a slot to the QNetworkReply::finished
  // signal and do something there. However, this would require some not so
  // nice (in my opinion) redesign of this class and of those that need to call
  // calculateDistances(). Instead, what is done is to spawn an event loop and
  // connecting its QEventLoop::quit slot to QNetworkReply::finished. In all
  // honesty, I am not sure if this can ever block the application, e.g., when
  // a request is "malformed" or if there is some connection error.
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  loop.exec();

  // Close the dialog
  if(progress_dialog != nullptr) {
    progress_dialog->close();
    progress_dialog->deleteLater();
  }

  // Allow Qt to do its magic in terms of memory management!
  reply->deleteLater();

  // Try to parse the JSON returned by OpenRouteService, and exit on failure.
  QJsonParseError error;
  json = QJsonDocument::fromJson(reply->readAll(), &error);

  if(error.error != QJsonParseError::NoError) {
    QMessageBox::critical(parent_widget_, "Failed to parse response", error.errorString());
    return false;
  }
  return true;
}


bool RouterOpenRouteService::calculatePath(
  double departure_latitude,
  double departure_longitude,
  double arrival_latitude,
  double arrival_longitude,
  QList<double>& latitudes,
  QList<double>& longitudes
)
{
  // Exit immediately if we do not have an API key.
  if(api_key_.isEmpty()) {
    return false;
  }

  // Create the request.
  // See https://openrouteservice.org/dev/#/api-docs/v2/directions/{profile}/get
  QNetworkRequest request(
    QUrl(
      QString("https://api.openrouteservice.org/v2/directions/driving-car?api_key=%1&start=%2,%3&end=%4,%5").arg(
        api_key_,
        QString::number(departure_longitude, 'f', 6),
        QString::number(departure_latitude, 'f', 6),
        QString::number(arrival_longitude, 'f', 6),
        QString::number(arrival_latitude, 'f', 6)
      )
    )
  );

  // Send the request and wait for the reply.
  QJsonDocument json_doc;
  if(!waitForJson(network_manager_->get(request), json_doc)) {
    return false;
  }

  QJsonObject json = json_doc.object();
  QJsonValue coordinates_json_value = json.value("features").toArray().at(0).toObject().value("geometry").toObject().value("coordinates");
  QJsonArray coordinates_array = coordinates_json_value.toArray();

  if(coordinates_json_value.isNull() || !coordinates_json_value.isArray()) {
    QMessageBox::critical(parent_widget_, "Failed to parse response", "Could not retrieve 'features/0/geometry/coordinates' as an array from parsed GEOJson.");
    return false;
  }

  if(coordinates_array.empty()) {
    QMessageBox::critical(parent_widget_, "Failed to get route", "Array 'features/0/geometry/coordinates' from parsed GEOJson is empty.");
    return false;
  }

  latitudes.resize(coordinates_array.size());
  longitudes.resize(coordinates_array.size());

  for(unsigned int i=0; i<coordinates_array.size(); i++) {
    QJsonArray c = coordinates_array.at(i).toArray();
    latitudes[i] = c.at(1).toDouble();
    longitudes[i] = c.at(0).toDouble();
  }
  return true;
}


bool RouterOpenRouteService::calculateDistances(
  const QList<double>& latitudes,
  const QList<double>& longitudes,
  QList<QList<double>>& distances
)
{
  // Exit immediately if we do not have an API key.
  if(api_key_.isEmpty()) {
    return false;
  }

  // Also exit if the lists are incompatible.
  if(latitudes.size() != longitudes.size()) {
    return false;
  }

  // If there are no coordinates, exit immediately.
  if(latitudes.size() == 0) {
    distances.resize(0);
    return true;
  }

  // Create a request and attach a header to it.
  // See https://openrouteservice.org/dev/#/api-docs/v2/matrix/{profile}/post
  QNetworkRequest request(QUrl("https://api.openrouteservice.org/v2/matrix/driving-car"));
  request.setRawHeader("Accept", "application/json, application/geo+json, application/gpx+xml, img/png; charset=utf-8");
  request.setRawHeader("Authorization", api_key_.toUtf8());
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");

  // Store GPS coordinates as a nested JSON array.
  QJsonArray locations;
  for(unsigned int i=0; i<latitudes.size(); i++) {
    // WARNING: OpenRouteService expects coordinates as (LONG.,LAT.).
    locations.append(QJsonArray({longitudes[i], latitudes[i]}));
  }

  // Define the body for the POST request.
  QJsonObject body;
  body["locations"] = locations;
  body["metrics"] = QJsonArray({"distance"});
  QByteArray data = QJsonDocument(body).toJson();

  // Send the request and wait for the reply.
  QJsonDocument json_doc;
  if(!waitForJson(network_manager_->post(request, data), json_doc)) {
    return false;
  }

  // Extract the distance matrix returned by OpenRouteService and use it to
  // fill our own, i.e., the elements in 'distances'.
  QJsonArray distances_array = json_doc.object().value("distances").toArray();
  distances.resize(latitudes.size());
  for(unsigned int i=0; i<latitudes.size(); i++) {
    distances[i].resize(latitudes.size());
    QJsonArray distances_i = distances_array.at(i).toArray();
    for(unsigned int j=0; j<latitudes.size(); j++) {
      distances[i][j] = 1e-3 * distances_i.at(j).toDouble();
    }
  }
  return true;
}
