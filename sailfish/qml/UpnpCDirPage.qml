/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Sailfish.Silica 1.0

import harbour.jupii.CDirModel 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    canAccept: itemModel.selectedCount > 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    onAccepted: {
        playlist.addItemUrls(itemModel.selectedItems())
    }

    CDirModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchDialogHeader {
            implicitWidth: root.width
            model: itemModel
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu
            busy: itemModel.busy
            visible: itemModel.count !== 0

            MenuItem {
                visible: itemModel.selectableCount !== 0
                text: itemModel.selectableCount === itemModel.selectedCount ?
                          qsTr("Unselect all") :
                          qsTr("Select all")
                onClicked: {
                    if (itemModel.selectableCount === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }

            MenuItem {
                text: {
                    // 0 - by title
                    // 1 - by album
                    // 2 - by artist
                    // 3 - by track number
                    // 4 - by date
                    var type = ""
                    switch (itemModel.queryType) {
                    case 1: type = qsTr("Album"); break;
                    case 2: type = qsTr("Artist"); break;
                    case 3: type = qsTr("Track number"); break;
                    case 4: type = qsTr("Date"); break;
                    default: type = qsTr("Title");
                    }
                    return qsTr("Sort by: %1").arg(type)
                }

                onClicked: {
                    itemModel.switchQueryType()
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            highlighted: down || model.selected
            title.text: model.title
            subtitle.text: {
                var ctype = itemModel.currentType
                if (model.selectable) {
                    if (model.type === CDirModel.ImageType)
                        return model.date
                    if (model.artist.length > 0 && model.album.length > 0)
                        return model.artist + " · " + model.album
                    if (model.artist.length > 0)
                        return model.artist
                    if (model.album.length > 0)
                        return model.artist
                    return ""
                }
                if (model.type === CDirModel.MusicAlbumType && model.artist.length > 0)
                    return model.artist
                return ""
            }

            enabled: !itemModel.busy

            onClicked: {
                if (model.selectable) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else {
                    itemModel.currentId = model.id
                }
            }

            icon.source: model.icon
            defaultIcon.source: {
                switch (model.type) {
                case CDirModel.BackType:
                    return "image://theme/icon-m-back?" + primaryColor
                case CDirModel.DirType:
                    return "image://theme/icon-m-file-folder?" + primaryColor
                case CDirModel.MusicAlbumType:
                    return "image://theme/icon-m-media-albums?" + primaryColor
                case CDirModel.ArtistType:
                    return "image://theme/icon-m-media-artists?" + primaryColor
                case CDirModel.AudioType:
                    return "image://theme/icon-m-file-audio?" + primaryColor
                case CDirModel.VideoType:
                    return "image://theme/icon-m-file-video?" + primaryColor
                case CDirModel.ImageType:
                    return "image://theme/icon-m-file-image?" + primaryColor
                default:
                    return "image://theme/icon-m-file-other?" + primaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: itemModel.count === 0 && !itemModel.busy && !cdir.busy
            text: qsTr("No items")
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: cdir.busy || itemModel.busy
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
