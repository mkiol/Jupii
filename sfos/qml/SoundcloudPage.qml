/* Copyright (C) 2021-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.SoundcloudModel 1.0

Dialog {
    id: root

    objectName: "soundcloud"

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    property alias albumPage: itemModel.albumUrl
    property alias artistPage: itemModel.artistUrl
    readonly property bool albumMode: albumPage && albumPage.toString().length > 0
    readonly property bool artistMode: artistPage && artistPage.toString().length > 0
    readonly property bool searchMode: !albumMode && !artistMode
    readonly property bool notableMode: artistPage && artistPage === itemModel.featuredUrl
    readonly property bool featureMode: root.searchMode && itemModel.filter.length === 0 && listView.count > 0

    canAccept: itemModel.selectedCount > 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    onAccepted: playlist.addItemUrls(itemModel.selectedItems())

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
            if (filter.length > 0) settings.addSoundcloudSearchHistory(filter)
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
                if (idx > 0) listView.positionViewAtIndex(idx, ListView.Beginning)
            }
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        footer: ShowMoreItem {
            // enabled: !itemModel.busy && itemModel.filter.length === 0 && (root.featureMode || (root.notableMode && itemModel.canShowMore))
            enabled: false
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

        header: SearchDialogHeader {
            search: !root.albumMode && !root.notableMode
            implicitWidth: root.width
            noSearchCount: -1
            dialog: root
            view: listView
            recentSearches: root.searchMode && itemModel.filter.length === 0 ? settings.soundcloudSearchHistory : []
            sectionHeaderText: root.featureMode ? qsTr("Trending tracks") : ""
            onActiveFocusChanged: listView.currentIndex = -1
            onRemoveSearchHistoryClicked: settings.removeSoundcloudSearchHistory(value)
        }

        PullDownMenu {
            id: menu
            visible: itemModel.selectableCount > 0 || root.albumMode || root.artistMode
            busy: itemModel.busy

            MenuItem {
                visible: !root.notableMode && (root.albumMode || root.artistMode)
                text: qsTr("Open website")
                onClicked: Qt.openUrlExternally(root.albumMode ? root.albumPage : root.artistPage)
            }

            MenuItem {
                enabled: !itemModel.busy && itemModel.selectableCount > 0
                text: itemModel.selectableCount === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                onClicked: {
                    if (itemModel.selectableCount === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }

            MenuLabel {
                visible: albumMode || (artistMode && !notableMode)
                text: {
                    var label = itemModel.artistName
                    if (itemModel.albumTitle.length !== 0) {
                        if (label.length !== 0) label += " — " + itemModel.albumTitle
                        else label += itemModel.albumTitle
                    }
                    return label;
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
                case SoundcloudModel.Type_Album:
                case SoundcloudModel.Type_Playlist:
                    return model.album
                case SoundcloudModel.Type_Artist:
                    return model.artist
                }
                return model.name
            }
            subtitle.text: {
                if (!model) return ""
                switch (model.type) {
                case SoundcloudModel.Type_Track:
                case SoundcloudModel.Type_Playlist:
                case SoundcloudModel.Type_Album:
                    return model.artist
                }
                return ""
            }
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            defaultIcon.source: {
                if (!model) return ""
                switch (model.type) {
                case SoundcloudModel.Type_Artist:
                    return "image://theme/icon-m-media-artists?" + primaryColor
                case SoundcloudModel.Type_Album:
                    return "image://theme/icon-m-media-albums?" + primaryColor
                case SoundcloudModel.Type_Playlist:
                    return "image://theme/icon-m-media-playlists?" + primaryColor
                }
                return "image://theme/icon-m-file-audio?" + primaryColor
            }
            attachedIcon.source: {
                if (!model || icon.source.length === 0 || icon.status !== Image.Ready)
                    return ""
                return defaultIcon.source
            }
            icon.source: model ? model.icon : ""
            extra: model ? model.genre : ""
            onClicked: {
                if (!model) return
                if (model.type === SoundcloudModel.Type_Track) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected)
                } else if (model.type === SoundcloudModel.Type_Album || model.type === SoundcloudModel.Type_Playlist) {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {albumPage: model.url})
                } else if (model.type === SoundcloudModel.Type_Artist) {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {artistPage: model.url})
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
        section.delegate: root.notableMode || root.featureMode ? sectionHeader : null

        ViewPlaceholder {
            verticalOffset: listView.headerItem.height / 2
            enabled: listView.count === 0 && !itemModel.busy
            text: itemModel.filter.length === 0 && !root.albumMode && !root.artistMode ?
                      qsTr("Type the words to search") : qsTr("No items")
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
