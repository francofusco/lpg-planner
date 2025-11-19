#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "database_manager.hpp"
#include "lpg_planner.hpp"
#include "router_service.hpp"
#include "lpg_planner_widget.hpp"

#include <QGridLayout>
#include <QMainWindow>
#include <QQuickWidget>


class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private:
    LpgPlanner* planner_ = nullptr;
    RouterService* distance_calculator_ = nullptr;
    DatabaseManager* database_;
    LpgPlannerWidget* planner_widget_ = nullptr;
    QQuickWidget* map_quick_widget_ = nullptr;
};

#endif // MAINWINDOW_H
