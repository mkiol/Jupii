/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.BcModel 1.0

Dialog {
    id: root

    objectName: "bc"

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    property alias albumPage: itemModel.albumUrl
    property alias artistPage: itemModel.artistUrl
    readonly property bool albumMode: albumPage && albumPage.toString().length > 0
    readonly property bool artistMode: artistPage && artistPage.toString().length > 0
    readonly property bool searchMode: !albumMode && !artistMode
    readonly property bool notableMode: artistPage && artistPage === itemModel.notableUrl
    readonly property bool featureMode: root.searchMode && itemModel.filter.length === 0

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
            if (filter.length > 0) settings.addBcSearchHistory(filter)
        }
    }

    BcModel {
        id: itemModel
        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
        onProgressChanged: {
            busyIndicator.progressText = total == 0 ? "" : "" + n + "/" + total
        }
        onBusyChanged: {
            if (!busy) {
                var idx = itemModel.lastIndex();
                if (idx > 2) listView.positionViewAtIndex(idx - 2, ListView.Beginning)
            }
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        footer: ShowMoreItem {
            enabled: !itemModel.busy && ((root.featureMode || root.notableMode) && itemModel.canShowMore)
            onClicked: {
                itemModel.requestMoreItems()
                itemModel.updateModel()
            }
        }

        header: SearchDialogHeader {
            search: root.searchMode
            implicitWidth: root.width
            noSearchCount: -1
            dialog: root
            view: listView
            recentSearches: root.searchMode && itemModel.filter.length === 0 ? settings.bcSearchHistory : []
            sectionHeaderText: root.featureMode ? qsTr("New and Notable") : ""
            onActiveFocusChanged: listView.currentIndex = -1
            onRemoveSearchHistoryClicked: settings.removeBcSearchHistory(value)
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
                case BcModel.Type_Album:
                    return model.album
                case BcModel.Type_Artist:
                    return model.artist
                }
                return model.name
            }
            subtitle.text: {
                if (!model) return ""
                switch (model.type) {
                case BcModel.Type_Track:
                case BcModel.Type_Album:
                    return model.artist
                }
                return ""
            }
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            defaultIcon.source: {
                if (!model) return ""
                switch (model.type) {
                case BcModel.Type_Artist:
                    return "image://theme/icon-m-media-artists?" + primaryColor
                case BcModel.Type_Album:
                    return "image://theme/icon-m-media-albums?" + primaryColor
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
                if (model.type === BcModel.Type_Track) {
                    var selected = model.selected
                    itemModel.setSelected(model.index, !selected);
                } else {
                    pageStack.push(Qt.resolvedUrl("BcPage.qml"),
                                   model.type === BcModel.Type_Album ?
                                       {albumPage: model.url} : {artistPage: model.url})
                }
            }
        }

        ViewPlaceholder {
            verticalOffset: listView.headerItem.height / 2
            enabled: listView.count === 0 && !itemModel.busy
            text: itemModel.filter.length === 0 && !root.albumMode && !root.artistMode ?
                      qsTr("Type the words to search") : root.artistMode ? qsTr("No albums") : qsTr("No items")
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
