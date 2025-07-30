/* Copyright (C) 2017-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: root

    allowedOrientations: Orientation.All

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: content.height

        Column {
            id: content

            width: root.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Changes")
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.15")
            }

            LogItem {
                title: "<ul>
                <li>User Interface translated into Estonian language</li>
                <li>Fix: YouTube Music browser didn't work due to YouTube API change.</li>
                <li>Fix: Bandcamp search didn't work due to Bandcamp API change.</li>
                <li>Fix: Thumbnails of FLAC files were not displayed correctly.</li>
                <li>Fix: Track duration was not displayed when viewing Media Server content.</li>
                </ul>"
            }

            Spacer {}
        }
    }

    VerticalScrollDecorator {
        flickable: flick
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
