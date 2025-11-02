/* Copyright (C) 2020-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.TuneinModel 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
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
        var filter = itemModel.filter
        if (filter.length > 0) settings.addTuneinSearchHistory(filter)
    }

    TuneinModel {
        id: itemModel

        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchDialogHeader {
            implicitWidth: root.width
            noSearchCount: -1
            dialog: root
            view: listView
            recentSearches: itemModel.filter.length === 0 ? settings.tuneinSearchHistory : []
            sectionHeaderText: listView.count > 0 && recentSearches.length > 0 ? qsTr("Radio stations") : ""
            onActiveFocusChanged: listView.currentIndex = -1
            onRemoveSearchHistoryClicked: settings.removeTuneinSearchHistory(value)
        }

        PullDownMenu {
            id: menu
            visible: itemModel.selectableCount > 0
            busy: itemModel.busy

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
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            highlighted: down || (model && model.selected)
            title.text: model ? model.name : ""
            subtitle.text: model ? model.description : ""
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            icon.source: model ? model.icon : ""
            defaultIcon.source: "image://theme/icon-m-file-audio?" + primaryColor
            attachedIcon.source: {
                if (!model || icon.source == "" || icon.status !== Image.Ready)
                    return ""
                return defaultIcon.source
            }
            onClicked: {
                if (!model) return
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }
        }

        ViewPlaceholder {
            verticalOffset: listView.headerItem.height / 2
            enabled: listView.count === 0 && !itemModel.busy
            text: itemModel.filter.length === 0 ?
                      qsTr("Type the words to search") : qsTr("No stations")
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
