/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.5
import Sailfish.Silica 1.0

import harbour.jupii.PlayListModel 1.0

Grid {
    id: root

    property alias nextEnabled: nextButton.enabled
    property alias prevEnabled: prevButton.enabled
    property alias forwardEnabled: forwardButton.enabled
    property alias backwardEnabled: backwardButton.enabled
    property alias playmodeEnabled: playmodeButton.enabled
    property alias recordEnabled: recordButton.enabled
    property int playMode: PlayListModel.PM_RepeatAll

    readonly property real size: pageStack.currentPage.isLandscape ?
                                 Theme.itemSizeExtraSmall : Theme.itemSizeSmall

    signal nextClicked
    signal prevClicked
    signal forwardClicked
    signal backwardClicked
    signal repeatClicked
    signal recordClicked

    columns: 6
    horizontalItemAlignment: pageStack.currentPage.isLandscape ? Grid.AlignRight : Grid.AlignHCenter

    IconButton {
        id: prevButton
        width: parent.size; height: parent.size
        icon.source: "image://theme/icon-m-previous"
        onClicked: root.prevClicked()
    }

    IconButton {
        id: backwardButton
        width: parent.size; height: parent.size
        icon.source: "image://icons/icon-m-backward"
        onClicked: root.backwardClicked()
    }

    IconButton {
        id: forwardButton
        width: parent.size; height: parent.size
        icon.source: "image://icons/icon-m-forward"
        onClicked: root.forwardClicked()
    }

    IconButton {
        id: nextButton
        width: parent.size; height: parent.size
        icon.source: "image://theme/icon-m-next"
        onClicked: root.nextClicked()
    }

    IconButton {
        id: recordButton
        width: parent.size; height: parent.size
        icon.source: recordActive ? "image://icons/icon-m-record-active" : "image://icons/icon-m-record"
        onClicked: recordClicked()
        visible: root.enabled
    }

    IconButton {
        id: playmodeButton
        width: parent.size; height: parent.size
        icon.source: root.playMode === PlayListModel.PM_RepeatAll ? "image://theme/icon-m-repeat" :
                                       root.playMode === PlayListModel.PM_RepeatOne ? "image://theme/icon-m-repeat-single" :
                                       "image://icons/icon-m-norepeat"
        onClicked: root.repeatClicked()
    }
}
