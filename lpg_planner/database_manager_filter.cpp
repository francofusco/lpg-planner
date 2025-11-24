#include "database_manager.hpp"


QSqlQuery DatabaseManager::Filter::compile() const
{
  // "Base" statement to select elements from the Stations table.
  QString select_query_string = "SELECT * FROM Stations";

  // Query parameters, to be added using QSqlQuery::bindValue().
  QMap<QString, QVariant> query_args;

  // Create a list of conditions to be appended in the form of:
  //   WHERE condition1 AND condition2 AND ...
  QStringList conditions;

  // Helper function: define a new condition for a given column. If the minimum
  // and maximum are the same, the condition is in the form 'column = value',
  // otherwise it is a range in the form 'column BETWEEN min AND max'. While
  // defining the string, also collect the values to be bound to the query.
  auto add_range = [&](const QString& column, const auto& min, const auto& max)
  {
    // Do not add "null" constraints.
    if(min == nullptr) {
      return;
    }

    // Add the constraint either as an equality, or a range.
    if(*min == *max) {
      conditions.append(QString("%1 = :%1").arg(column));
      query_args[":column"] = *min;
    }
    else {
      conditions.append(QString("%1 BETWEEN :%1_min AND :%1_max").arg(column));
      query_args[":" + column + "_min"] = *min;
      query_args[":" + column + "_max"] = *max;
    }
  };

  // Add constraints as needed.
  add_range("latitude", min_latitude, max_latitude);
  add_range("longitude", min_longitude, max_longitude);
  add_range("fuel_price", min_price, max_price);

  // If there are conditions, add them to the select query.
  if(!conditions.isEmpty()) {
    select_query_string += " WHERE " + conditions.join(" AND ");
  }

  // Now create the "full" query, which fetches the desired records, counts
  // them, and returns the record alongside the count. This is to workaround
  // the fact that QSqlQuery::size() is a no-op for SQLite databases.
  QSqlQuery query;
  query.prepare(
    QString(
      "WITH filtered_stations AS (" + select_query_string + ")"
      " "
      "SELECT *, (SELECT COUNT(*) FROM filtered_stations) AS query_size FROM filtered_stations;"
    )
  );

  // Bind values to the query - this should be safe against injections.
  for(const auto& [key, val] : query_args.asKeyValueRange()) {
    query.bindValue(key, val);
  }

  // Return the query.
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
