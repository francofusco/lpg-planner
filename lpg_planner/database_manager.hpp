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


class DatabaseManager : public QObject {
  Q_OBJECT

public:
  /// Auxiliary class to specify a set of filters when requesting data.
  class Filter {
  public:
    Filter() = default;
    QString compile(const QString& fields="*") const;
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

  explicit inline DatabaseManager(QObject *parent = nullptr) : DatabaseManager(nullptr, parent) {}
  explicit DatabaseManager(RouterService* calculator, QObject *parent = nullptr);

  inline bool ok() const { return db_ok_; }

  bool stationsInfo(
    const QList<int>& ids,
    QList<double>& prices,
    QList<double>& latitudes,
    QList<double>& longitudes
    );

  bool stationsAddresses(
    const QList<int>& ids,
    QStringList& addresses
  );

  inline bool stations(
    QList<int>& ids,
    QList<double>& prices,
    QList<double>& latitudes,
    QList<double>& longitudes
  ) { return stations(Filter(), ids, prices, latitudes, longitudes); }

  bool stations(
    const Filter& filter,
    QList<int>& ids,
    QList<double>& prices,
    QList<double>& latitudes,
    QList<double>& longitudes
  );

  bool distanceMatrix(
    const QList<int>& ids,
    QList<QList<double>>& distances
  );

signals:
  void locationNameChanged(QString name_before, QString name_after);

private:
  bool db_ok_ = false;
  QWidget* parent_widget_ = nullptr;
  RouterService* router_ = nullptr;
  QString DISTANCE_TABLE_NAME = "DISTANCE_TABLE_NAME_TO_BE_FILLED";

  QString distancesQueryString(const QList<int>& ids);

  static bool validDatabase(QSqlDatabase& db, const QMap<QString, QSet<QString>>& expected_tables);
  static QSqlError initDatabase(QWidget* qmessagebox_parent=nullptr);
};

#endif // DATABASE_MANAGER_HPP
