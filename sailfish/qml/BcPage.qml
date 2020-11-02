/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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

    allowedOrientations: Orientation.All

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge
    property alias albumPage: itemModel.albumUrl
    property alias artistPage: itemModel.artistUrl
    readonly property bool albumMode: albumPage && albumPage.toString().length > 0
    readonly property bool artistMode: artistPage && artistPage.toString().length > 0

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

    BcModel {
        id: itemModel
        onError: {
            notification.show(qsTr("Error in getting data from bandcamp.com"))
        }
    }

    SilicaListView {
        id: listView

        anchors.fill: parent

        currentIndex: -1

        model: itemModel

        /*title: itemModel.albumTitle.length > 0 ? itemModel.albumTitle :
               itemModel.artistName.length > 0 ? itemModel.artistName : qsTr("Bandcamp")*/

        header: SearchDialogHeader {
            search: !root.artistMode && !root.albumMode
            implicitWidth: root.width
            noSearchCount: -1
            searchPlaceholderText: qsTr("Search items")
            model: itemModel
            dialog: root
            view: listView

            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        PullDownMenu {
            id: menu
            visible: itemModel.selectableCount > 0
            busy: itemModel.busy

            MenuItem {
                visible: itemModel.selectableCount > 0

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
            title.text: model.type === BcModel.Type_Track ? model.name :
                        model.type === BcModel.Type_Album ? model.album :
                        model.type === BcModel.Type_Artist ? model.artist : ""
            subtitle.text: model.type === BcModel.Type_Track ||
                           model.type === BcModel.Type_Album ? model.artist : ""
            enabled: !itemModel.busy
            defaultIcon.source: model.type === BcModel.Type_Album ?
                                    "image://theme/icon-m-media-albums?" + primaryColor :
                                model.type === BcModel.Type_Artist ?
                                    "image://theme/icon-m-media-artists?" + primaryColor :
                                "image://theme/icon-m-file-audio?" + primaryColor
            icon.source: model.icon
            extra: model.type === BcModel.Type_Album ? qsTr("album") :
                   model.type === BcModel.Type_Artist ? qsTr("artist") : ""

            onClicked: {
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
