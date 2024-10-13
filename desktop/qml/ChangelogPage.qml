/* Copyright (C) 2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

Kirigami.ScrollablePage {
    id: root

    Kirigami.Theme.colorSet: Kirigami.Theme.Window

    title: qsTr("Changes")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        SectionLabel {
            text: qsTr("Version %1").arg(APP_VERSION)
        }

        RichLabel {
            text: "<ul>
            <li>A new option that let you switch between different User Interface styles.
                Different styles can improve readability on non-KDE Plasma desktops.</li>
            <li>User Interface translated into Estonian language</li>
            <li>Fix: YouTube Music browser didn't work due to YouTube API change.</li>
            <li>Fix: Bandcamp search didn't work due to Bandcamp API change.</li>
            <li>Fix: Thumbnails of FLAC files were not displayed correctly.</li>
            <li>Fix: Track duration was not displayed when viewing Media Server content.</li>
            <li>Fix: User Interface glitches</li>
            </ul>"
        }
    }
}
