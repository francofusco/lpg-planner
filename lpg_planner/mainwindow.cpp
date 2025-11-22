#include "mainwindow.hpp"
#include "router_openrouteservice.hpp"

#include <QQuickItem>
#include <QTimer>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
  // Styling: set title and icon of the App.
  setWindowTitle("LPG Planner");
  setWindowIcon(QIcon(":/icons/pump.ico"));

  // Check if we can load an API key for OpenRouteSerivce; if so, create a
  // RouterOpenRouteService and use it!
  if(!RouterOpenRouteService::key(this).isEmpty()) {
    distance_calculator_ = new RouterOpenRouteService(this);
  }

  // Try to initialize the database.
  database_ = new DatabaseManager(distance_calculator_, this);
  if(!database_->ok()) {
    // Ask the application to close as soon as the event-loop starts.
    qDebug() << "ERROR :(";
    QTimer::singleShot(0, this, SLOT(close()));
    return;
  }

  // Create the planner.
  planner_ = new LpgPlanner(distance_calculator_, database_, this);

  // Add the router widget.
  planner_widget_ = new LpgPlannerWidget(database_);

  // Add the map widget.
  map_quick_widget_ = new QQuickWidget(this);
  map_quick_widget_->setSource(QUrl("qrc:/lpg_planner/map.qml"));
  map_quick_widget_->setResizeMode(QQuickWidget::SizeRootObjectToView);

  QGridLayout* layout = new QGridLayout();
  const int MAP_COLUMN_SPAN = 3;
  layout->addWidget(map_quick_widget_, 0, 0, -1, MAP_COLUMN_SPAN);
  layout->addWidget(planner_widget_, 0, MAP_COLUMN_SPAN, -1, 1);
  QWidget* main_widget = new QWidget();
  main_widget->setLayout(layout);
  setCentralWidget(main_widget);

  // Connect the planner and widget.
  QObject::connect(planner_widget_, SIGNAL(solve(LpgProblem)), planner_, SLOT(solve(LpgProblem)));
  // QObject::connect(planner_, SIGNAL(solved(LpgRoute)), router_, SLOT(showResult()));
  QObject::connect(planner_, SIGNAL(solved(LpgRoute)), planner_widget_, SLOT(showResult(LpgRoute)));
  QObject::connect(planner_, SIGNAL(failed(QString)), planner_widget_, SLOT(showError(QString)));

  // Get the root object of the QML file, used for connections.
  QQuickItem* map_root_object = map_quick_widget_->rootObject();

  // Connect a signal to allow showing a path on the map.
  auto path_callback = [map_root_object] (
    const QVariantList& path,
    const QVariantMap& center,
    int zoom
  ) {
    QMetaObject::invokeMethod(
      map_root_object,
      "updatePath",
      Q_ARG(QVariant, QVariant::fromValue(path)),
      Q_ARG(QVariant, QVariant::fromValue(center)),
      Q_ARG(QVariant, zoom)
    );
  };
  QObject::connect(planner_, &LpgPlanner::pathUpdated, map_root_object, path_callback);

  // Connect a signal to allow showing selected stations on the map.
  auto stations_callback = [map_root_object] (
    const QVariantList& stations
  ) {
    QMetaObject::invokeMethod(
      map_root_object,
      "showMarkers",
      Q_ARG(QVariant, QVariant::fromValue(stations))
    );
  };
  QObject::connect(planner_, &LpgPlanner::stationsUpdated, map_root_object, stations_callback);
}
