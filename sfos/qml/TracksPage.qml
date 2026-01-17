/* Copyright (C) 2018-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.TrackModel 1.0
import harbour.jupii.AVTransport 1.0
import harbour.jupii.ContentServer 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property alias albumId: itemModel.albumId
    property alias artistId: itemModel.artistId
    property alias playlistId: itemModel.playlistId

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    canAccept: itemModel.selectedCount > 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    onAccepted: {
        playlist.addItemUrls(itemModel.selectedItems())
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
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu

            visible: itemModel.count > 0

            MenuItem {
                enabled: !itemModel.busy
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
            readonly property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            property bool isMic: utils.isIdMic(model.url)
            property bool isPulse: utils.isIdPulse(model.id)
            readonly property int itemType: utils.itemTypeFromUrl(model.url)

            highlighted: down || (model && model.selected)
            title.text: model ? model.title : ""
            subtitle.text: model ? root.albumId.length !== 0 || root.playlistId.length !== 0 ?
                               model.artist : model.album : ""
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            icon.source: {
                if (!model ||
                    itemType === ContentServer.ItemType_Mic ||
                    itemType === ContentServer.ItemType_PlaybackCapture ||
                    itemType === ContentServer.ItemType_ScreenCapture ||
                    itemType === ContentServer.ItemType_Cam ||
                    itemType === ContentServer.ItemType_Slides) {
                    return ""
                }
                return model.icon
            }
            defaultIcon.source: {
                if (!model) return ""
                if (itemType === ContentServer.ItemType_Mic)
                    return "image://theme/icon-m-mic?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_PlaybackCapture)
                    return "image://theme/icon-m-speaker?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_ScreenCapture)
                    return "image://theme/icon-m-display?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_Cam)
                    return "image://theme/icon-m-browser-camera?" + primaryColor
                else if (model.itemType === ContentServer.ItemType_Slides)
                    return "image://icons/icon-m-slidesitem?" + primaryColor
                else {
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
            }
            attachedIcon.source: {
                if (!model) return ""
                switch(itemType) {
                case ContentServer.ItemType_Url:
                    return "image://icons/icon-s-browser?" + primaryColor
                case ContentServer.ItemType_Upnp:
                    return "image://icons/icon-s-device?" + primaryColor
                }
                return ""
            }
            attachedIcon2.source: {
                if (!model || (icon.source.toString().length > 0 && icon.status !== Image.Ready)) {
                    return ""
                }
                if (itemType === ContentServer.ItemType_Mic ||
                        itemType === ContentServer.ItemType_PlaybackCapture ||
                        itemType === ContentServer.ItemType_ScreenCapture ||
                        itemType === ContentServer.ItemType_Cam ||
                        itemType === ContentServer.ItemType_Slides) {
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
                if (icon.source.toString().length === 0) {
                    return ""
                }
                return defaultIcon.source
            }
            onClicked: {
                if (!model) return
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
