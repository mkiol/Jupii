/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AlbumModel 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    property bool _doPop: false

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

    // Hack to update model after all transitions
    property bool _completed: false
    Component.onCompleted: {
        _completed = true
        itemModel.filter = ""

    }
    onStatusChanged: {
        if (status === PageStatus.Active && _completed) {
            _completed = false
            itemModel.updateModel()
        }
    }

    AlbumModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchPageHeader {
            implicitWidth: root.width
            title: qsTr("Albums")
            searchPlaceholderText: qsTr("Search albums")
            model: itemModel
            view: listView
        }

        PullDownMenu {
            MenuItem {
                text: qsTr("Sort by: %1")
                        .arg(itemModel.queryType == 0 ? qsTr("Album") : qsTr("Artist"));
                onClicked: {
                    itemModel.queryType = itemModel.queryType == 0 ? 1 : 0
                }
            }
        }

        delegate: DoubleListItem {
            title.text: model.title
            subtitle.text: model.artist + " · " + qsTr("%n track(s)", "", model.count)
            enabled: !itemModel.busy
            icon.source: model.icon
            defaultIcon.source: "image://theme/graphic-grid-playlist?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Select tracks")
                    onClicked: click()
                }
            }

            onClicked: click()

            function click() {
                pageStack.push(Qt.resolvedUrl("TracksPage.qml"), {albumId: model.id})
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No albums")
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: itemModel.busy
        size: BusyIndicatorSize.Large
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
