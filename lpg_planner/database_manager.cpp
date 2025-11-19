#include "database_manager.hpp"

#include <QDir>
#include <QGeoCoordinate>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringAlgorithms>

#include <Eigen/Dense>


QString DatabaseManager::Filter::compile(
  const QString& fields
) const
{
  QString query = "SELECT " + fields + " FROM Stations";
  QList<QString> extras;
  if(min_latitude != nullptr) {
    // Set bounds on latitude.
    if(*min_latitude == *max_latitude) {
      extras.append("latitude=" + QString::number(*min_latitude, 'f', 6));
    }
    else {
      extras.append(
        QString("latitude BETWEEN %1 AND %2").arg(
          QString::number(*min_latitude, 'f', 6),
          QString::number(*max_latitude, 'f', 6)
        )
      );
    }

    // Set bounds on longitude.
    if(*min_longitude == *max_longitude) {
      extras.append("longitude=" + QString::number(*min_longitude, 'f', 6));
    }
    else {
      extras.append(
        QString("longitude BETWEEN %1 AND %2").arg(
          QString::number(*min_longitude, 'f', 6),
          QString::number(*max_longitude, 'f', 6)
        )
      );
    }
  }

  if(min_price != nullptr) {
    // Set bounds on latitude.
    if(*min_price == *max_price) {
      extras.append("fuel_price=" + QString::number(*min_price, 'f', 6));
    }
    else {
      extras.append(
        QString("fuel_price BETWEEN %1 AND %2").arg(
          QString::number(*min_price, 'f', 6),
          QString::number(*max_price, 'f', 6)
        )
      );
    }
  }

  if(extras.size() > 0) {
    query += " WHERE " + extras.join(" AND ");
  }

  query += ";";
  return query;
}


bool DatabaseManager::Filter::setGPSRange(
  double min_latitude,
  double max_latitude,
  double min_longitude,
  double max_longitude
)
{
  if(min_latitude > max_latitude || min_longitude > max_longitude)
    return false;

  this->min_latitude = std::make_unique<double>(min_latitude);
  this->max_latitude = std::make_unique<double>(max_latitude);
  this->min_longitude = std::make_unique<double>(min_longitude);
  this->max_longitude = std::make_unique<double>(max_longitude);
  return true;
}


bool DatabaseManager::Filter::setPriceRange(
  double min_price,
  double max_price
)
{
  if(min_price > max_price || min_price < 0)
    return false;

  this->min_price = std::make_unique<double>(min_price);
  this->max_price = std::make_unique<double>(max_price);
  return true;
}



DatabaseManager::DatabaseManager(
  RouterService* calculator,
  QObject *parent
) : QObject(parent)
{
  // If the parent is a Widget, store an explicit reference to it so that it
  // can be used as the parent for all QMessageBox dialogs.
  parent_widget_ = qobject_cast<QWidget*>(parent);

  // If a distance calculator was given, just use it.
  if(calculator != nullptr) {
    router_ = calculator;
    DISTANCE_TABLE_NAME = "Distances";
  }
  else {
    router_ = new RouterService(this);
    DISTANCE_TABLE_NAME = "HaversineDistances";
  }

  // Initialize the database.
  QSqlError error = initDatabase(parent_widget_);
  if(error.type() != QSqlError::NoError) {
    QMessageBox::critical(
      parent_widget_,
      "Unable to initialize Database",
      "Error initializing database: " + error.text()
      );
    return;
  }

  // Signals that the database was initialized correctly.
  db_ok_ = true;
}


bool DatabaseManager::stationsInfo(
  const QList<int>& ids,
  QList<double>& prices,
  QList<double>& latitudes,
  QList<double>& longitudes
)
{
  // Fetch the required info, given the IDs.
  prices.resize(ids.size());
  latitudes.resize(ids.size());
  longitudes.resize(ids.size());

  // Compile a query to access all required data.
  QSqlQuery query;
  query.prepare("SELECT fuel_price, latitude, longitude FROM Stations WHERE id = ?");

  // Retrieve data, one record at a time.
  for(unsigned int i=0; i<ids.size(); i++) {
    query.addBindValue(ids[i]);

    if(!query.exec() || !query.next()) {
      return false;
    }

    prices[i] = query.value("fuel_price").toDouble();
    latitudes[i] = query.value("latitude").toDouble();
    longitudes[i] = query.value("longitude").toDouble();
  }

  return true;
}


