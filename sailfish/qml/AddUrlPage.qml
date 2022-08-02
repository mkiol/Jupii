/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.ContentServer 1.0

Dialog {
    id: root

    allowedOrientations: Orientation.All

    property alias url: urlField.text
    property alias name: nameField.text
    property bool ok: utils.isUrlOk(urlField.text.trim())

    canAccept: ok

    onDone: {
        if (result === DialogResult.Accepted) {
            var type = audioSwitch.checked ? ContentServer.Type_Music :
                          ContentServer.Type_Unknown
            playlist.addItemUrl(url, type, name);
            app.popToQueue()
        }
    }

    Component.onCompleted: {
        if (utils.clipboardContainsUrl())
            urlField.text = utils.clipboard()
    }

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            DialogHeader {
                acceptText: qsTr("Add URL")
            }

            TextField {
                id: urlField
                width: parent.width
                placeholderText: qsTr("Enter URL")
                label: qsTr("URL")
                wrapMode: TextInput.WrapAnywhere
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText

                EnterKey.iconSource: nameField.text.length > 0 && root.ok ?
                                         "image://theme/icon-m-enter-accept" :
                                         "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (nameField.text.length > 0 && root.ok)
                        root.accept();
                    else
                        nameField.forceActiveFocus();
                }

                rightItem: IconButton {
                    onClicked: urlField.text = ""
                    width: icon.width
                    height: icon.height
                    icon.source: "image://theme/icon-m-input-clear"
                    opacity: urlField.text.length > 0 ? 1.0 : 0.0
                    Behavior on opacity { FadeAnimation {} }
                }
            }

            TextField {
                id: nameField
                width: parent.width
                placeholderText: qsTr("Enter Name (optional)")
                label: qsTr("Name")
                wrapMode: TextInput.WrapAnywhere
                EnterKey.iconSource: root.ok ? "image://theme/icon-m-enter-accept" :
                                               "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (ok)
                        root.accept();
                    else
                        urlField.forceActiveFocus()
                }

                rightItem: IconButton {
                    onClicked: nameField.text = ""
                    width: icon.width
                    height: icon.height
                    icon.source: "image://theme/icon-m-input-clear"
                    opacity: nameField.text.length > 0 ? 1.0 : 0.0
                    Behavior on opacity { FadeAnimation {} }
                }
            }

            TextSwitch {
                id: audioSwitch
                width: parent.width
                text: qsTr("Add only audio stream")
            }

            Tip {
                text: qsTr("Only HTTP/HTTPS URLs are supported. If URL points to a playlist file, " +
                           "first playlist item will be added. If URL doesn't point to any media content, " +
                           "youtube-dl will be used to find a direct media URL.")
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
