/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.GpodderPodcastModel 1.0

Page {
    id: root

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property bool _doPop: false

    signal accepted(var items);

    function doPop() {
        if (pageStack.busy)
            _doPop = true
        else
            pageStack.pop(pageStack.previousPage(root))
    }

    // Hack to update model after all transitions
    property bool _completed: false
    Component.onCompleted: _completed = true
    onStatusChanged: {
        if (status === PageStatus.Active && _completed) {
            _completed = false
            itemModel.updateModel()
        }
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && root._doPop) {
                root._doPop = false
                pageStack.pop()
            }
        }
    }

    GpodderPodcastModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchPageHeader {
            implicitWidth: root.width
            title: qsTr("Podcasts")
            searchPlaceholderText: qsTr("Search podcasts")
            //tip: qsTr("Only podcasts that contain at least one downloaded episode are visible.")
            model: itemModel
            view: listView
        }

        delegate: DoubleListItem {
            id: listItem
            title.text: model.title
            //subtitle.text: model.description
            enabled: !itemModel.busy
            icon.source: model.icon
            defaultIcon.source: "image://theme/icon-m-media-albums?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Select episodes")
                    onClicked: click()
                }
            }

            onClicked: click()

            function click() {
                var dialog = pageStack.push(Qt.resolvedUrl("GpodderEpisodesPage.qml"),{podcastId: model.id})
                dialog.accepted.connect(function() {
                    root.accepted(dialog.selectedItems)
                    root.doPop()
                })
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No podcasts")
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
}
