#include "main_window.hpp"
#include "router_openrouteservice.hpp"

#include <QMenuBar>
#include <QMessageBox>
#include <QQuickItem>
#include <QTimer>


MainWindow::MainWindow(
  QWidget *parent
) : QMainWindow(parent)
{
  // Styling: set title and icon of the App.
  setWindowTitle("LPG Planner");
  setWindowIcon(QIcon(":/icons/pump.ico"));

  // Try to initialize the database.
  QString db_error = DatabaseManager::loadDatabase();
  if(!db_error.isEmpty()) {
    // Show an error to the user, then "ask" the application to close as soon
    // as the event-loop starts. Do not bother creating the UI of course.
    QMessageBox::critical(
      this,
      "Database Error",
      "An error occurred while loading the database: " + db_error
      );
    QTimer::singleShot(0, this, SLOT(close()));
    return;
  }

  // Instanciate the object used to access the database.
  database_ = new DatabaseManager(this);

  // Check if we can load an API key for OpenRouteService. If not, ask the user
  // to provide such key.
  if(RouterOpenRouteService::key().isEmpty()) {
    RouterOpenRouteService::manageKey(this);
  }

  // If after asking for a key, said key is still empty, just start the
  // software in "demo mode" using haversine distances. Otherwise, use ORS.
  if(RouterOpenRouteService::key().isEmpty()) {
    QMessageBox::information(
      this,
      "Demo Mode",
      "By not providing an API key for OpenRouteService, the app\n"
      "will start in 'demo mode': paths will be straight lines and\n"
      "therefore the results will not be accurate!\n"
      "\n"
      "Furthermore, please note that as of right now switching\n"
      "between demo mode and regular mode will likely invalidate\n"
      "the distance records in the local database. Before\n"
      "activating regular mode, please makes sure to clear the\n"
      "contents of the 'Distances' table (this will be done\n"
      "automatically in future versions)."
    );
    router_ = new RouterService(database_, this);
  }
  else {
    router_ = new RouterOpenRouteService(database_, this);
  }

  // Create the planner.
  planner_ = new LpgPlanner(router_, database_, this);

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

  // Create a menu bar and add a bunch of actions to it.
  QMenu* edit_menu = menuBar()->addMenu("&Edit");

  // Add an action to edit the API key for ORS.
  edit_menu->addAction(
    "Edit API key for OpenRouteService",
    this,
    [&](){
      // Allow the user to edit the key.
      RouterOpenRouteService::manageKey(this);
      // If we are actually using ORS, make sure the new key is used!
      RouterOpenRouteService* ors = dynamic_cast<RouterOpenRouteService*>(router_);
      if(ors != nullptr) {
        ors->reloadKey();
      }
      else {
        QMessageBox::information(
          this,
          "Demo Mode",
          "The API key for ORS has been changed, but the app was in\n"
          "'demo mode'. To actually use OpenRouteService, please close\n"
          "the app, clear the 'Distances' table in the database and\n"
          "restart the application."
        );
      }
    }
  );
}
