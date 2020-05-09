/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.RecModel 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property var selectedItems

    canAccept: itemModel.selectedCount > 0

    onDone: {
        if (result === DialogResult.Accepted)
            selectedItems = itemModel.selectedItems()
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

    RecModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        header: SearchDialogHeader {
            implicitWidth: root.width
            searchPlaceholderText: qsTr("Search recordings")
            model: itemModel
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu
            busy: itemModel.busy
            visible: itemModel.count !== 0

            MenuItem {
                visible: itemModel.selectedCount > 0
                text: qsTr("Delete selected")
                onClicked: {
                    Remorse.popupAction(root,
                        qsTr("Deleting %n item(s)", "", itemModel.selectedCount),
                        function(){itemModel.deleteSelected()})
                }
            }

            MenuItem {
                visible: itemModel.count !== 0

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

            MenuItem {
                text: qsTr("Sort by: %1")
                        .arg(itemModel.queryType == 0 ? qsTr("Recording time") :
                        itemModel.queryType == 1 ? qsTr("Title") : qsTr("Author"));
                onClicked: {
                    var m = itemModel.queryType + 1
                    if (m >= 3)
                        m = 0
                    itemModel.queryType = m
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            highlighted: down || model.selected
            title.text: model.title
            subtitle.text: {
                var date = model.friendlyDate
                var author = model.author
                if (author.length > 0 && date.length > 0)
                    return author + " · " + date
                if (date.length > 0)
                    return date
                if (author.length > 0)
                    return author
            }
            enabled: !itemModel.busy
            icon.source: model.icon
            defaultIcon.source: "image://theme/icon-m-file-audio?" + primaryColor

            onClicked: {
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No recordings")
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
