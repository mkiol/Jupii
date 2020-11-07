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
import QtQuick.Dialogs 1.1

Kirigami.ScrollablePage {
    id: root

    title: qsTr("Settings")

    actions {
        main: Kirigami.Action {
            iconName: "documentinfo"
            text: qsTr("Info")
            checkable: true
            onCheckedChanged: infoSheet.sheetOpen = checked;
            shortcut: "Alt+I"
        }
    }

    onBackRequested: {
        if (infoSheet.sheetOpen) {
            event.accepted = true;
            infoSheet.close();
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a directory for recordings")
        selectMultiple: false
        selectFolder: true
        selectExisting: true
        folder: utils.pathToUrl(_settings.recDir)
        onAccepted: {
            fileDialog.fileUrls.forEach(
                        url => _settings.recDir = utils.urlToPath(url))
        }
    }

    Kirigami.OverlaySheet {
        id: infoSheet

        onSheetOpenChanged: root.actions.main.checked = sheetOpen
        header: Kirigami.Heading {
            text: qsTr("Settings options")
        }
        Controls.Label {
            textFormat: Text.StyledText
            text: "<h4>" + qsTr("Share play queue items via UPnP Media Server") + "</h4>" +
                  "<p>" + qsTr("When enabled, items in Play Queue are accessible " +
                               "for other UPnP devices in your local network.") + "</p>" +
                  "<br/>" +
                  "<h4>" + qsTr("Stream recorder") + "</h4>" +
                  "<p>" + qsTr("Enables audio recording from URL items. " +
                               "If URL item is a Icecast stream, individual tracks " +
                               "from a stream will be recorded. " +
                               "To enable recording use 'Record' button located on the bottom bar. " +
                               "When the 'Record' button is activated before " +
                               "the end of currently played track, the whole track is saved to a file.") + "</p>" +
                  "<br/>" +
                  "<h4>" + qsTr("Stream relaying") + "</h4>" +
                  "<p>" + qsTr("Internet streams are relayed to UPnP device through %1. " +
                               "Recommended option is 'Always' because it provides best " +
                               "compatibility. When relaying is disabled ('Never' option), " +
                               "Icecast titles and Stream recorder " +
                               "are not available.").arg(APP_NAME) + "</p>" +
                  "<br/>" +
                  "<h4>" + qsTr("All devices visible") + "</h4>" +
                  "<p>" + qsTr("All types of UPnP devices are detected " +
                               "and shown, including unsupported devices like " +
                               "home routers. For unsupported devices only " +
                               "basic description information is available. " +
                               "This option might be useful for auditing UPnP devices " +
                               "in your local network.") + "</p>" +
                  "<br/>" +
                  "<h4>" + qsTr("Enable logging") + "</h4>" +
                  "<p>" + qsTr("Needed for troubleshooting purposes. " +
                               "The log data is stored in %1 file.")
                                 .arg(utils.homeDirPath() + "/jupii.log") + "</p>"
            wrapMode: Text.WordWrap
        }
    }

    MessageDialog {
        id: resetDialog
        title: qsTr("Reset settings")
        icon: StandardIcon.Question
        text: qsTr("Reset all settings to defaults?")
        informativeText: qsTr("Restart is required for the changes to take effect.")
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onAccepted: {
            settings.reset()
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            type: Kirigami.MessageType.Warning
            visible: settings.remoteContentMode == 2
            text: qsTr("Icecast titles and Stream recorder are disabled.")
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Controls.Switch {
                checked: settings.contentDirSupported
                text: qsTr("Share play queue items via UPnP Media Server")
                onToggled: {
                    settings.contentDirSupported = !settings.contentDirSupported
                }
            }

            /*Item {
                Kirigami.FormData.isSection: true
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Forward/backward step")
                Controls.Slider {
                    from: 1
                    to: 60
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.forwardTime
                    onValueChanged: {
                        settings.forwardTime = value
                    }
                }
                Controls.Label {
                    text: settings.forwardTime + "s"
                }
            }*/

            Item {
                Kirigami.FormData.isSection: true
            }

            /*Controls.CheckBox {
                checked: settings.useHWVolume
                text: qsTr("Volume control with hardware keys")
                onClicked: {
                    settings.useHWVolume = !settings.useHWVolume
                }
            }*/

            RowLayout {
                Kirigami.FormData.label: qsTr("Volume level step")
                Controls.Slider {
                    from: 1
                    to: 10
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.volStep
                    onValueChanged: {
                        settings.volStep = value
                    }
                }
                Controls.SpinBox {
                    from: 1
                    to: 10
                    value: settings.volStep
                    onValueChanged: {
                        settings.volStep = value
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Microphone sensitivity")
                Controls.Slider {
                    from: 1
                    to: 100
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: Math.round(settings.micVolume)
                    onValueChanged: {
                        settings.micVolume = value
                    }
                }
                Controls.SpinBox {
                    from: 1
                    to: 100
                    value: Math.round(settings.micVolume)
                    onValueChanged: {
                        settings.micVolume = value
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Audio Capture volume boost")
                Controls.Slider {
                    from: 1
                    to: 10
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: Math.round(settings.audioBoost)
                    onValueChanged: {
                        settings.audioBoost = value
                    }
                }
                Controls.SpinBox {
                    from: 1
                    to: 10
                    value: Math.round(settings.audioBoost)
                    onValueChanged: {
                        settings.audioBoost = value
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Screen capture")
                currentIndex: settings.screenAudio ? 1 : 0

                model: [
                    qsTr("Enabled"),
                    qsTr("Enabled with audio")
                ]

                onCurrentIndexChanged: {
                    if (currentIndex === 1) {
                        settings.screenAudio = true;
                    } else {
                        settings.screenAudio = false;
                    }
                }
            }

            Controls.ComboBox {
                enabled: settings.screenSupported
                Kirigami.FormData.label: qsTr("Force screen 16:9 aspect ratio")
                currentIndex: settings.screenCropTo169
                model: [
                   qsTr("Don't force"),
                   qsTr("Scale"),
                   qsTr("Crop")
                ]

                onCurrentIndexChanged: {
                    settings.screenCropTo169 = currentIndex
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Screen capture quality")
                Controls.Slider {
                    enabled: settings.screenSupported
                    from: 1
                    to: 5
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.screenQuality
                    onValueChanged: {
                        settings.screenQuality = value
                    }
                }
                Controls.SpinBox {
                    from: 1
                    to: 5
                    value: settings.screenQuality
                    onValueChanged: {
                        settings.screenQuality = value
                    }
                }
            }

            Controls.ComboBox {
                enabled: settings.screenSupported
                Kirigami.FormData.label: qsTr("Screen capture framerate")
                currentIndex: {
                    if (settings.screenFramerate < 15) {
                        return 0
                    } else if (settings.screenFramerate < 30) {
                        return 1
                    } else {
                        return 2
                    }
                }
                model: [
                    "5 fps",
                    "15 fps",
                    "30 fps"
                ]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.screenFramerate = 5; break;
                    case 1:
                        settings.screenFramerate = 15; break;
                    case 2:
                        settings.screenFramerate = 30; break;
                    default:
                        settings.screenFramerate = 5;
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                enabled:  settings.remoteContentMode == 0
                checked: settings.rec
                text: qsTr("Stream recorder")
                onToggled: {
                    settings.rec = !settings.rec
                }
            }

            Controls.Button {
                enabled: settings.rec && settings.remoteContentMode == 0
                Kirigami.FormData.label: qsTr("Directory for recordings")
                text: utils.dirNameFromPath(settings.recDir)
                onClicked: fileDialog.open()
            }

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("Advanced")
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Preferred network interface")
                currentIndex: utils.prefNetworkInfIndex()
                model: utils.networkInfs()
                onCurrentIndexChanged: {
                    utils.setPrefNetworkInfIndex(currentIndex)
                }

                Connections {
                    target: settings
                    onPrefNetInfChanged: {
                        parent.model = utils.networkInfs()
                        parent.currentIndex = utils.prefNetworkInfIndex()
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Stream relaying")
                currentIndex: {
                    // 0 - proxy for all
                    // 1 - redirection for all
                    // 2 - none for all
                    // 3 - proxy for shoutcast, redirection for others
                    // 4 - proxy for shoutcast, none for others
                    if (settings.remoteContentMode == 0)
                        return 0
                    if (settings.remoteContentMode == 1 || settings.remoteContentMode == 2)
                        return 2
                    if (settings.remoteContentMode == 3 || settings.remoteContentMode == 4)
                        return 1
                    return 0
                }

                model: [
                    qsTr("Always"),
                    qsTr("Only Icecast"),
                    qsTr("Never")
                ]

                onCurrentIndexChanged: {
                    if (currentIndex == 0)
                        settings.remoteContentMode = 0
                    else if (currentIndex == 1)
                        settings.remoteContentMode = 4
                    else if (currentIndex == 2)
                        settings.remoteContentMode = 2
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                enabled: settings.screenSupported
                Kirigami.FormData.label: qsTr("Screen encoder")
                currentIndex: {
                    var enc = settings.screenEncoder;
                    if (enc === "libx264")
                        return 1;
                    if (enc === "libx264rgb")
                        return 2;
                    if (enc === "h264_nvenc")
                        return 3;
                    return 0;
                }
                model: [
                    qsTr("Auto"),
                    "libx264",
                    "libx264rgb",
                    "h264_nvenc"
                ]

                onCurrentIndexChanged: {
                    if (currentIndex === 1)
                        settings.screenEncoder = "libx264";
                    else if (currentIndex === 2)
                        settings.screenEncoder = "libx264rgb";
                    else if (currentIndex === 3)
                        settings.screenEncoder = "h264_nvenc";
                    else
                        settings.screenEncoder = "";
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                checked: settings.showAllDevices
                text: qsTr("All devices visible")
                onToggled: {
                    settings.showAllDevices = !settings.showAllDevices
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                checked: settings.logToFile
                text: qsTr("Enable logging")
                onToggled: {
                    settings.logToFile = !settings.logToFile
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset settings")
                onClicked: {
                    resetDialog.open()
                }
            }
        }
    }
}
