#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include "database_manager.hpp"
#include "lpg_planner.hpp"
#include "lpg_planner_widget.hpp"
#include "router_service.hpp"

#include <QGridLayout>
#include <QMainWindow>
#include <QQuickWidget>

/* TODO LIST:
 * - Add slots to show errors and warnings.
 * - Add a signal/slot mechanism to show that the planner is busy. Maybe
 *     something as simple as a slot showBusy(msg) that accepts a string
 *     describing what is "blocking" the app. In this slot, we create a new
 *     QProgressDialog if none is currently active, showing the message 'msg'.
 *     Another slot called, idk, hideBusy() closes the progress dialog.
 *     We could add a second slot, showBusy(msg, percentage) to show a progress
 *     bar filled by the given percentage.
 */


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

#endif // MAINWINDOW_H
