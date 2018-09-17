/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.TrackModel 1.0

Dialog {
    id: root

    property alias albumId: trackModel.albumId
    property alias artistId: trackModel.artistId

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property var selectedPaths

    canAccept: trackModel.selectedCount > 0

    onDone: {
        if (result === DialogResult.Accepted)
            selectedPaths = trackModel.selectedPaths()
    }

    TrackModel {
        id: trackModel
        onFilterChanged: console.log("Filter: " + filter)
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        currentIndex: -1

        model: trackModel

        header: FocusScope {
            implicitHeight: column.height
            implicitWidth: root.width

            Column {
                id: column
                width: root.width

                DialogHeader {
                    id: header
                    dialog: root
                    spacing: 0

                    acceptText: {
                        var count = trackModel.selectedCount
                        return count > 0 ? qsTr("%n selected", "", count) : header.defaultAcceptText
                    }
                }

                SearchField {
                    width: root.width
                    placeholderText: qsTr("Search tracks")

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            listView.currentIndex = -1
                        }
                    }

                    onTextChanged: {
                        trackModel.filter = text.toLowerCase().trim()
                    }
                }
            }
        }

        PullDownMenu {
            id: menu

            enabled: trackModel.count > 0

            MenuItem {
                text: trackModel.count === trackModel.selectedCount ?
                          qsTr("Unselect all") :
                          qsTr("Select all")
                onClicked: {
                    if (trackModel.count === trackModel.selectedCount)
                        trackModel.setAllSelected(false)
                    else
                        trackModel.setAllSelected(true)
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor

            highlighted: down || model.selected
            title.text: model.title
            subtitle.text: root.albumId.length > 0 ? model.artist : model.album
            icon.source: model.image + "?" + primaryColor
            defaultIcon.source: "image://theme/icon-m-file-audio?" + primaryColor

            onClicked: {
                var selected = model.selected
                trackModel.setSelected(model.index, !selected);
            }
        }

        ViewPlaceholder {
            enabled: listView.count == 0
            text: qsTr("No tracks")
        }
    }

    VerticalScrollDecorator {
        flickable: listView
    }
}
