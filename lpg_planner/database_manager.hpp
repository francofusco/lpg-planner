#ifndef DATABASE_MANAGER_HPP
#define DATABASE_MANAGER_HPP

#include <memory>

#include <QList>
#include <QMap>
#include <QObject>
#include <QSqlQuery>
#include <QString>
#include <QStringList>


class DatabaseManager : public QObject {
  Q_OBJECT

public:
  /// Load the database from a file.
  /** @return An empty string if the database was loaded successfully,
    *   otherwise a string explaining what went wrong.
    */
  static QString loadDatabase();

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

  /// Create a new DatabaseManager.
  explicit inline DatabaseManager(QObject* parent = nullptr) : QObject(parent) { }

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

  /// Retrieve all distance pairs for the given IDs.
  /** @param ids A list of IDs for which pairs are to be fetched. All
    *   possible combinations will be looked for.
    * @param[out] distances A map whose elements are in the form
    *   {(ID1, ID2): d}, where the key is a pair of IDs and d is the distance
    *   from ID1 to ID2.
    * @return false if an error occurred, true otherwise.
    */
  bool distancePairs(
    const QList<int>& ids,
    QMap<QPair<int,int>,double>& distances
  );

  /// Add or update distance pairs for the given IDs.
  /** @param distances A map whose elements are in the form
    *   {(ID1, ID2): d}, where the key is a pair of IDs and d is the distance
    *   from ID1 to ID2. If an entry for the pair (ID1, ID2) already exists in
    *   the database, the corresponding distance is updated. If no such entry
    *   exists, a new one is added.
    * @return false if an error occurred, true otherwise.
    */
  bool insertPairs(
    const QMap<QPair<int,int>,double>& distances
  );
};

#endif // DATABASE_MANAGER_HPP
