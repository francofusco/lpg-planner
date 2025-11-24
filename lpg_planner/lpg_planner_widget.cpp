#include "lpg_planner_widget.hpp"

#include <QMessageBox>


LpgPlannerWidget::LpgPlannerWidget(
  DatabaseManager* database,
  QWidget *parent
)
  : QWidget{parent}
  , database_(database)
{
  // Create the main layout that will contain all other widgets.
  layout_ = new QVBoxLayout(this);

  // Create the layout for the input coordinates.
  endpoints_layout_ = new QGridLayout();
  departure_label_ = new QLabel("Departure:");
  endpoints_layout_->addWidget(departure_label_, 0, 0);
  departure_latitude_ = new QDoubleSpinBox();
  departure_latitude_->setDecimals(6);
  departure_latitude_->setRange(-90, 90);
  departure_latitude_->setValue(43.7102);
  departure_latitude_->setPrefix("Latitude: ");
  departure_latitude_->setSuffix("°");
  endpoints_layout_->addWidget(departure_latitude_, 0, 1, 1, 2);
  departure_longitude_ = new QDoubleSpinBox();
  departure_longitude_->setDecimals(6);
  departure_longitude_->setRange(-180, 180);
  departure_longitude_->setValue(7.2620);
  departure_longitude_->setPrefix("Longitude: ");
  departure_longitude_->setSuffix("°");
  endpoints_layout_->addWidget(departure_longitude_, 0, 3, 1, 2);
  arrival_label_ = new QLabel("Arrival:");
  endpoints_layout_->addWidget(arrival_label_, 1, 0);
  arrival_latitude_ = new QDoubleSpinBox();
  arrival_latitude_->setDecimals(6);
  arrival_latitude_->setRange(-90, 90);
  arrival_latitude_->setValue(45.659039);
  arrival_latitude_->setPrefix("Latitude: ");
  arrival_latitude_->setSuffix("°");
  endpoints_layout_->addWidget(arrival_latitude_, 1, 1, 1, 2);
  arrival_longitude_ = new QDoubleSpinBox();
  arrival_longitude_->setDecimals(6);
  arrival_longitude_->setRange(-180, 180);
  arrival_longitude_->setValue(13.771907);
  arrival_longitude_->setPrefix("Longitude: ");
  arrival_longitude_->setSuffix("°");
  endpoints_layout_->addWidget(arrival_longitude_, 1, 3, 1, 2);

  // Add the coordinates layout to the main widget.
  layout_->addLayout(endpoints_layout_);

  // Create the spinboxes to select the various parameters.
  tank_capacity_spinbox_ = new QSpinBox();
  tank_capacity_spinbox_->setRange(1, 100);
  tank_capacity_spinbox_->setValue(50);
  tank_capacity_spinbox_->setPrefix("Tank capacity: ");
  tank_capacity_spinbox_->setSuffix("L");

  fuel_efficiency_spinbox_ = new QDoubleSpinBox();
  fuel_efficiency_spinbox_->setDecimals(2);
  fuel_efficiency_spinbox_->setRange(0.01, 99.99);
  fuel_efficiency_spinbox_->setValue(10.0);
  fuel_efficiency_spinbox_->setPrefix("Fuel efficiency: ");
  fuel_efficiency_spinbox_->setSuffix("km/L");

  minimum_purchase_spinbox_ = new QDoubleSpinBox();
  minimum_purchase_spinbox_->setDecimals(2);
  minimum_purchase_spinbox_->setRange(0.0, 99.99);
  minimum_purchase_spinbox_->setValue(10.0);
  minimum_purchase_spinbox_->setPrefix("Minimum purchase: ");
  minimum_purchase_spinbox_->setSuffix("€");

  autonomy_margin_spinbox_ = new QSpinBox();
  autonomy_margin_spinbox_->setRange(0, 100);
  autonomy_margin_spinbox_->setValue(10);
  autonomy_margin_spinbox_->setPrefix("Autonomy margin: ");
  autonomy_margin_spinbox_->setSuffix("km");

  initial_fuel_spinbox_ = new QSpinBox();
  initial_fuel_spinbox_->setRange(0, tank_capacity_spinbox_->maximum());
  initial_fuel_spinbox_->setValue(10);
  initial_fuel_spinbox_->setPrefix("Initial fuel: ");
  initial_fuel_spinbox_->setSuffix("L");

  // Create the layout that contains settings widgets.
  params_layout_ = new QGridLayout();
  params_layout_->addWidget(tank_capacity_spinbox_, 0, 0);
  params_layout_->addWidget(fuel_efficiency_spinbox_, 0, 1);
  params_layout_->addWidget(minimum_purchase_spinbox_, 0, 2);
  params_layout_->addWidget(autonomy_margin_spinbox_, 1, 0);
  params_layout_->addWidget(initial_fuel_spinbox_, 1, 1);
  layout_->addLayout(params_layout_);

  // Add a button to calculate the optimal route.
  routing_btn_ = new QPushButton("Calculate best route");
  QObject::connect(routing_btn_, SIGNAL(clicked()), this, SLOT(requestRoute()));
  layout_->addWidget(routing_btn_);

  // Add a table to show the results.
  route_details_ = new QTableWidget(1, 5);
  route_details_->setHorizontalHeaderLabels({"Est.Fuel at Arrival [L]", "Fuel [L]", "Cost [€]", "Est.Fuel at Departure [L]", "Address"});
  route_details_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout_->addWidget(route_details_);
}


