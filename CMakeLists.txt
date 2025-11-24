cmake_minimum_required(VERSION 3.16)

project(lpg_planner VERSION 0.0.0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Location Network Positioning Qml Quick QuickWidgets Sql Widgets)
find_package(Eigen3 REQUIRED)
find_package(EigenOpt REQUIRED)


qt_add_executable(lpg_planner
    MANUAL_FINALIZATION
    lpg_planner/database_manager.hpp
    lpg_planner/database_manager.cpp
    lpg_planner/database_manager_filter.cpp
    lpg_planner/lpg_planner.hpp
    lpg_planner/lpg_planner.cpp
    lpg_planner/lpg_planner_widget.hpp
    lpg_planner/lpg_planner_widget.cpp
    lpg_planner/lpg_problem.hpp
    lpg_planner/lpg_route.hpp
    lpg_planner/lpg_route.cpp
    lpg_planner/lpg_stop.hpp
    lpg_planner/main.cpp
    lpg_planner/main_window.hpp
    lpg_planner/main_window.cpp
    lpg_planner/map.qml
    lpg_planner/math_utilities.hpp
    lpg_planner/math_utilities.hxx
    lpg_planner/router_openrouteservice.hpp
    lpg_planner/router_openrouteservice.cpp
    lpg_planner/router_service.hpp
    lpg_planner/router_service.cpp
    resources.qrc
)

# Needed because otherwise Eigen will generate binaries that are too large.
target_compile_options(lpg_planner PRIVATE -O1 -g0 -flto)
target_link_options(lpg_planner PRIVATE -flto)

target_link_libraries(lpg_planner PRIVATE
  Qt6::Core
  Qt6::Location
  Qt6::Network
  Qt6::Positioning
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickWidgets
  Qt6::Sql
  Qt6::Widgets
  Eigen3::Eigen
  EigenOpt::EigenOpt
)


set_target_properties(lpg_planner PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS lpg_planner
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(lpg_planner)