bool DatabaseManager::stationsAddresses(
  const QList<int>& ids,
  QStringList& addresses
)
{
  // Fetch the required info, given the IDs.
  addresses.resize(ids.size());

  // Compile a query to access all required data.
  QSqlQuery query;
  query.prepare("SELECT address FROM Stations WHERE id = ?");

  // Retrieve data, one record at a time.
  for(unsigned int i=0; i<ids.size(); i++) {
    query.addBindValue(ids[i]);

    if(!query.exec() || !query.next()) {
      return false;
    }

    addresses[i] = query.value("address").toString();
  }

  return true;
}


bool DatabaseManager::stations(
  const Filter& filter,
  QList<int>& ids,
  QList<double>& prices,
  QList<double>& latitudes,
  QList<double>& longitudes
)
{
  QString query_str = filter.compile("id, fuel_price, latitude, longitude");
  qDebug() << "Running query:" << query_str;
  QSqlQuery query;
  if(!query.exec(query_str)) {
    return false;
  }

  ids.resize(0);
  prices.resize(0);
  latitudes.resize(0);
  longitudes.resize(0);
  while(query.next()) {
    ids.append(query.value("id").toInt());
    prices.append(query.value("fuel_price").toDouble());
    latitudes.append(query.value("latitude").toDouble());
    longitudes.append(query.value("longitude").toDouble());
  }
  return true;
}


QString DatabaseManager::distancesQueryString(
  const QList<int>& ids
  )
{
  // Convert ints to strings.
  QStringList ids_str;
  ids_str.reserve(ids.size());
  for(unsigned int i=0; i<ids.size(); i++) {
    ids_str.append(QString::number(ids[i]));
  }

  // Create a query string that can select matching location pairs.
  return QString(
    "SELECT * FROM %1"
    " "
    "WHERE from_id IN (%2) AND to_id IN (%2)"
    ).arg(
      DISTANCE_TABLE_NAME,
      ids_str.join(",")
    );
}


bool DatabaseManager::distanceMatrix(
  const QList<int>& ids,
  QList<QList<double>>& distances
  )
{
  // No need to do anything unless we have two or more locations!
  if(ids.size() == 0) {
    distances.resize(0);
    return true;
  }

  if(ids.size() == 1) {
    distances.resize(1);
    distances[0] = {};
    return true;
  }

  // Resize the distance matrix, so it is ready for use.
  distances.resize(ids.size());
  for(unsigned int i=0; i<ids.size(); i++) {
    distances[i].resize(ids.size());
    distances[i][i] = 0;
    for(unsigned int j=0; j<ids.size(); j++) {
      distances[i][j] = i==j ? 0.0 : -1.0;
    }
  }

  // Retrieve all existing distance pairs from the database.
  QSqlQuery query;
  QString query_str = distancesQueryString(ids);
  qDebug() << "Fetching records using:" << query_str;
  if(!query.exec(query_str)) {
    QMessageBox::critical(
      parent_widget_,
      "Failed to query database",
      "While calling DatabaseManager::distanceMatrix(): failed to execute query."
    );
    return false;
  }

  // Create an inverse map from IDs to indices.
  QMap<int,int> idx;
  for(unsigned int i=0; i<ids.size(); i++) {
    idx[ids[i]] = i;
  }

  // Fill the matrix with existig distances.
  while(query.next()) {
    distances[idx[query.value("from_id").toInt()]][idx[query.value("to_id").toInt()]] = query.value("distance").toDouble();
  }

  // Respectively, list of missing distances and list of ids appearing in at
  // least one missing pair.
  QList<QPair<int, int>> missing_pairs;
  QSet<int> missing_set;
  for(unsigned int i=0; i<ids.size(); i++) {
    for(unsigned int j=0; j<ids.size(); j++) {
      if(distances[i][j] < 0) {
        missing_pairs.append({i, j});
        missing_set.insert(i);
        missing_set.insert(j);
      }
    }
  }

  // If we found every entry, we can leave.
  if(missing_set.size() == 0) {
    qDebug() << "Distances were all cached in the database";
    return true;
  }

  qDebug() << "Missing distances for" << missing_pairs.size() << "pairs";

  // Transform the set into a list and obtain the inverse map.
  QList<int> missing_list = missing_set.values();
  QList<int> missing_ids(missing_list.size());
  idx.clear();
  for(unsigned int i=0; i<missing_list.size(); i++) {
    idx[missing_list[i]] = i;
    missing_ids[i] = ids[missing_list[i]];
  }

  // Obtain the coordinates of the missing IDs.
  QList<double> latitudes, longitudes;
  latitudes.reserve(missing_ids.size());
  longitudes.reserve(missing_ids.size());
  query.prepare("SELECT latitude, longitude FROM Stations WHERE id = ?");
  for(unsigned int i=0; i<missing_ids.size(); i++) {
    query.addBindValue(missing_ids[i]);

    if(!query.exec()) {
      QMessageBox::critical(
        parent_widget_,
        "Failed to query database",
        "While calling DatabaseManager::distanceMatrix(): failed to retrieve record."
        );
      return false;
    }

    if(!query.next()) {
      QMessageBox::critical(
        parent_widget_,
        "Failed to query database",
        "While calling DatabaseManager::distanceMatrix(): failed to retrieve record."
        );
      return false;
      continue;
    }
    latitudes.append(query.value("latitude").toDouble());
    longitudes.append(query.value("longitude").toDouble());
  }

  // Calculate the distance matrix for the required stations.
  QList<QList<double>> missing_distances;
  if(!router_->calculateDistances(latitudes, longitudes, missing_distances)) {
    QMessageBox::critical(
      parent_widget_,
      "Failed to calculate distances",
      "While calling DatabaseManager::distanceMatrix(): call to RouterService::calculateDistances() failed."
    );
    return false;
  }

  // Now that we have the missing distances, we can fill the matrix and cache
  // the values into the database!
  if(!query.prepare("INSERT INTO " + DISTANCE_TABLE_NAME + "(from_id, to_id, distance) values(?, ?, ?)")) {
    QMessageBox::warning(
      parent_widget_,
      "Internal Error",
      "While calling DatabaseManager::distanceMatrix(): failed to prepare 'INSERT' statement."
    );
    return false;
  }

  // Save missing values in the database.
  for(const auto& p : missing_pairs) {
    distances[p.first][p.second] = missing_distances[idx[p.first]][idx[p.second]];
    query.addBindValue(ids[p.first]);
    query.addBindValue(ids[p.second]);
    query.addBindValue(distances[p.first][p.second]);
    if(!query.exec()) {
      QMessageBox::critical(
        parent_widget_,
        "Failed to query database",
        "While calling DatabaseManager::distanceMatrix(): failed to insert new record."
        );
      return false;
    }
  }

  return true;
}


