/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.GpodderEpisodeModel 1.0
import harbour.jupii.AVTransport 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    canAccept: itemModel.selectedCount > 0
    acceptDestination: app.queuePage()
    acceptDestinationAction: PageStackAction.Pop

    onAccepted: {
        playlist.addItemUrls(itemModel.selectedItems())
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

    GpodderEpisodeModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchDialogHeader {
            implicitWidth: root.width
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu

            visible: itemModel.count > 0
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

            highlighted: down || model.selected
            title.text: model.title
            subtitle.text: model.podcastTitle
            enabled: !itemModel.busy && listView.count > 0
            icon.source: model.icon
            defaultIcon.source: {
                switch (model.type) {
                case AVTransport.T_Image:
                    return "image://theme/icon-m-file-image?" + primaryColor
                case AVTransport.T_Audio:
                    return "image://theme/icon-m-file-audio?" + primaryColor
                case AVTransport.T_Video:
                    return "image://theme/icon-m-file-video?" + primaryColor
                default:
                    return "image://theme/icon-m-file-other?" + primaryColor
                }
            }

            onClicked: {
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }
        }

        ViewPlaceholder {
            enabled: itemModel.count === 0 && !itemModel.busy
            text: qsTr("No episodes")
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
