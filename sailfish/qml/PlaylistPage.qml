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

    signal accepted(var songs);

    Component.onCompleted: {
        playlistFileModel.filter = ""
    }

    function doPop() {
        if (pageStack.busy)
            _doPop = true;
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop)
                pageStack.pop();
        }
    }

    PlaylistFileModel {
        id: playlistFileModel

        onSongsQueryResult: {
            root.accepted(songs);
            root.doPop()
        }

        onFilterChanged: console.log("Filter: " + filter)
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        currentIndex: -1

        model: playlistFileModel

        header: SearchField {
            width: parent.width
            placeholderText: qsTr("Search playlist")

            onActiveFocusChanged: {
                if (activeFocus) {
                    listView.currentIndex = -1
                }
            }

            onTextChanged: {
                playlistFileModel.filter = text.toLowerCase().trim()
            }
        }

        delegate: DoubleListItem {
            id: listItem
            title.text: model.title
            subtitle.text: qsTr("%n track(s)", "", model.count)
            icon.source: model.image
            defaultIcon.source: "image://theme/icon-m-media-playlists?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)
            ListView.onRemove: animateRemoval(listItem)

            function remove() {
                remorseAction("Deleting", function() {
                    playlistFileModel.deleteFile(model.id)
                })
            }

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Add tracks")
                    onClicked: {
                        playlistFileModel.querySongs(model.id)
                    }
                }

                MenuItem {
                    text: qsTr("Delete playlist")
                    onClicked: {
                        remove()
                    }
                }
            }

            onClicked: {
                playlistFileModel.querySongs(model.id)
            }
        }

        ViewPlaceholder {
            enabled: listView.count == 0
            text: qsTr("No playlists")
        }
    }

    VerticalScrollDecorator {
        flickable: listView
    }
}
