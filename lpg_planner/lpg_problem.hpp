#ifndef LPG_PROBLEM_HPP
#define LPG_PROBLEM_HPP

#include <QMetaType>
#include <QString>

struct LpgProblem {
  double departure_latitude = 0.0;
  double departure_longitude = 0.0;
  double arrival_latitude = 0.0;
  double arrival_longitude = 0.0;
  double fuel_efficiency = 0.0;
  double tank_capacity = 0.0;
  double minimum_purchase = 0.0;
  double autonomy_margin = 0.0;
  double initial_fuel = 0.0;
  double segment_length = 0.0;
  double search_distance = 0.0;

  bool isValid(QString& why) const {
    if(fuel_efficiency <= 0.0) {
      why = "Parameter 'fuel_efficiency' must be positive";
      return false;
    }

    if(tank_capacity <= 0.0) {
      why = "Parameter 'tank_capacity' must be positive";
      return false;
    }

    if(minimum_purchase < 0.0) {
      why = "Parameter 'minimum_purchase' must be positive or zero";
      return false;
    }

    if(autonomy_margin < 0.0) {
      why = "Parameter 'autonomy_margin' must be positive or zero";
      return false;
    }

    if(initial_fuel < 0.0) {
      why = "Parameter 'initial_fuel' must be positive or zero";
      return false;
    }

    if(segment_length <= 0.0) {
      why = "Parameter 'segment_length' must be positive";
      return false;
    }

    if(search_distance <= 0.0) {
      why = "Parameter 'search_distance' must be positive";
      return false;
    }

    return true;
  }

  inline bool isValid() const {
    QString s;
    return isValid(s);
  }
};

Q_DECLARE_METATYPE(LpgProblem);

#endif // LPG_PROBLEM_HPP
