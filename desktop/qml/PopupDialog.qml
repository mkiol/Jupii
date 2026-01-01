/* Copyright (C) 2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import org.kde.kirigami 2.20 as Kirigami

Dialog {
    id: root

    default property alias content: _column.data
    property alias column: _column
    property real columnWidth: 0

    standardButtons: Dialog.Close
    anchors.centerIn: parent
    bottomInset: Kirigami.Units.largeSpacing
    modal: true

    width: flick.width + root.leftPadding + root.rightMargin
    height: flick.height + root.header.height + root.footer.height + root.topPadding + root.bottomPadding + root.bottomInset

    Component.onCompleted: {
        footer.bottomPadding += bottomInset
    }

    Flickable {
        id: flick

        width: contentWidth
        height: Math.min(contentHeight, root.parent.height -
                         root.header.height - root.footer.height - root.topPadding - root.bottomPadding - root.bottomInset)
        contentWidth: Math.min(_column.implicitWidth, root.parent.width - root.leftPadding - root.rightPadding)
        contentHeight: _column.height
        clip: true

        Keys.onUpPressed: scrollBar.decrease()
        Keys.onDownPressed: scrollBar.increase()
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: _column

            spacing: Kirigami.Units.largeSpacing
            width: Math.min(_column.implicitWidth, root.parent.width - root.leftPadding - root.rightPadding) -
                    (((!root.mirrored && flick.ScrollBar.vertical.visible) ? flick.ScrollBar.vertical.width : root.rightPadding) + x)
            x: (root.mirrored && flick.ScrollBar.vertical.visible) ? flick.ScrollBar.vertical.width : 0

            Item {
                Layout.preferredWidth: root.columnWidth
                Layout.fillWidth: true
            }
        }
    }
}
