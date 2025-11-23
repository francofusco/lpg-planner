#include "database_manager.hpp"

#include <QDir>
#include <QGeoCoordinate>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QStringAlgorithms>


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
  if(!query.prepare("SELECT * FROM Stations WHERE id=?;")) {
    qDebug() << "Failed to prepare select statement";
    return false;
  }

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


bool DatabaseManager::distancePairs(
  const QList<int>& ids,
  QMap<QPair<int,int>,double>& distances
)
{
  // No need to do anything unless we have two or more locations!
  if(ids.size() <= 1) {
    return true;
  }

  // Convert ints to strings.
  QStringList ids_str;
  ids_str.reserve(ids.size());
  for(unsigned int i=0; i<ids.size(); i++) {
    ids_str.append(QString::number(ids[i]));
  }

  // Create a query string that can select matching location pairs.
  QString query_str = QString(
    "SELECT * FROM Distances WHERE from_id IN (%1) AND to_id IN (%1)"
  ).arg(
    ids_str.join(",")
  );
  qDebug() << "Fetching records using:" << query_str;

  // Retrieve all existing distance pairs from the database.
  QSqlQuery query(query_str);
  if(!query.exec()) {
    qDebug() << "Failed to execute query";
    return false;
  }

  // Copy fetched records into the map.
  while(query.next()) {
    distances[{
      query.value("from_id").toInt(),
      query.value("to_id").toInt()
    }] = query.value("distance").toDouble();
  }
  return true;
}


bool DatabaseManager::insertPairs(
  const QMap<QPair<int,int>,double>& distances
)
{
  // Create a query that can insert distance pairs if they do not exist, or
  // update them if they exist.
  QString query_str = QString(
    "INSERT INTO Distances (from_id, to_id, distance)"
    " "
    "VALUES (?, ?, ?)"
    " "
    "ON CONFLICT(from_id, to_id)"
    " "
    "DO UPDATE SET distance = excluded.distance;"
  );
  qDebug() << "Preparing query:" << query_str;

  // Prepare the query for execution.
  QSqlQuery query;
  if(!query.prepare(query_str)) {
    qDebug() << "Failed to prepare query";
    return false;
  }

  // For each distance pair, run the query.
  for(auto [ids, distance] : distances.asKeyValueRange()) {
    query.addBindValue(ids.first);
    query.addBindValue(ids.second);
    query.addBindValue(distance);
    if(!query.exec()) {
      // Exit on failure.
      qDebug() << "Failed to run query with parameters:" << ids.first << ids.second << distance;
      return false;
    }
  }

  // All pairs were inserted!
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
    {"Distances", {"from_id", "to_id", "distance"}}
  };
  if(!openAndValidate(db, expected_db)) {
    // Remove the database from the list of connections.
    QSqlDatabase::removeDatabase(db.connectionName());
    return "The database in incompatible, it does not have the required tables and columns";
  }

  // Ok, the database was open!
  return QString();
}
