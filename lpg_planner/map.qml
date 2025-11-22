import QtQuick 2.15
import QtQuick.Controls 2.15
import QtLocation 5.15
import QtPositioning 5.15

Item {
    width: 700
    height: 800

    // Array to be dynamically filled to show the driving path.
    property var route_path: []

    Map {
        id: the_map
        anchors.fill: parent
        plugin: Plugin {
            name: "osm" // OpenStreetMap plugin.
        }

        // Start zoomed on Nice, because why not :)
        center: QtPositioning.coordinate(43.7102, 7.2620)
        zoomLevel: 13

        // Path displayed on the map.
        MapPolyline {
            id: polyline
            line.width: 4
            line.color: "blue"
            path: route_path
        }

        // List of markers represeting LPG stations.
        ListModel {
            id: markers_list
        }

        // View for the stations.
        MapItemView {
            model: markers_list

            // How individual markers are to be shown.
            delegate: MapQuickItem {
                id: marker
                coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
                width: model.icon_size
                height: model.icon_size

                anchorPoint.x: model.icon_size/2
                anchorPoint.y: 1+model.icon_size

                sourceItem: Image {
                    source: model.icon_file
                    width: model.icon_size
                    height: model.icon_size
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }

    // Function to update the path and recenter the map.
    function updatePath(path, center, zoom) {
        route_path = path
        the_map.center = QtPositioning.coordinate(center.latitude, center.longitude)
        the_map.zoomLevel = zoom
    }

    // Function to update the list of stations.
    function showMarkers(markers_properties) {
        markers_list.clear()
        for(var i=0; i<markers_properties.length; i++) {
            markers_list.append({
                latitude: markers_properties[i].latitude,
                longitude: markers_properties[i].longitude,
                icon_file: "qrc:/icons/pin-%1.png".arg(markers_properties[i].stop ? "green" : "red"),
                icon_size: markers_properties[i].stop ? 32 : 24
            })
        }
    }
}
