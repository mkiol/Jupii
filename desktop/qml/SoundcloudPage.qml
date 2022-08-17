/* Copyright (C) 2021-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import harbour.jupii.SoundcloudModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "soundcloud"

    property alias albumPage: itemModel.albumUrl
    property alias artistPage: itemModel.artistUrl
    readonly property bool albumMode: albumPage && albumPage.toString().length > 0
    readonly property bool artistMode: artistPage && artistPage.toString().length > 0
    readonly property bool searchMode: !albumMode && !artistMode
    readonly property bool notableMode: artistPage && artistPage === itemModel.featuredUrl
    readonly property bool featureMode: !itemModel.busy && root.searchMode && itemModel.filter.length === 0

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: itemModel.albumTitle.length > 0 ? itemModel.albumTitle :
           notableMode ? qsTr("Trending tracks") : itemModel.artistName.length > 0 ? itemModel.artistName : "SoundCloud"

    //refreshing: itemModel.busy
    Component.onCompleted: {
        refreshing = Qt.binding(function() { return itemModel.busy })
        itemModel.updateModel()
    }

    actions {
        main: Kirigami.Action {
            text: itemModel.selectedCount > 0 ? qsTr("Add %n selected", "", itemModel.selectedCount) : qsTr("Add selected")
            enabled: itemModel.selectedCount > 0 && !itemModel.busy
            iconName: "list-add"
            onTriggered: {
                playlist.addItemUrls(itemModel.selectedItems())
                app.removePagesAfterAddMedia()
            }
        }
        contextualActions: [
            Kirigami.Action {
                text: itemModel.selectableCount === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                iconName: itemModel.count === itemModel.selectedCount ?
                              "dialog-cancel" : "checkbox"
                enabled: itemModel.selectableCount > 0 && !itemModel.busy
                displayHint: Kirigami.Action.DisplayHint.AlwaysHide
                onTriggered: {
                    if (itemModel.selectableCount === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }
        ]
    }

    header: Controls.ToolBar {
        visible: !root.albumMode
        height: visible ? implicitHeight : 0
        RowLayout {
            spacing: 0
            anchors.fill: parent
            Kirigami.SearchField {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                Layout.rightMargin: Kirigami.Units.smallSpacing
                onTextChanged: {
                    itemModel.filter = text
                }
            }
        }
    }

    SoundcloudModel {
        id: itemModel
        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
        onBusyChanged: {
            if (!busy) {
                var idx = itemModel.lastIndex();
                if (idx > 0) itemList.positionViewAtIndex(idx, ListView.Beginning)
            }
        }
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem
            enabled: !itemModel.busy
            label: model.type === SoundcloudModel.Type_Track ? model.name :
                   model.type === SoundcloudModel.Type_Album ? model.album :
                   model.type === SoundcloudModel.Type_Artist ? model.artist : ""
            subtitle:  model.type === SoundcloudModel.Type_Track ||
                       model.type === SoundcloudModel.Type_Album ? model.artist : ""
            defaultIconSource: {
                switch (model.type) {
                case SoundcloudModel.Type_Artist:
                    return "view-media-artist"
                case SoundcloudModel.Type_Album:
                    return "media-album-cover"
                }
                return "emblem-music-symbolic"
            }
            iconSource: {
                if (model.type === SoundcloudModel.Type_Track && albumMode)
                    return ""
                return model.icon
            }
            iconSize: Kirigami.Units.iconSizes.medium
            next: model.type !== SoundcloudModel.Type_Track

            onClicked: {
                if (model.type === SoundcloudModel.Type_Track) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else if (model.type === SoundcloudModel.Type_Album) {
                    pageStack.pop(root)
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {albumPage: model.url})
                } else if (model.type === SoundcloudModel.Type_Artist) {
                    pageStack.pop(root)
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {artistPage: model.url})
                }
            }

            extra: model.type === SoundcloudModel.Type_Album ? qsTr("Album") :
                   model.type === SoundcloudModel.Type_Artist ? qsTr("Artist") : ""
            extra2: model.genre

            highlighted: {
                if (pageStack.currentItem !== root) {
                    var rightPage = app.rightPage(root)
                    if (rightPage && rightPage.objectName === "soundcloud") {
                        if (model.type === SoundcloudModel.Type_Album)
                            return rightPage.albumPage === model.url
                        if (model.type === SoundcloudModel.Type_Artist)
                            return rightPage.artistPage === model.url
                    }
                }

                return model.selected
            }

            actions: [
                Kirigami.Action {
                    iconName: "checkbox"
                    text: qsTr("Toggle selection")
                    visible: !listItem.next
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
            sourceComponent: listItemComponent
        }

        footer: ShowmoreItem {
            visible: !itemModel.busy && (root.featureMode || (root.notableMode && itemModel.canShowMore))
            onClicked: {
                if (root.featureMode) {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"),
                                   {artistPage: itemModel.featuredUrl})
                } else if (root.notableMode) {
                    itemModel.requestMoreItems()
                    itemModel.updateModel()
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: itemList.count === 0 && !itemModel.busy
            text: itemModel.filter.length == 0 && !root.albumMode && !root.artistMode ?
                      qsTr("Type the words to search") : qsTr("No items")
        }
    }
}
