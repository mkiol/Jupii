/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.PlaylistFileModel 1.0

Page {
    id: root

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

    PlaylistFileModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchPageHeader {
            implicitWidth: root.width
            title: qsTr("Playlist files")
            searchPlaceholderText: qsTr("Search playlist files")
            model: itemModel
            view: listView
        }

        delegate: DoubleListItem {
            id: listItem
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            title.text: model.title
            subtitle.text: qsTr("%n track(s)", "", model.count)
            enabled: !itemModel.busy
            icon.source: model.icon
            defaultIcon.source: "image://theme/icon-m-media-playlists?" + primaryColor

            ListView.onRemove: animateRemoval(listItem)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Select tracks")
                    onClicked: click()
                }

                MenuItem {
                    text: qsTr("Delete playlist file")
                    onClicked: remove()
                }
            }

            onClicked: click()

            function click() {
                var dialog = pageStack.push(Qt.resolvedUrl("TracksPage.qml"),{playlistId: model.id})
                dialog.accepted.connect(function() {
                    root.accepted(dialog.selectedItems)
                    root.doPop()
                })
            }

            function remove() {
                remorseAction(qsTr("Deleting"), function() {
                    itemModel.deleteFile(model.id)
                })
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No playlist files")
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
