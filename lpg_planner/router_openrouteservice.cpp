#include "router_openrouteservice.hpp"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardPaths>


const QString RouterOpenRouteService::API_KEY_FILENAME = "open_route_service_api_key";


QString RouterOpenRouteService::key() {
  // Locate the file where the API key is expected to be. Return an empty key
  // if the file is missing.
  QString api_key_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, API_KEY_FILENAME);
  if(api_key_path.isEmpty()) {
    qDebug() << QString(
      "Could not retrieve API key for OpenRouteService from file '%1' - expected to be in one of the following locations:\n%2"
    ).arg(
      API_KEY_FILENAME,
      QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).join("\n")
    );
    return "";
  }

  // Try to open the located file. Return an empty key on failure.
  QFile api_file(api_key_path);
  if(!api_file.open(QIODevice::ReadOnly)) {

    qDebug() << "Failed reading API key: could not open file" << api_key_path;
    return "";
  }

  // Create a stream to read from the file; if the file is empty, exit.
  QTextStream in(&api_file);
  if(in.atEnd()) {
    qDebug() << "Failed reading API key: file" << api_key_path << "appears to be empty.";
    return "";
  }

  // Read and return the API key.
  QString api_key = in.readLine();
  qDebug() << "Loaded API key";
  return api_key;
}


void RouterOpenRouteService::manageKey(QWidget* parent) {
  // Load the current key - if one exists.
  QString api_key = key();

  // Create a simple input dialog to ask the user for a key.
  bool ok;
  QString new_api_key = QInputDialog::getText(
    parent,
    "OpenRouteService setup",
    "To send requests to OpenRouteService, an API key is needed.\n"
    "Please, visit https://openrouteservice.org and create an\n"
    "account. After receiving the API key, please paste it here.",
    QLineEdit::Normal,
    api_key,
    &ok
  );

  // If the user clicked on "cancel", just exit.
  if(!ok) {
    qDebug() << "API key update: aborted";
    return;
  }

  // The user clicked on "ok", meaning that we can replace the API key. Start
  // by getting the path of the API key and making sure it exists. If it does
  // not, create the file from scratch!
  QString api_key_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, API_KEY_FILENAME);

  if(api_key_path.isEmpty()) {
    // Locate the AppData dir for this application and make sure it exists.
    QDir data_dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if(!data_dir.exists() && !data_dir.mkpath(".")) {
      qDebug() << "Failed to create paths for" << data_dir.absolutePath();
      return;
    }
    // Make sure the API key path is now defined.
    api_key_path = data_dir.filePath(API_KEY_FILENAME);
  }

  qDebug() << "Storing new API key into" << api_key_path;

  // Create and/or open the API key file so that we can save the new key.
  QFile api_file(api_key_path);
  if(!api_file.open(QIODevice::WriteOnly)) {
    qDebug() << "Could not create or open API key file" << api_key_path;
    return;
  }

  // Create a stream and write into the file.
  QTextStream(&api_file) << new_api_key;
}


RouterOpenRouteService::RouterOpenRouteService(
  DatabaseManager* database,
  QObject *parent
) : RouterService(database, parent)
{
  // If the parent is a Widget, store an explicit reference to it so that it
  // can be used as the parent for all QMessageBox dialogs.
  // TODO: THIS SHOULD BE REMOVED IN FAVOR OF A SIGNAL/SLOT MECHANISM.
  parent_widget_ = qobject_cast<QWidget*>(parent);

  // Create a new Network Manager to send HTTPS requests.
  network_manager_ = new QNetworkAccessManager(this);

  // Load the API key for OpenRouteService.
  reloadKey();
}


void RouterOpenRouteService::reloadKey() {
  api_key_ = key();
  if(api_key_.isEmpty()) {
    qDebug() << "WARNING: API key for OpenRouteService is empty!";
  }
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


bool RouterOpenRouteService::path(
  const QList<double>& waypoints_latitudes,
  const QList<double>& waypoints_longitudes,
  QList<double>& path_latitudes,
  QList<double>& path_longitudes
)
{
  // Exit immediately if we do not have an API key.
  if(api_key_.isEmpty()) {
    qDebug() << "Missing API key, cannot calculate paths";
    return false;
  }

  // The input coordinates must have the same length.
  if(waypoints_latitudes.size() != waypoints_longitudes.size()) {
    qDebug() << "Bad inputs passed to RouterOpenRouteService::path()";
    return false;
  }

  // For the moment, we don't allow intermediate waypoints...
  if(waypoints_latitudes.size() != 2) {
    qDebug() << "Currently, paths with only departure and arrival are supported";
    return false;
  }

  // Create the request.
  // See https://openrouteservice.org/dev/#/api-docs/v2/directions/{profile}/get
  QNetworkRequest request(
    QUrl(
      QString("https://api.openrouteservice.org/v2/directions/driving-car?api_key=%1&start=%2,%3&end=%4,%5").arg(
        api_key_,
        QString::number(waypoints_longitudes[0], 'f', 6),
        QString::number(waypoints_latitudes[0], 'f', 6),
        QString::number(waypoints_longitudes[1], 'f', 6),
        QString::number(waypoints_latitudes[1], 'f', 6)
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

  path_latitudes.resize(coordinates_array.size());
  path_longitudes.resize(coordinates_array.size());

  for(unsigned int i=0; i<coordinates_array.size(); i++) {
    QJsonArray c = coordinates_array.at(i).toArray();
    path_latitudes[i] = c.at(1).toDouble();
    path_longitudes[i] = c.at(0).toDouble();
  }
  return true;
}


bool RouterOpenRouteService::distanceMatrix(
  const QList<double>& latitudes,
  const QList<double>& longitudes,
  QList<QList<double>>& distances
)
{
  // Exit immediately if we do not have an API key.
  if(api_key_.isEmpty()) {
    qDebug() << "Missing API key, cannot calculate distance matrix";
    return false;
  }

  // The input coordinates must have the same length.
  if(latitudes.size() != longitudes.size()) {
    qDebug() << "Bad inputs passed to RouterOpenRouteService::distanceMatrix()";
    return false;
  }

  // No need to do anything unless we have two or more locations!
  if(latitudes.size() == 0) {
    distances.resize(0);
    return true;
  }

  // If there is just one point, the distance matrix is trivial.
  if(latitudes.size() == 1) {
    distances.resize(1);
    distances[0] = {};
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
