/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
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
                text: qsTr("Version %1").arg(APP_VERSION)
            }

            LogItem {
                title: "<ul>
                <li>Bandcamp: Support for Bandcamp Radio shows</li>
                <li>Fix: Currently playing item is not highlighted in the play queue for some Media Servers</li>
                </ul>"
            }

            SectionHeader {
                text: qsTr("Version %1").arg("2.16.0")
            }

            LogItem {
                title: "<ul>
                <li>New feature: Slideshow — lets you combine a series of images into a low-framerate video that plays in real time.
                    You can use controls to set how long each image is displayed, pause, resume, or rewind slideshow to a specific image.
                    Slideshow is an another way to view images, addressing issues found on many TV sets, such as limited or unreliable image-sharing support.
                    In the <i>Add</i> menu, the <i>Slideshow</i> option allows you to add automatically created slideshows
                    from <i>Today's images</i> or <i>Images from last 7 days</i> to the playback queue.
                    You can also create your own slideshow by selecting individual images.</li>
                <li>FOSDEM: Videos from 2025 and 2026 conferences added.</li>
                <li>UI: Icons now load asynchronously, which makes the user interface smoother.</li>
                <li>UI: The settings have two views: <i>Basic options</i> and <i>All options</i>.
                    When you select <i>All options</i>, you will have access to many advanced options that allow you to customize streaming parameters and much more.</li>
                <li>UI: Option to move items up/down in the queue.</li>
                <li>Youtube: Support for Youtube has been disabled for now.
                    Libraries providing integration (ytmusicapi, yt-dlp) require a higher version of Python than the one currently available in SFOS.
                    Youtube support will be re-enabled after updating SFOS to next version.</li>
                <li>Screen capture: Performance has been improved, offering greater stability and higher framerate.
                <li>Fix: Integration with Bandcamp and Soundcloud was broken due to API change.</li>
                <li>Fix: User Interface glitches</li>
                </ul>"
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
