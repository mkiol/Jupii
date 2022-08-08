/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
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
    readonly property bool notableMode: artistPage && artistPage.toString() == "jupii://soundcloud-featured"
    readonly property bool featureMode: !itemModel.busy && root.searchMode && itemModel.filter.length === 0 && listView.count > 0

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
            notifications.show(qsTr("Error in getting data from soundcloud.com"))
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
            enabled: !itemModel.busy && (root.featureMode || (root.notableMode && itemModel.canShowMore))
            onClicked: {
                if (root.featureMode) {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"),
                                   {artistPage: "jupii://soundcloud-featured"})
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
            visible: itemModel.selectableCount > 0
            busy: itemModel.busy

            MenuItem {
                enabled: !itemModel.busy
                text: itemModel.selectableCount === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                onClicked: {
                    if (itemModel.selectableCount === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            highlighted: down || model.selected
            title.text: model.type === SoundcloudModel.Type_Track ? model.name :
                        model.type === SoundcloudModel.Type_Album ? model.album :
                        model.type === SoundcloudModel.Type_Artist ? model.artist : ""
            subtitle.text: model.type === SoundcloudModel.Type_Track ||
                           model.type === SoundcloudModel.Type_Album ? model.artist : ""
            dimmed: itemModel.filter.length == 0
            enabled: !itemModel.busy && listView.count > 0
            defaultIcon.source: model.type === SoundcloudModel.Type_Album ?
                                    "image://theme/icon-m-media-albums?" + primaryColor :
                                model.type === SoundcloudModel.Type_Artist ?
                                    "image://theme/icon-m-media-artists?" + primaryColor :
                                "image://theme/icon-m-file-audio?" + primaryColor
            icon.source: model.icon
            extra: model.type === SoundcloudModel.Type_Album ? qsTr("Album") :
                   model.type === SoundcloudModel.Type_Artist ? qsTr("Artist") : ""
            extra2: model.genre

            onClicked: {
                if (model.type === SoundcloudModel.Type_Track) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else {
                    pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"),
                                   model.type === SoundcloudModel.Type_Album ?
                                       {albumPage: model.url} : {artistPage: model.url})
                }
            }
        }

        ViewPlaceholder {
            verticalOffset: listView.headerItem.height / 2
            enabled: listView.count === 0 && !itemModel.busy
            text: itemModel.filter.length == 0 && !root.albumMode && !root.artistMode ?
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
