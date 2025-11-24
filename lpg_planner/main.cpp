#include "main_window.hpp"
#include "lpg_problem.hpp"
#include "lpg_route.hpp"
#include "lpg_stop.hpp"

#include <QApplication>


int main(int argc, char *argv[]) {
  qRegisterMetaType<LpgProblem>();
  qRegisterMetaType<LpgStop>();
  qRegisterMetaType<LpgRoute>();
  QApplication a(argc, argv);
  MainWindow w;
  w.show();
  return a.exec();
}
