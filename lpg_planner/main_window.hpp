#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "database_manager.hpp"
#include "lpg_planner.hpp"
#include "lpg_planner_widget.hpp"
#include "router_service.hpp"

#include <QMainWindow>
#include <QQuickWidget>


class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);

private:
  DatabaseManager* database_ = nullptr;
  RouterService* router_ = nullptr;
  LpgPlanner* planner_ = nullptr;
  LpgPlannerWidget* planner_widget_ = nullptr;
  QQuickWidget* map_quick_widget_ = nullptr;
};

#endif // MAIN_WINDOW_HPP