bool DatabaseManager::validDatabase(QSqlDatabase& db, const QMap<QString, QSet<QString>>& expected_tables) {
  // Check that we can access the given file.
  if(!db.open())
    return false;

  // Make sure the DB contains the required table and columns.
  for(const auto [table_name, required_columns] : expected_tables.asKeyValueRange()) {
    // Does the DB contain the required table?
    if(!db.tables().contains(table_name))
      return false;

    // Does the table contain the expected columns?
    QSqlRecord r = QSqlQuery("SELECT * FROM " + table_name).record();
    QSet<QString> existing_columns;
    for(int i=0; i<r.count(); i++) {
      existing_columns << r.fieldName(i).toLower();
    }
    if(!existing_columns.contains(required_columns))
      return false;
  }
  return true;
}


QSqlError DatabaseManager::initDatabase(
  QWidget* qmessagebox_parent
)
{
  // Sanity check to be able to use SQLite.
  if(!QSqlDatabase::drivers().contains("QSQLITE")) {
    QMessageBox::critical(qmessagebox_parent, "Unable to load database", "This demo needs the SQLITE driver");
    return QSqlError("Missing SQLITE driver", "", QSqlError::UnknownError);
  }

  // Try to open an existing database.
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  QString db_filename("stations.db");
  QString db_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, db_filename);

  if(db_path.isEmpty()) {
    QMessageBox::critical(
      qmessagebox_parent,
      "Missing database",
      QString("Could not locate database file '%1' - expected to be in one of the following locations:\n%2").arg(
        db_filename,
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).join("\n")
      )
    );
    return QSqlError("", "Missing database file", QSqlError::ConnectionError);
  }

  // We located the required DB file, just use it.
  db.setDatabaseName(db_path);

  // Try to open the database and check that there are the required tables.
  QMap<QString, QSet<QString>> expected_db{
    {"Stations", {"id", "longitude", "latitude", "fuel_price"}},
    {"Distances", {"from_id", "to_id", "distance"}},
    {"HaversineDistances", {"from_id", "to_id", "distance"}}
  };
  if(!validDatabase(db, expected_db)) {
    QMessageBox::critical(qmessagebox_parent, "Malformed database", "The database does not have the required tables and columns");
    return QSqlError("", "Wrong layout: missing table or columns", QSqlError::UnknownError);
  }

  return QSqlError();
}
