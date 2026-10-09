/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import org.mkiol.jupii.CDirModel 1.0

Kirigami.ScrollablePage {
    id: root

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: cdir.deviceFriendlyName.length > 0 ? cdir.deviceFriendlyName : qsTr("Add item")

    supportsRefreshing: false
    Component.onCompleted: {
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
            },
            Kirigami.Action {
                text: itemModel.query.length > 0 ? qsTr("New search") :
                          qsTr("Search server…")
                iconName: "system-search"
                enabled: cdir.inited && itemModel.searchSupported && !itemModel.busy
                onTriggered: {
                    searchDialog.open()
                }
            },
            Kirigami.Action {
                visible: itemModel.query.length > 0
                text: qsTr("Clear search")
                iconName: "edit-clear"
                enabled: cdir.inited && !itemModel.busy
                onTriggered: {
                    itemModel.query = ""
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
            highlighted: model && model.selected
            enabled: !itemModel.busy
            label: model ? model.title : ""
            subtitle: {
                if (!model) return ""
                var ctype = itemModel.currentType
                if (model.selectable) {
                    var dur = model.duration > 1 ? " · " + utils.secToStr(model.duration) : "";
                    if (model.type === CDirModel.ImageType)
                        return model.date
                    if (model.artist.length > 0 && model.album.length > 0)
                        return model.artist + " · " + model.album + dur
                    if (model.artist.length > 0)
                        return model.artist + dur
                    if (model.album.length > 0)
                        return model.artist + dur
                    return ""
                }
                if (model.type === CDirModel.MusicAlbumType && model.artist.length > 0)
                    return model.artist
                return ""
            }
            defaultIconSource: {
                if (!model) return ""
                switch (model.type) {
                case CDirModel.BackType:
                    return "go-up"
                case CDirModel.DirType:
                    return "folder-open"
                case CDirModel.MusicAlbumType:
                    return "media-album-cover"
                case CDirModel.ArtistType:
                    return "view-media-artist"
                case CDirModel.AudioType:
                    return "audio-x-generic"
                case CDirModel.VideoType:
                    return "video-x-generic"
                case CDirModel.ImageType:
                    return "image-x-generic"
                }
                return "unknown"
            }
            attachedIconName: {
                if (!model || !showingIcon || model.type === CDirModel.BackType ||
                        model.type === CDirModel.DirType)
                    return ""
                switch (model.type) {
                case CDirModel.MusicAlbumType:
                    return "media-album-cover"
                case CDirModel.ArtistType:
                    return "view-media-artist"
                case CDirModel.VideoType:
                    return "video-x-generic"
                case CDirModel.ImageType:
                    return "image-x-generic"
                }
                return "emblem-music-symbolic"
            }

            iconSource: model ? model.icon : ""
            iconSize: Kirigami.Units.iconSizes.medium

            onClicked: {
                if (!model) return
                if (model.selectable) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else {
                    itemModel.currentId = model.id
                }
            }

            actions: [
                Kirigami.Action {
                    visible: model && model.selectable
                    iconName: "checkbox"
                    text: qsTr("Toggle selection")
                    onTriggered: {
                        if (!model) return
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
            anchors.left: parent.left; anchors.right: parent.right
            enabled: cdir.inited
            sourceComponent: listItemComponent
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: itemModel.count === 0 && !itemModel.busy && !cdir.busy
            text: itemModel.query.length > 0 ?
                      qsTr("No results for “%1”").arg(itemModel.query) :
                      qsTr("No items")
        }
    }

    Controls.Dialog {
        id: searchDialog
        title: qsTr("Search %1").arg(cdir.deviceFriendlyName)
        modal: true
        anchors.centerIn: parent
        width: Math.min(400, root.width - 2 * Kirigami.Units.largeSpacing)

	ColumnLayout {
		anchors.horizontalCenter: parent
		width: searchDialog.width - 2* Kirigami.Units.largeSpacing
		spacing: Kirigami.UnitslargeSpacing
            Controls.Label {
                text: qsTr("Searches the whole content directory of %1 for items matching your text.").arg(cdir.deviceFriendlyName)
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Controls.TextField {
                id: searchField
                placeholderText: qsTr("Title, artist or album")
                Layout.fillWidth: true
                onAccepted: {
                    if (searchField.text.trim().length > 0)
                        searchDialog.accept()
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Controls.Button {
                    text: qsTr("Cancel")
                    onClicked: searchDialog.reject()
                }
                Controls.Button {
                    text: qsTr("Search")
                    highlighted: true
                    enabled: searchField.text.trim().length > 0
                    onClicked: {
                        itemModel.query = searchField.text.trim()
                        searchDialog.accept()
                    }
                }
            }
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: cdir.busy || itemModel.busy
    }
}
