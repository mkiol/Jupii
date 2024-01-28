/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

Kirigami.BasicListItem {
    id: root
    property bool showArrow: true

    trailing: Kirigami.Icon {
        Layout.alignment: Qt.AlignVCenter
        Layout.preferredHeight: Kirigami.Units.iconSizes.small
        opacity: 0.7
        Layout.preferredWidth: Layout.preferredHeight
        source: (LayoutMirroring.enabled ? "go-next-symbolic-rtl" : "go-next-symbolic")
        visible: root.showArrow
    }
}
