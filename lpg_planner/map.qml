import QtQuick 2.15
import QtQuick.Controls 2.15
import QtLocation 5.15
import QtPositioning 5.15

Item {
    width: 700
    height: 800

    // Size of the dots used to show where stops are to be made.
    readonly property int marker_size: 8

    // Array to be dynamically filled to show the driving path.
    property var routePath: []

    Map {
        id: myMap
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
            path: routePath
        }

        // List of markers represeting LPG stations.
        ListModel {
            id: markerModel
        }

        // View for the stations.
        MapItemView {
            model: markerModel

            // How individual markers are to be shown.
            delegate: MapQuickItem {
                id: marker
                coordinate: QtPositioning.coordinate(model.latitude, model.longitude)
                z: 100
                width: marker_size
                height: marker_size

                anchorPoint.x: marker_size/2
                anchorPoint.y: marker_size/2

                sourceItem: Rectangle {
                    width: marker_size
                    height: marker_size
                    radius: marker_size/2
                    color: model.color
                }
            }
        }
    }

    // Function to update the path and recenter the map.
    function updatePath(path, center, zoom) {
        routePath = path
        myMap.center = QtPositioning.coordinate(center.latitude, center.longitude)
        myMap.zoomLevel = zoom
    }

    // Function to update the list of stations.
    function showMarkers(markerList) {
        markerModel.clear()
        for(var i=0; i<markerList.length; i++) {
            markerModel.append({
                latitude: markerList[i].latitude,
                longitude: markerList[i].longitude,
                color: markerList[i].stop ? "red" : "gray"
            })
        }
    }
}
