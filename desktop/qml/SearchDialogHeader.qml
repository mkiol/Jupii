/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

ColumnLayout {
    id: root
    property var view
    property var recentSearches: []

    anchors.fill: parent
    spacing: 0

    Connections {
        target: root.view.model
        onFilterChanged: searchField.text = root.view.model.filter
    }

    Kirigami.SearchField {
        id: searchField
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        text: root.view.model.filter
        onTextChanged: {
            root.view.model.filter = text.trim()
        }
    }

    Flow {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        Repeater {
            model: recentSearches
            delegate: Component {
                Controls.ItemDelegate {
                    text: modelData
                    onClicked: root.view.model.filter = modelData
                }
            }
        }
    }
}
