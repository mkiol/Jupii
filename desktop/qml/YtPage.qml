/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

import org.mkiol.jupii.Settings 1.0
import org.mkiol.jupii.ContentServer 1.0
import org.mkiol.jupii.YtModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "yt"

    property alias albumPage: itemModel.albumId
    property alias albumType: itemModel.albumType
    property alias artistPage: itemModel.artistId
    readonly property bool albumMode: albumPage && albumPage.length > 0
    readonly property bool artistMode: artistPage && artistPage.length > 0
    readonly property bool searchMode: !albumMode && !artistMode
    readonly property bool homeMode: artistPage && artistPage === itemModel.homeId
    readonly property bool featureMode: !itemModel.busy && root.searchMode && itemModel.filter.length === 0

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: itemModel.albumTitle.length > 0 ? itemModel.albumTitle :
           homeMode ? qsTr("Home") : itemModel.artistName.length > 0 ? itemModel.artistName : "YouTube Music"

    supportsRefreshing: false
    Component.onCompleted: {
        itemModel.updateModel()
    }
    Component.onDestruction: addHistory()

    actions {
        main: Kirigami.Action {
            text: itemModel.selectedCount > 0 ? qsTr("Add %n selected", "", itemModel.selectedCount) : qsTr("Add selected")
            enabled: itemModel.selectedCount > 0 && !itemModel.busy
            iconName: "list-add"
            onTriggered: {
                if (settings.ytPreferredType === Settings.YtPreferredType_Audio)
                    playlist.addItemUrls(itemModel.selectedItems(), ContentServer.Type_Music)
                else
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

    function addHistory() {
        if (searchMode) {
            var filter = itemModel.filter
            if (filter.length > 0) settings.addYtSearchHistory(filter)
        }
    }

    header: Controls.ToolBar {
        visible: !root.albumMode && !root.artistMode
        height: visible ? implicitHeight : 0
        RowLayout {
            anchors.fill: parent
            SearchDialogHeader {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                view: itemList
                recentSearches: root.searchMode ? settings.ytSearchHistory : []
            }
            Controls.ComboBox {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.rightMargin: Kirigami.Units.smallSpacing
                currentIndex: settings.ytPreferredType === Settings.YtPreferredType_Audio ? 1 : 0
                model: [
                    qsTr("Type: %1").arg(qsTr("Video")),
                    qsTr("Type: %1").arg(qsTr("Audio"))
                ]
                onCurrentIndexChanged: {
                    if (currentIndex === 1)
                        settings.ytPreferredType = Settings.YtPreferredType_Audio
                    else
                        settings.ytPreferredType = Settings.YtPreferredType_Video
                }
            }
        }
    }

    YtModel {
        id: itemModel
        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
        onProgressChanged: {
            busyIndicator.text = total == 0 ? "" : "" + n + "/" + total
        }
    }

    Component {
        id: sectionHeader
        Kirigami.ListSectionHeader {
            visible: !itemModel.busy
            text: section
        }
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem
            enabled: !itemModel.busy
            label: {
                switch (model.type) {
                case YtModel.Type_Album: return model.album;
                case YtModel.Type_Artist: return model.artist;
                default: return model.name;
                }
            }

            function addDur(text, dur) {
                return text + (dur.length > 0 ? ((text.length > 0 ? " · " : "") + dur) : "");
            }

            subtitle: {
                switch (model.type) {
                case YtModel.Type_Video:
                    return root.artistMode ? addDur(model.album, model.duration) :
                                             addDur(model.artist, model.duration)
                case YtModel.Type_Playlist:
                case YtModel.Type_Album:
                    return addDur(model.artist, model.duration)
                }
                return addDur("", model.duration)
            }
            defaultIconSource: {
                switch (model.type) {
                case YtModel.Type_Artist:
                    return "view-media-artist"
                case YtModel.Type_Album:
                    return "media-album-cover"
                case YtModel.Type_Playlist:
                    return "view-media-playlist"
                }
                return settings.ytPreferredType === Settings.YtPreferredType_Audio ?
                                "emblem-music-symbolic" : "emblem-videos-symbolic"
            }
            attachedIconName: {
                if (model.type !== YtModel.Type_Video || iconSource.length === 0)
                    return ""
                return defaultIconSource
            }
            iconSource: {
                // if (model.type === YtModel.Type_Video && albumMode)
                //     return ""
                return model.icon
            }
            iconSize: Kirigami.Units.iconSizes.medium
            next: model.type !== YtModel.Type_Video

            onClicked: {
                if (model.type === YtModel.Type_Video) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else if (model.type === YtModel.Type_Album || model.type === YtModel.Type_Playlist) {
                    pageStack.pop(root)
                    pageStack.push(Qt.resolvedUrl("YtPage.qml"), {albumPage: model.id, albumType: model.type})
                } else if (model.type === YtModel.Type_Artist) {
                    pageStack.pop(root)
                    pageStack.push(Qt.resolvedUrl("YtPage.qml"), {artistPage: model.id})
                }
            }

            extra: {
                switch (model.type) {
                case YtModel.Type_Album:
                    return qsTr("Album");
                case YtModel.Type_Playlist:
                    return qsTr("Playlist");
                case YtModel.Type_Artist:
                    return qsTr("Artist");
                default: return "";
                }
            }

            highlighted: {
                if (pageStack.currentItem !== root) {
                    var rightPage = app.rightPage(root)
                    if (rightPage && rightPage.objectName === "yt") {
                        if (model.type === YtModel.Type_Album || model.type === YtModel.Type_Playlist)
                            return rightPage.albumPage === model.id
                        if (model.type === YtModel.Type_Artist)
                            return rightPage.artistPage === model.id
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
            anchors.left: parent.left; anchors.right: parent.right
            sourceComponent: listItemComponent
        }

        // footer: ShowmoreItem {
        //     visible: !itemModel.busy && root.featureMode && itemList.count !== 0
        //     onClicked: {
        //         if (root.featureMode) {
        //             pageStack.push(Qt.resolvedUrl("YtPage.qml"),
        //                            {artistPage: itemModel.homeId})
        //         }
        //     }
        // }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: itemList.count === 0 && !itemModel.busy
            text: itemModel.filter.length == 0 && !root.albumMode && !root.artistMode ?
                      qsTr("Type the words to search") : qsTr("No items")
        }

        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: root.homeMode || root.featureMode ? sectionHeader : null
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: itemModel.busy
    }
}