void LpgPlannerWidget::requestRoute() {
  // Clear the results table.
  route_details_->clearContents();

  // Simply forward the request to solve the optimization problem, given all the parameters.
  LpgProblem problem;
  problem.departure_latitude = departure_latitude_->value();
  problem.departure_longitude = departure_longitude_->value();
  problem.arrival_latitude = arrival_latitude_->value();
  problem.arrival_longitude = arrival_longitude_->value();
  problem.fuel_efficiency = fuel_efficiency_spinbox_->value();
  problem.tank_capacity = tank_capacity_spinbox_->value();
  problem.minimum_purchase = minimum_purchase_spinbox_->value();
  problem.autonomy_margin = autonomy_margin_spinbox_->value();
  problem.initial_fuel = initial_fuel_spinbox_->value();
  problem.segment_length = 150.0; // HARDCODED, FOR NOW
  problem.search_distance = 5.0; // HARDCODED, FOR NOW
  emit solve(problem);
}


void LpgPlannerWidget::showResult() {
  qDebug() << "HABEMUS SOLUTIONEM BUT NO PARAMETERS";
}


void LpgPlannerWidget::showResult(LpgRoute solution) {
  qDebug() << "HABEMUS SOLUTIONEM";

  // This should not be needed, but better safe than sorry.
  route_details_->clearContents();

  QList<int> ids(solution.stops.size());
  for(unsigned int i=0; i<solution.stops.size(); i++) {
    ids[i] = solution.stops[i].id;
  }

  QList<double> prices;
  QStringList addresses;
  if(!database_->stationsFromIds(ids, &prices, nullptr, nullptr, nullptr, &addresses)) {
    QMessageBox::critical(
      this,
      "Unexpected error",
      "Failed to access prices & addresses from the database"
    );
    return;
  }

  qDebug() << "Filling results table";
  route_details_->setRowCount(solution.stops.size());
  for(unsigned int i=0; i<solution.stops.size(); i++) {
    route_details_->setCellWidget(i, 0, new QLabel(QString::number(solution.stops[i].tank_level_before) + "L"));
    route_details_->setCellWidget(i, 1, new QLabel(QString::number(solution.stops[i].fuel) + "L"));
    route_details_->setCellWidget(i, 2, new QLabel(QString::number(solution.stops[i].fuel * prices[i]) + "€"));
    route_details_->setCellWidget(i, 3, new QLabel(QString::number(solution.stops[i].tank_level_after) + "L"));
    route_details_->setCellWidget(i, 4, new QLabel(addresses[i]));
  }
}


void LpgPlannerWidget::showError(const QString& error) {
  QMessageBox::critical(
    this,
    "Unable to solve the optimization",
    error
  );
}
