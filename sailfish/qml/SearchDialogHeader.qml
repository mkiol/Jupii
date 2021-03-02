/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

FocusScope {
    id: root

    property alias searchPlaceholderText: searchField.placeholderText
    property int noSearchCount: 10
    property bool search: true
    property var model
    property var dialog
    property var view

    height: column.height

    onHeightChanged: view.scrollToTop()

    opacity: enabled ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation {} }

    Column {
        id: column
        width: parent.width

        DialogHeader {
            id: header
            dialog: root.dialog
            spacing: 0

            acceptText: {
                var count = model.selectedCount
                return count > 0 ? qsTr("%n selected", "", count) : header.defaultAcceptText
            }
        }

        SearchField {
            id: searchField
            width: parent.width
            visible: root.search && (model.filter.length !== 0 ||
                     model.count > root.noSearchCount)
            text: model.filter

            onActiveFocusChanged: {
                if (activeFocus)
                    root.view.currentIndex = -1
            }

            onTextChanged: {
                model.filter = text.trim()
            }
        }
    }
}
