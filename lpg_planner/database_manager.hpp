#ifndef DATABASE_MANAGER_HPP
#define DATABASE_MANAGER_HPP

#include "router_service.hpp"

#include <memory>

#include <QMap>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlTableModel>
#include <QString>

/* TODO LIST:
 * - Refactor the code: the routing service should be using the DB, not the
 *     opposite as it happens right now.
 * - Allow more filters for the queries (mainly, dates).
 * - Allow resetting a filter.
 * - Documentation!
 * - Remove all QMessageBox calls and add warning("Message") and
 *     error("Message") signals instead. Related to this, remove the "parent
 *     widget" member.
 * - Refactor the DB initialization strategy. Let the main window call
 *     "initDB()" and figure out what to do upon failure. This class should
 *     instead just call initDB() internally. I'm not sure if I want to even
 *     keep db_ok_ as a member, maybe I can just let the queries fail.
 */


class DatabaseManager : public QObject {
  Q_OBJECT

public:
  /// Auxiliary class to specify a set of filters when requesting data.
  class Filter {
  public:
    Filter() = default;
    QSqlQuery compile() const;
    bool setGPSRange(
      double min_latitude,
      double max_latitude,
      double min_longitude,
      double max_longitude
    );
    bool setPriceRange(double min_price, double max_price);
    // bool setDateRange(int max_days); // TODO!
  private:
    std::unique_ptr<double> min_latitude = nullptr;
    std::unique_ptr<double> max_latitude = nullptr;
    std::unique_ptr<double> min_longitude = nullptr;
    std::unique_ptr<double> max_longitude = nullptr;
    std::unique_ptr<double> min_price = nullptr;
    std::unique_ptr<double> max_price = nullptr;
    // std::unique_ptr<string> min_date = nullptr; // TODO!
  };

  /// Create a new DatabaseManager with no routing capabilities.
  explicit inline DatabaseManager(QObject *parent = nullptr) : DatabaseManager(nullptr, parent) {}

  // NOTE: the following should be removed as the refactoring will remove the router from the DB manager.
  /// Create a new DatabaseManager with a given router.
  explicit DatabaseManager(RouterService* calculator, QObject *parent = nullptr);

  /// Tells if the database was loaded successfully.
  inline bool ok() const { return db_ok_; }

  /// Retrieve a list of stations given their IDs.
  /** @param[in] ids A list of IDs to locate in the database.
    * @param[out] prices Pointer to a list to be filled with stations prices.
    *   It can be nullptr, in which case prices are not retrieved.
    * @param[out] latitudes Pointer to a list to be filled with stations
    *   latitudes. It can be nullptr, in which case latitudes are not
    *   retrieved.
    * @param[out] longitudes Pointer to a list to be filled with stations
    *   longitudes. It can be nullptr, in which case longitudes are not
    *   retrieved.
    * @param[out] dates Pointer to a list to be filled with stations dates. It
    *   can be nullptr, in which case dates are not retrieved.
    * @param[out] addresses Pointer to a list to be filled with stations
    *   addresses. It can be nullptr, in which case addresses are not
    *   retrieved.
    * @return The method scans the database ID-by-ID, and returns false
    *   immediately if an ID is missing (or if a database issue is encoutered).
    *   If all stations were found, the function returns true.
    */
  bool stationsFromIds(
    const QList<int>& ids,
    QList<double>* prices,
    QList<double>* latitudes,
    QList<double>* longitudes,
    QStringList* dates,
    QStringList* addresses
  );

  /// Retrieve all stations from the database, given some conditions.
  /** @param[in] filter A DatabaseManager::Filter instance that sets conditions
    *   on the records to be fetched.
    * @param[out] ids Pointer to a list to be filled with stations IDs. It can
    *   be nullptr, in which case IDs are not retrieved.
    * @param[out] prices Pointer to a list to be filled with stations prices.
    *   It can be nullptr, in which case prices are not retrieved.
    * @param[out] latitudes Pointer to a list to be filled with stations
    *   latitudes. It can be nullptr, in which case latitudes are not
    *   retrieved.
    * @param[out] longitudes Pointer to a list to be filled with stations
    *   longitudes. It can be nullptr, in which case longitudes are not
    *   retrieved.
    * @param[out] dates Pointer to a list to be filled with stations dates. It
    *   can be nullptr, in which case dates are not retrieved.
    * @param[out] addresses Pointer to a list to be filled with stations
    *   addresses. It can be nullptr, in which case addresses are not
    *   retrieved.
    * @return The method returns false if there was an issue accessing the
    *   database. It will return true if data could be retrieved. Note that if
    *   no station matches the given filter, the method returns true as this is
    *   not a database access issue. In this case, all output lists will simply
    *   have zero-size.
    */
  bool findStations(
    const Filter& filter,
    QList<int>* ids,
    QList<double>* prices,
    QList<double>* latitudes,
    QList<double>* longitudes,
    QStringList* dates,
    QStringList* addresses
  );

  /// Retrieve all stations from the database.
  /** @see findStations().
    */
  inline bool allStations(
    QList<int>* ids,
    QList<double>* prices,
    QList<double>* latitudes,
    QList<double>* longitudes,
    QStringList* dates,
    QStringList* addresses
    ) { return findStations(Filter(), ids, prices, latitudes, longitudes, dates, addresses); }

  /// Query the database for pairs of distances.
  /** Given a list of IDs, this method looks for records in the "Distances"
    * table whose from_id and to_id appear in the list of input IDs. It then
    * stores the values in the 'distances' nested-list, so that distances[i][j]
    * provides the distance between ids[i] and ids[j]. Note that the matrix
    * needs not to be symmetric.
    *
    * @todo Currently, the method will fetch existing records, determine
    *   missing ones, use the router to calculate the missing distances and
    *   store the obtained values in the database. However, this should be
    *   refactored to work in the opposite way: the router should be the one
    *   asking for existing data and filling the blanks where needed.
    */
  bool distanceMatrix(
    const QList<int>& ids,
    QList<QList<double>>& distances
  );

private:
  bool db_ok_ = false; ///< Flag that tells if the database was loaded successfully.
  QWidget* parent_widget_ = nullptr; ///< @todo Remove this and emit signals with text instead.
  RouterService* router_ = nullptr; ///< @todo Remove this, add functions to retrieve and store distances instead.
  QString DISTANCE_TABLE_NAME = "DISTANCE_TABLE_NAME_TO_BE_FILLED"; ///< @todo Maybe remove this, the "haversineDistance()" table was needed only when a router is not given, but the router will disappear entirely.

  /// Helper function that creates a query to select distance records.
  QString distancesQueryString(const QList<int>& ids);

  /// Helper function that can determine if a databse has the expected structure.
  static bool validDatabase(QSqlDatabase& db, const QMap<QString, QSet<QString>>& expected_tables);

  /// Load the database from a file.
  /** @todo Remove the "qmessagebox_parent" parameter, pass a string instead.
    */
  static QSqlError initDatabase(QWidget* qmessagebox_parent=nullptr);
};

#endif // DATABASE_MANAGER_HPP
