/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.SomafmModel 1.0

Page {
    id: root

    property real preferredItemHeight: root && root.isLandscape ?
                                           Theme.itemSizeSmall :
                                           Theme.itemSizeLarge

    property bool _doPop: false

    signal accepted(url url, string name, url icon, string desc);

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

    SomafmModel {
        id: itemModel
    }

    SilicaListView {
        id: listView

        anchors.fill: parent
        /*width: parent.width
        height: parent.height - (tip.height + 2*Theme.paddingLarge)
        clip: true*/

        opacity: itemModel.busy ? 0.0 : 1.0
        visible: opacity > 0.0
        Behavior on opacity { FadeAnimation {} }

        currentIndex: -1

        model: itemModel

        header: SearchPageHeader {
            implicitWidth: root.width
            title: "SomaFM"
            searchPlaceholderText: qsTr("Search channels")
            model: itemModel
            onActiveFocusChanged: {
                listView.currentIndex = -1
            }
        }

        delegate: DoubleListItem {
            id: listItem
            title.text: model.name
            subtitle.text: model.description
            icon.source: model.icon
            defaultIcon.source: "image://icons/icon-m-browser?" + (highlighted ?
                                    Theme.highlightColor : Theme.primaryColor)

            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Add channel")
                    onClicked: click()
                }
            }

            onClicked: click()

            function click() {
                root.accepted(model.url, model.name,
                              model.icon, model.description)
                root.doPop()
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !itemModel.busy
            text: qsTr("No channels")
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

    /*Tip {
        id: tip
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.paddingLarge
        anchors.bottomMargin: Theme.paddingLarge
        text: qsTr("SomaFM is supported entirely by the listeners. If you enjoy, please consider making a <a href=\"%1\">donation</a>.").arg("http://somafm.com/support/");
    }*/
}
