#include "database_manager.hpp"

#include <QDir>
#include <QGeoCoordinate>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringAlgorithms>

#include <Eigen/Dense>


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
}


// Helper function: resize the objects, if they are not null. Having a specific
// function ensures that the operation (which would be valid also on
// QStringList* objects) is done instead for the correct type.
template<class T>
inline void resize(
  unsigned int size,
  std::initializer_list<QList<T>*> fields
)
{
  // Force the compiler to accept only QList<int> and QList<double> as inputs.
  static_assert(
    std::is_same_v<T,int> || std::is_same_v<T,double>,
    "resize(uint, {QList<T>*}): T must be int or double"
  );

  // For each input, resize it if it is not nullptr.
  for(auto* field : fields) {
    if(field != nullptr) {
      field->resize(size);
    }
  }
}


// Helper function: clear and reserve memory for the objects (if not null).
template<class T>
inline void reserve(
  unsigned int size,
  std::initializer_list<QList<T>*> fields
)
{
  // Force the compiler to accept only int, double and QString as inputs.
  static_assert(
    std::is_same_v<T,int> || std::is_same_v<T,double> || std::is_same_v<T,QString>,
    "reserve(uint, {QList<T>*}): T must be int, double or QString"
  );

  // For each non-nullptr input, clear and reserve memory.
  for(auto* field : fields) {
    if(field != nullptr) {
      field->resize(0);
      field->reserve(size);
    }
  }
}


// Helper function: for all "List-like" objects, call clear().
template<typename... Lists>
inline void clearLists(Lists*... lists)
{
  ( (lists ? lists->clear() : void()), ... );
}


bool DatabaseManager::stationsFromIds(
  const QList<int>& ids,
  QList<double>* prices,
  QList<double>* latitudes,
  QList<double>* longitudes,
  QStringList* dates,
  QStringList* addresses
)
{
  // Resize QList<double> objects directly, and pre-allocate memory for
  // QStringList objects. This should be more memory-efficient.
  resize(ids.size(), {prices, latitudes, longitudes});
  reserve(ids.size(), {dates, addresses});

  // Compile a query to access all required data, one ID at a time.
  QSqlQuery query;
  query.prepare("SELECT * FROM Stations WHERE id=?;");

  // Helper initializer lists.
  auto double_fields = {std::pair{prices, "fuel_price"}, std::pair{latitudes, "latitude"}, std::pair{longitudes, "longitude"}};
  auto string_fields = {std::pair{dates, "date"}, std::pair{addresses, "address"}};

  // Retrieve data, one record at a time.
  for(unsigned int i=0; i<ids.size(); i++) {
    // Run the query for the next ID, exit on failure.
    query.addBindValue(ids[i]);
    if(!query.exec() || !query.next()) {
      clearLists(prices, latitudes, longitudes, dates, addresses);
      return false;
    }

    // Copy the reults into the corresponding lists.
    for(auto [list, field] : double_fields) {
      if(list != nullptr)
        (*list)[i] = query.value(field).toDouble();
    }
    for(auto [list, field] : string_fields) {
      if(list != nullptr)
        list->append(query.value(field).toString());
    }
  }

  // Success!
  return true;
}


bool DatabaseManager::findStations(
  const Filter& filter,
  QList<int>* ids,
  QList<double>* prices,
  QList<double>* latitudes,
  QList<double>* longitudes,
  QStringList* dates,
  QStringList* addresses
)
{
  // Given the filter, obtain the corresponding query.
  QSqlQuery query = filter.compile();
  qDebug() << "Running query:" << query.lastQuery();

  // Execute the query, and exit on failure.
  if(!query.exec()) {
    qDebug() << "Failed to run query";
    return false;
  }

  if(!query.next()) {
    qDebug() << "Query appears to be empty";
    // If the first call to query.next() returns false, then there are no
    // records matching the filter! Return "true" since this is not an error.
    resize(0, {ids});
    resize(0, {prices, latitudes, longitudes});
    reserve(0, {dates, addresses});
    return true;
  }

  // Fetch the number of records - which is a field contained in each record!
  unsigned int query_size = query.value("query_size").toInt();

  // Reserve memory for the lists.
  reserve(query_size, {ids});
  reserve(query_size, {prices, latitudes, longitudes});
  reserve(query_size, {dates, addresses});

  // Helper initializer lists.
  auto int_fields = {std::pair{ids, "id"}};
  auto double_fields = {std::pair{prices, "fuel_price"}, std::pair{latitudes, "latitude"}, std::pair{longitudes, "longitude"}};
  auto string_fields = {std::pair{dates, "date"}, std::pair{addresses, "address"}};

  do {
    // Copy the reults into the corresponding lists.
    for(auto [list, field] : int_fields) {
      if(list != nullptr)
        list->append(query.value(field).toInt());
    }
    for(auto [list, field] : double_fields) {
      if(list != nullptr)
        list->append(query.value(field).toDouble());
    }
    for(auto [list, field] : string_fields) {
      if(list != nullptr)
        list->append(query.value(field).toString());
    }
  } while(query.next());

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


// Helper function that can determine if a databse has the expected structure.
bool openAndValidate(
  QSqlDatabase& db,
  const QMap<QString, QSet<QString>>& expected_tables
)
{
  // Check that we can access the given file.
  if(!db.open())
    return false;

  // Make sure the DB contains the required table and columns.
  for(const auto [table_name, required_columns] : expected_tables.asKeyValueRange()) {
    // Does the DB contain the required table?
    if(!db.tables().contains(table_name)) {
      db.close();
      return false;
    }

    // Does the table contain the expected columns?
    QSqlRecord r = QSqlQuery("SELECT * FROM " + table_name).record();
    QSet<QString> existing_columns;
    for(int i=0; i<r.count(); i++) {
      existing_columns << r.fieldName(i).toLower();
    }
    if(!existing_columns.contains(required_columns)) {
      db.close();
      return false;
    }
  }

  return true;
}


QString DatabaseManager::loadDatabase() {
  // Sanity check to be able to use SQLite.
  if(!QSqlDatabase::drivers().contains("QSQLITE")) {
    return "Unable to load database: missing SQLITE driver";
  }

  // Try to locate the database.
  QString db_filename("stations.db");
  QString db_path = QStandardPaths::locate(QStandardPaths::AppDataLocation, db_filename);

  // Give up if the database is not in one of the "AppData" directories.
  if(db_path.isEmpty()) {
    return QString(
      "Could not locate database file '%1' - expected to be in one of the following locations:\n%2"
    ).arg(
        db_filename,
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).join("\n")
    );
  }

  // We located the required DB file: let's use it.
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
  db.setDatabaseName(db_path);

  // Try to open the database and check that there are the required tables.
  QMap<QString, QSet<QString>> expected_db{
    {"Stations", {"id", "longitude", "latitude", "fuel_price"}},
    {"Distances", {"from_id", "to_id", "distance"}},
    {"HaversineDistances", {"from_id", "to_id", "distance"}}
  };
  if(!openAndValidate(db, expected_db)) {
    // Remove the database from the list of connections.
    QSqlDatabase::removeDatabase(db.connectionName());
    return "The database in incompatible, it does not have the required tables and columns";
  }

  // Ok, the database was open!
  return QString();
}
