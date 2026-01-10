/* Copyright (C) 2025-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.SlidesModel 1.0
import harbour.jupii.Settings 1.0

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
    Component.onCompleted: {
        _completed = true

    }
    onStatusChanged: {
        if (status === PageStatus.Active && _completed) {
            _completed = false
            itemModel.updateModel()
        }
    }

    SlidesModel {
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
            busy: itemModel.busy

//            MenuItem {
//                visible: enabled
//                enabled: listView.count > 0 && itemModel.selectedCount > 0 && !itemModel.busy
//                text: qsTr("Delete selected")
//                onClicked: {
//                    Remorse.popupAction(root,
//                        qsTr("Deleting %n item(s)", "", itemModel.selectedCount),
//                        function(){itemModel.deleteSelected()})
//                }
//            }

            MenuItem {
                visible: enabled
                enabled: listView.count > 0 && !itemModel.busy
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
                visible: enabled
                enabled: listView.count > 0
                text: qsTr("Sort by: %1")
                        .arg(itemModel.queryType === 0 ? qsTr("Last edit time") :
                        qsTr("Title"));
                onClicked: {
                    var m = itemModel.queryType + 1
                    if (m >= 2)
                        m = 0
                    itemModel.queryType = m
                }
            }

            MenuItem {
                text: qsTr("Create slideshow")
                enabled: !itemModel.busy
                onClicked: {
                    itemModel.setAllSelected(false)
                    pageStack.push(Qt.resolvedUrl("CreateSlidesDialog.qml"), {"model": itemModel});
                }
            }
        }

        Component {
            id: contextMenuCmp
            ContextMenu {
                property string modelId: model.id
                MenuItem {
                    text: qsTr("Edit")
                    onClicked: {
                        itemModel.setAllSelected(false)
                        var mid = modelId
                        pageStack.push(Qt.resolvedUrl("CreateSlidesDialog.qml"),
                            {"model": itemModel, "slidesId": mid})
                    }
                }
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: {
                        itemModel.setAllSelected(false)
                        var mid = modelId
                        Remorse.popupAction(root,
                            qsTr("Deleting %n item(s)", "", 1),
                            function(){itemModel.deleteItem(mid)})
                    }
                }
            }
        }

        delegate: DoubleListItem {
            property color primaryColor: highlighted ?
                                         Theme.highlightColor : Theme.primaryColor
            property bool autoItem: model.size === 0
            highlighted: down || (model && model.selected)
            title.text: model ? model.title : ""
            subtitle.text: {
                if (!model || autoItem) return ""
                var date = model.friendlyDate
                return qsTr("%n image(s)", "", model.size) + (date.length > 0 ? " · " + date : "")
            }
            dimmed: listView.count > 0
            enabled: !itemModel.busy && listView.count > 0
            icon.source: model ? model.icon : ""
            defaultIcon.source: "image://icons/icon-m-slidesitem?" + primaryColor
            attachedIcon.source: {
                return "image://theme/icon-m-file-video?" + primaryColor
            }

            onClicked: {
                if (!model) return
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }

            Component.onCompleted: {
                if (autoItem) return
                menu = contextMenuCmp.createObject(this, {modelId: model.id})
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No slideshows")
            hintText: qsTr("Pull down to create a slideshow")
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

    HintTip {
        hintType: Settings.Hint_SlidesTip
        text: qsTr("A slideshow allows you to combine a set of images into a real-time video.") + " " +
              qsTr("Using the controls, you can interactively set the display time for each image, pause, resume, or rewind to a specific image.")
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
