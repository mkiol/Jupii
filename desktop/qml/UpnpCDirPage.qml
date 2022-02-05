/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import harbour.jupii.CDirModel 1.0

Kirigami.ScrollablePage {
    id: root

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: cdir.deviceFriendlyName.length > 0 ? cdir.deviceFriendlyName : qsTr("Add item")

    //refreshing: cdir.busy || itemModel.busy
    Component.onCompleted: {
        refreshing = Qt.binding(function() { return cdir.busy || itemModel.busy })
        itemModel.updateModel()
    }

    actions {
        main: Kirigami.Action {
            text: itemModel.selectedCount > 0 ? qsTr("Add %n selected", "", itemModel.selectedCount) : qsTr("Add selected")
            enabled: cdir.inited && itemModel.selectedCount > 0 && !itemModel.busy
            iconName: "list-add"
            onTriggered: {
                playlist.addItemUrls(itemModel.selectedItems())
                app.removePagesAfterAddMedia()
            }
        }
        contextualActions: [
            Kirigami.Action {
                text: itemModel.count === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                iconName: itemModel.count === itemModel.selectedCount ?
                              "dialog-cancel" : "checkbox"
                enabled: cdir.inited && itemModel.selectableCount > 0 && !itemModel.busy
                displayHint: Kirigami.Action.DisplayHint.AlwaysHide
                onTriggered: {
                    if (itemModel.count === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }
        ]
    }

    header: Controls.ToolBar {
        enabled: cdir.inited
        RowLayout {
            anchors.fill: parent
            Kirigami.SearchField {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                onTextChanged: {
                    itemModel.filter = text
                }
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: Kirigami.Units.smallSpacing
                text: qsTr("Sort by:")
            }
            Controls.ComboBox {
                Layout.alignment: Qt.AlignLeft
                Layout.rightMargin: Kirigami.Units.smallSpacing
                currentIndex: itemModel.queryType
                model: [
                    qsTr("Title"),
                    qsTr("Album"),
                    qsTr("Artist"),
                    qsTr("Track number"),
                    qsTr("Date")
                ]
                onCurrentIndexChanged: {
                    itemModel.queryType = currentIndex
                }
            }
        }
    }

    CDirModel {
        id: itemModel
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem
            highlighted: model.selected
            enabled: !itemModel.busy
            label: model.title
            subtitle: {
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
            defaultIconSource: {
                switch (model.type) {
                case CDirModel.BackType:
                    return "go-up"
                case CDirModel.DirType:
                    return "folder-open"
                case CDirModel.MusicAlbumType:
                    return "media-album-cover"
                case CDirModel.ArtistType:
                    return "amarok_artist"
                case CDirModel.AudioType:
                    return "audio-x-generic"
                case CDirModel.VideoType:
                    return "video-x-generic"
                case CDirModel.ImageType:
                    return "image-x-generic"
                default:
                    return "unknown"
                }
            }
            iconSource: model.icon
            iconSize: Kirigami.Units.iconSizes.medium

            onClicked: {
                if (model.selectable) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else {
                    itemModel.currentId = model.id
                }
            }

            actions: [
                Kirigami.Action {
                    visible: model.selectable
                    iconName: "checkbox"
                    text: qsTr("Toggle selection")
                    onTriggered: {
                        var selected = model.selected
                        itemModel.setSelected(model.index, !selected);
                    }
                }
            ]
        }
    }

    ListView {
        id: itemList
        model: itemModel

        delegate: Kirigami.DelegateRecycler {
            width: parent.width
            enabled: cdir.inited
            sourceComponent: listItemComponent
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: itemModel.count === 0 && !itemModel.busy && !cdir.busy
            text: qsTr("No items")
        }
    }
}
