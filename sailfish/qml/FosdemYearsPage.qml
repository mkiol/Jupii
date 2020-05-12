/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property bool _doPop: false

    signal accepted(var items);

    function doPop() {
        if (pageStack.busy)
            _doPop = true
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop) {
                root._doPop = false
                pageStack.pop()
            }
        }
    }

    SilicaListView {
        id: listView
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.height
        clip: true

        model: ListModel {
            id: conferencesModel
            ListElement {
                title: "FOSDEM 2020"
                year: 2020
            }
            ListElement {
                title: "FOSDEM 2019"
                year: 2019
            }
            ListElement {
                title: "FOSDEM 2018"
                year: 2018
            }
            ListElement {
                title: "FOSDEM 2017"
                year: 2017
            }
            ListElement {
                title: "FOSDEM 2016"
                year: 2016
            }
            ListElement {
                title: "FOSDEM 2015"
                year: 2015
            }
            ListElement {
                title: "FOSDEM 2014"
                year: 2014
            }
            ListElement {
                title: "FOSDEM 2013"
                year: 2013
            }
        }

        header: PageHeader {
            title: qsTr("Conferences")
        }

        delegate: SimpleListItem {
            title.text: model.title
            icon.source: "image://icons/icon-m-fosdem"
            defaultIcon.source: "image://icons/icon-m-item?" +
                                (highlighted || model.active ?
                                Theme.highlightColor : Theme.primaryColor)
            onClicked: {
                var dialog = pageStack.push(Qt.resolvedUrl("FosdemPage.qml"),
                                            {"year" : model.year})
                dialog.accepted.connect(function() {
                    root.accepted(dialog.selectedItems)
                    root.doPop()
                })
            }
        }

        ViewPlaceholder {
            enabled: listView.count == 0
            text: qsTr("No conferences found")
        }
    }

    VerticalScrollDecorator {
        flickable: listView
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
