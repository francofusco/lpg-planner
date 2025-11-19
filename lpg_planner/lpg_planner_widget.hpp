#ifndef LPG_PLANNER_WIDGET_H
#define LPG_PLANNER_WIDGET_H

#include "lpg_problem.hpp"
#include "lpg_route.hpp"
#include "database_manager.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSqlRelationalTableModel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>


/// Widget that allows planning a roadtrip.
class LpgPlannerWidget : public QWidget {
  Q_OBJECT

public:
  /// Create a new widget for planning roadtrips.
  /** @param database Pointer to an existing DatabaseManager instance, which
   *    will be used to access data such as possible stops and driving
   *    distances.
   *  @param parent Parent of this widget.
   */
  explicit LpgPlannerWidget(
      DatabaseManager* database,
      QWidget *parent = nullptr
  );

private:
  DatabaseManager* database_ = nullptr; ///< Reference to the database manager.
  QVBoxLayout* layout_ = nullptr; ///< Main layout.
  QGridLayout* endpoints_layout_ = nullptr; ///< Layout that contains the widgets used to determine where to start/end the roadtrip.
  QLabel* departure_label_ = nullptr; ///< Label for the departure.
  QDoubleSpinBox* departure_latitude_ = nullptr; ///< Spinbox used to select the departure point.
  QDoubleSpinBox* departure_longitude_ = nullptr; ///< Spinbox used to select the departure point.
  QLabel* arrival_label_ = nullptr; ///< Label for the arrival.
  QDoubleSpinBox* arrival_latitude_ = nullptr; ///< Spinbox used to select the arrival point.
  QDoubleSpinBox* arrival_longitude_ = nullptr; ///< Spinbox used to select the arrival point.
  QGridLayout* params_layout_ = nullptr; ///< Layout containing some widgets to customize the roadtrip options.
  QSpinBox* tank_capacity_spinbox_ = nullptr; ///< Spinbox that allows to change the tank_capacity.
  QDoubleSpinBox* fuel_efficiency_spinbox_ = nullptr; ///< Spinbox that allows to change the fuel efficiency.
  QDoubleSpinBox* minimum_purchase_spinbox_ = nullptr; ///< Spinbox that allows to change the minimum fuel purchase.
  QSpinBox* autonomy_margin_spinbox_ = nullptr; ///< Spinbox that allows to change the autonomy margin.
  QSpinBox* initial_fuel_spinbox_ = nullptr; ///< Spinbox that allows to change the initial fuel in the tank.
  QPushButton* routing_btn_ = nullptr; ///< Button to start calculating the plan.
  QTableWidget* route_details_ = nullptr; ///< Table to display the result.

signals:
  void solve(LpgProblem);

public slots:
  /// Solve the optimization problem when a button is clicked.
  void requestRoute();

  /// Show the result of an optimization.
  void showResult(/*LpgRoute solution*/);
  void showResult(LpgRoute solution);

  /// Show a popup with an error message.
  void showError(const QString& error);
};

#endif // LPG_PLANNER_WIDGET_H
