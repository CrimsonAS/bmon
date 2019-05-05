import QtQuick 2.6
import QtQuick.Controls 1.4 as QQC1
import no.crimson.bmon 1.0

QQC1.ApplicationWindow {
    visible: true
    width: 800
    height: 600

    ProcessModel {
        id: processModel
    }

    QQC1.TableView {
        id: tableView

        model: SortFilterProxyModel {
            id: sortFilterProxyModel
            source: processModel
            sortOrder: tableView.sortIndicatorOrder
            sortCaseSensitivity: Qt.CaseInsensitive
            sortRole: tableView.getColumn(tableView.sortIndicatorColumn).role

            // filterString
            // filterSyntax
            // filterCaseSensitivity
        }
        sortIndicatorVisible: true
        currentRow: 0

        anchors.fill: parent

        itemDelegate: Rectangle {
            color: {
                if (styleData.role == "activePulseStreams") {
                    if (styleData.value == 0) {
                        return "transparent"
                    }

                    return "red";
                }

                return "transparent"
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: {
                    var row = sortFilterProxyModel.get(styleData.row);
                    var process = row.processInfoObject;

                    if (process && process.live) {
                        return styleData.textColor;
                    } else {
                        return "#666666";
                    }
                }
                elide: styleData.elideMode
                text: {
                    if (styleData.role == "powerImpact") {
                        if (styleData.value) {
                            return styleData.value.toFixed(2)
                        } else {
                            return "0.00"
                        }
                    }
                    return styleData.value
                }
                width: parent.width
            }
        }

        QQC1.TableViewColumn {
            role: "pid"
            title: "PID"
            width: 100
        }
        QQC1.TableViewColumn {
            role: "name"
            title: "Name"
            width: 200
        }
        QQC1.TableViewColumn {
            role: "powerImpact"
            title: "Power"
            width: 100
        }
        QQC1.TableViewColumn {
            role: "activePulseStreams"
            title: "Audio streams"
            width: 100
        }
    }
}
