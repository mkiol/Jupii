/* Copyright (C) 2017-2020 Michal Kosciesza <michal@mkiol.net>
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
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("About")
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                source: "image://icons/icon-i-jupii"
            }

            InfoLabel {
                text: APP_NAME
            }

            PaddedLabel {
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeMedium
                color: Theme.highlightColor
                text: qsTr("Version %1").arg(APP_VERSION);
            }

            Button {
                text: qsTr("Changes")
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: pageStack.push(Qt.resolvedUrl("ChangelogPage.qml"))
            }

            Button {
                text: qsTr("Project website")
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: Qt.openUrlExternally(PAGE)
            }

            SectionHeader {
                text: qsTr("Authors")
            }

            PaddedLabel {
                horizontalAlignment: Text.AlignLeft
                textFormat: Text.RichText
                text: ("Copyright &copy; %1 %2")
                .arg(COPYRIGHT_YEAR)
                .arg(AUTHOR)
            }

            PaddedLabel {
                horizontalAlignment: Text.AlignLeft
                textFormat: Text.StyledText
                text: qsTr("%1 is developed as an open source project under %2.")
                .arg(APP_NAME)
                .arg("<a href=\"" + LICENSE_URL + "\">" + LICENSE + "</a>")
            }

            SectionHeader {
                text: qsTr("Translators")
            }

            PaddedLabel {
                horizontalAlignment: Text.AlignLeft
                text: "Åke Engelbrektson \nCarlos Gonzalez \nd9h20f " +
                      "\nВячеслав Диконов \ndrosjesjaafoer \nRui Kon " +
                      "\nBoštjan Štrumbelj \njgibbon \nFra \nPetr Tsymbarovich"
            }

            SectionHeader {
                text: qsTr("Libraries in use")
            }

            PaddedLabel {
                horizontalAlignment: Text.AlignLeft
                text: "QHTTPServer \nLibupnpp \nLibupnp \nTagLib \nFFmpeg \nLAME \nx264 \ngumbo-parser"
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}
