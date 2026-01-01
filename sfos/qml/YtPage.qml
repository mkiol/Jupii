/* Copyright (C) 2022-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.Settings 1.0
import harbour.jupii.ContentServer 1.0
import harbour.jupii.YtModel 1.0

Dialog {
    id: root

    objectName: "yt"

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    property alias albumPage: itemModel.albumId
    property alias albumType: itemModel.albumType
    property alias artistPage: itemModel.artistId
    readonly property bool albumMode: albumPage && albumPage.length > 0
    readonly property bool artistMode: artistPage && artistPage.length > 0
    readonly property bool searchMode: !albumMode && !artistMode
    readonly property bool homeMode: artistPage && artistPage === itemModel.homeId
    readonly property bool featureMode: root.searchMode && itemModel.filter.length === 0

    canAccept: itemModel.selectedCount > 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    onAccepted: {
        if (settings.ytPreferredType === Settings.YtPreferredType_Audio)
            playlist.addItemUrls(itemModel.selectedItems(), ContentServer.Type_Music)
        else
            playlist.addItemUrls(itemModel.selectedItems())
    }

    Component.onDestruction: addHistory()

    // Hack to update model after all transitions
    property bool _completed: false
    Component.onCompleted: _completed = true
    onStatusChanged: {
        if (status === PageStatus.Active && _completed) {
            _completed = false
            itemModel.updateModel()
        }
    }

    function addHistory() {
        if (searchMode) {
            var filter = itemModel.filter
            if (filter.length > 0) settings.addYtSearchHistory(filter)
        }
    }

    YtModel {
        id: itemModel
        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
        onProgressChanged: {
            busyIndicator.progressText = total == 0 ? "" : "" + n + "/" + total
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        footer: ShowMoreItem {
            enabled: !itemModel.busy && itemModel.filter.length === 0 && root.featureMode
            onClicked: {
                if (root.featureMode) {
                    pageStack.push(Qt.resolvedUrl("YtPage.qml"),
                                   {artistPage: itemModel.homeId})
                }
            }
        }

        header: SearchDialogHeader {
            search: root.searchMode
            implicitWidth: root.width
            noSearchCount: -1
            dialog: root
            view: listView
            recentSearches: root.searchMode && itemModel.filter.length === 0 ? settings.ytSearchHistory : []
            onActiveFocusChanged: listView.currentIndex = -1
            onRemoveSearchHistoryClicked: settings.removeYtSearchHistory(value)
        }

        PullDownMenu {
            id: menu
            busy: itemModel.busy

            MenuItem {
                enabled: itemModel.selectableCount > 0 && !itemModel.busy

                text: itemModel.selectableCount === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                onClicked: {
                    if (itemModel.selectableCount === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }

            MenuItem {
                text: qsTr("Type: %1").arg(
                          settings.ytPreferredType === Settings.YtPreferredType_Audio ?
                              qsTr("Audio") : qsTr("Video"))
                onClicked: {
                    if (settings.ytPreferredType === Settings.YtPreferredType_Audio)
                        settings.ytPreferredType = Settings.YtPreferredType_Video
                    else
                        settings.ytPreferredType = Settings.YtPreferredType_Audio
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            highlighted: down || (model && model.selected)
            title.text: {
                if (!model) return ""
                switch (model.type) {
                case YtModel.Type_Album: return model.album
                case YtModel.Type_Artist: return model.artist
                }
                return model.name
            }

            function addDur(text, dur) {
                return text + (dur.length > 0 ? ((text.length > 0 ? " · " : "") + dur) : "");
            }

            subtitle.text: {
                if (!model) return ""
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
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            defaultIcon.source: {
                if (!model) return ""
                switch (model.type) {
                case YtModel.Type_Playlist:
                    return "image://theme/icon-m-media-playlists?" + primaryColor
                case YtModel.Type_Album:
                    return "image://theme/icon-m-media-albums?" + primaryColor
                case YtModel.Type_Artist:
                    return "image://theme/icon-m-media-artists?" + primaryColor
                }
                return (settings.ytPreferredType === Settings.YtPreferredType_Audio ?
                            "image://theme/icon-m-file-audio?" : "image://theme/icon-m-file-video?")
                        + primaryColor
            }
            attachedIcon.source: {
                if (!model || icon.source == "" || icon.status !== Image.Ready)
                    return ""
                return defaultIcon.source
            }
            icon.source: model ? model.icon : ""
            onClicked: {
                if (!model) return
                if (model.type === YtModel.Type_Video) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else if (model.type === YtModel.Type_Album || model.type === YtModel.Type_Playlist) {
                    pageStack.push(Qt.resolvedUrl("YtPage.qml"), {albumPage: model.id, albumType: model.type})
                } else if (model.type === YtModel.Type_Artist) {
                    pageStack.push(Qt.resolvedUrl("YtPage.qml"), {artistPage: model.id})
                }
            }
        }

        Component {
            id: sectionHeader
            SectionHeader {
                opacity: text.length > 0 && !itemModel.busy ? 1.0 : 0.0
                visible: opacity > 0.0
                Behavior on opacity { FadeAnimation {} }
                text: section
            }
        }

        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: root.featureMode || root.homeMode ? sectionHeader : null

        ViewPlaceholder {
            verticalOffset: listView.headerItem.height / 2
            enabled: listView.count === 0 && !itemModel.busy
            text: itemModel.filter.length === 0 && !root.albumMode && !root.artistMode ?
                      qsTr("Type the words to search") : qsTr("No items")
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        running: itemModel.busy
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
