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

    signal accepted(url url, string name, url icon);

    function doPop() {
        if (pageStack.busy)
            _doPop = true
        else
            pageStack.pop(pageStack.previousPage(root))
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

    SilicaListView {
        id: listView

        //anchors.fill: parent
        width: parent.width
        height: parent.height - (tip.height + 2*Theme.paddingLarge)
        clip: true

        currentIndex: -1

        model: SomafmModel {}

        header: PageHeader {
            title: "SomaFM"
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
                    onClicked: {
                        root.accepted(model.url, model.name, model.icon);
                        root.doPop()
                    }
                }
            }

            onClicked: {
                root.accepted(model.url, model.name, model.icon);
                root.doPop()
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0
            text: qsTr("No channels")
        }
    }

    VerticalScrollDecorator {
        flickable: listView
    }

    Tip {
        id: tip
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.paddingLarge
        anchors.bottomMargin: Theme.paddingLarge
        text: qsTr("SomaFM is supported entirely by the listeners. If you enjoy, please consider making a <a href=\"%1\">donation</a>.").arg("http://somafm.com/support/");
    }
}
