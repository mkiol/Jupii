/* Copyright (C) 2024-2026 Michal Kosciesza <michal@mkiol.net>
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
            <li>Bandcamp: Support for Bandcamp Radio shows</li>
            <li>Fix: Currently playing item is not highlighted in the play queue for some Media Servers</li>
            <li>Fix: Adding URL is not possible.</li>
            </ul>"
        }

        SectionLabel {
            text: qsTr("Version %1").arg("2.16.0")
        }

        RichLabel {
            text: "<ul>
            <li>New feature: Slideshow — lets you combine a series of images into a low-framerate video that plays in real time.
                You can use controls to set how long each image is displayed, pause, resume, or rewind slideshow to a specific image.
                Slideshow is an another way to view images, addressing issues found on many TV sets, such as limited or unreliable image-sharing support.</li>
            <li>FOSDEM: Videos from 2025 and 2026 conferences added.</li>
            <li>UI: The settings have two views: <i>Basic options</i> and <i>All options</i>.
                When you select <i>All options</i>, you will have access to many advanced options that allow you to customize streaming parameters and much more.</li>
            <li>UI: Icons now load asynchronously, which makes the user interface smoother.</li>
            <li>UI: Option to move items up/down in the queue.</li>
            <li>Fix: Integration with Bandcamp, Youtube and Soundcloud was broken due to API change.</li>
            <li>Fix: User Interface glitches</li>
            <li>Flatpak: KDE runtime update to version 5.15-25.08</li>
            </ul>"
        }

        SectionLabel {
            text: qsTr("Version %1").arg("2.15.0")
        }

        RichLabel {
            text: "<ul>
            <li>A new setting option that let you switch between different User Interface styles.
                Different styles can improve readability on non-KDE Plasma desktops.</li>
            <li>Menu option for adding all files in the folder and subfolders to the play queue</li>
            <li>Drag and Drop support for adding files and folders to the play queue</li>
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
