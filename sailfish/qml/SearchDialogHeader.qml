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

    property alias searchPlaceholderText: search.placeholderText
    property int noSearchCount: 10
    property var model
    property var dialog

    implicitHeight: column.height

    signal activeFocusChanged

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
            id: search
            width: parent.width
            visible: model.filter.length !== 0 ||
                     model.count > root.noSearchCount

            placeholderText: qsTr("Search episodes")

            onActiveFocusChanged: {
                if (activeFocus)
                    root.activeFocusChanged()
            }

            onTextChanged: {
                model.filter = text.toLowerCase().trim()
            }
        }
    }
}
