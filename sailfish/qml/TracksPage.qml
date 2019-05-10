/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.TrackModel 1.0
import harbour.jupii.AVTransport 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property alias albumId: itemModel.albumId
    property alias artistId: itemModel.artistId
    property alias playlistId: itemModel.playlistId

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property var selectedItems

    canAccept: itemModel.selectedCount > 0

    onDone: {
        if (result === DialogResult.Accepted)
            selectedItems = itemModel.selectedItems()
    }

    TrackModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchDialogHeader {
            implicitWidth: root.width
            searchPlaceholderText: qsTr("Search tracks")
            model: itemModel
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu

            enabled: itemModel.count !== 0

            MenuItem {
                text: itemModel.count === itemModel.selectedCount ?
                          qsTr("Unselect all") :
                          qsTr("Select all")
                onClicked: {
                    if (itemModel.count === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            property bool isMic: utils.isIdMic(model.url)
            property bool isPulse: utils.isIdPulse(model.id)

            highlighted: down || model.selected
            title.text: model.title
            subtitle.text: root.albumId.length !== 0 || root.playlistId.length !== 0 ?
                               model.artist : model.album
            enabled: !itemModel.busy
            //icon.source: model.icon.toString().length !== 0 ? (model.icon + "?" + primaryColor) : ""
            defaultIcon.source: {
                if (isMic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (isPulse)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else
                    switch (model.type) {
                    case AVTransport.T_Image:
                        return "image://theme/icon-m-file-image?" + primaryColor
                    case AVTransport.T_Audio:
                        return "image://theme/icon-m-file-audio?" + primaryColor
                    case AVTransport.T_Video:
                        return "image://theme/icon-m-file-video?" + primaryColor
                    default:
                        return "image://theme/icon-m-file-other?" + primaryColor
                    }
            }

            onClicked: {
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }
        }

        ViewPlaceholder {
            enabled: itemModel.count === 0 && !itemModel.busy
            text: qsTr("No tracks")
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
