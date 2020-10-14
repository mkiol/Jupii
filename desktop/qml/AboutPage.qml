/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
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

    title: qsTr("About")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing
        RowLayout {
            spacing: Kirigami.Units.largeSpacing
            Layout.fillWidth: true
            Image {
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                sourceSize.width: Kirigami.Units.iconSizes.huge
                sourceSize.height: Kirigami.Units.iconSizes.huge
                source: "qrc:/images/jupii.svg"
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 1
                    wrapMode: Text.WordWrap
                    text: ("%1 %2").arg(APP_NAME).arg(APP_VERSION)
                }
                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 2
                    wrapMode: Text.WordWrap
                    text: "Stream audio, video and image files to UPnP/DLNA devices"
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: qsTr("Copyright")
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            text: ("&copy; %1 %2").arg(COPYRIGHT_YEAR).arg(AUTHOR)
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("%1 is developed as an open source project under %2.")
                  .arg(APP_NAME)
                  .arg("<a href=\"" + LICENSE_URL + "\">" + LICENSE + "</a>")
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: qsTr("Support")
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Project website") + (": <a href=\"%1\">%2</a>")
                  .arg(PAGE).arg(PAGE)
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Support e-mail") + (": <a href=\"%1\">%2</a>")
                  .arg("mailto:" + SUPPORT_EMAIL).arg(SUPPORT_EMAIL)
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: qsTr("Authors")
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            text: ("Copyright &copy; %1 %2").arg(COPYRIGHT_YEAR).arg(AUTHOR)
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: qsTr("Translators")
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignLeft
            text: "Åke Engelbrektson \nCarlos Gonzalez \nd9h20f " +
                  "\nВячеслав Диконов \ndrosjesjaafoer \nRui Kon " +
                  "\nBoštjan Štrumbelj \njgibbon \nFra \nPetr Tsymbarovich"
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            level: 1
            text: qsTr("Libraries in use")
        }

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "QHTTPServer \nLibupnpp \nLibupnp \nTagLib \nFFmpeg \nLAME \nx264"
        }
    }
}
