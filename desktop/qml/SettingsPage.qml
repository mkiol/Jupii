/* Copyright (C) 2020-2024 Michal Kosciesza <michal@mkiol.net>
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

import org.mkiol.jupii.Settings 1.0

Kirigami.ScrollablePage {
    id: root

    title: qsTr("Settings")

    FileDialog {
        id: recDirDialog

        title: qsTr("Choose a directory for recordings")
        selectMultiple: false
        selectFolder: true
        selectExisting: true
        folder: utils.pathToUrl(_settings.recDir)
        onAccepted: {
            recDirDialog.fileUrls.forEach(function(url) {_settings.recDir = utils.urlToPath(url)})
        }
    }

    MessageDialog {
        id: resetDialog
        title: qsTr("Reset settings")
        icon: StandardIcon.Question
        text: qsTr("Reset all settings to defaults?")
        informativeText: qsTr("Restart is required for the changes to take effect.")
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onAccepted: settings.reset()
    }

    ColumnLayout {
        width: parent.width
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            type: Kirigami.MessageType.Information
            visible: settings.restartRequired
            text: qsTr("Restart is required for the changes to take effect.")
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Controls.Switch {
                checked: settings.contentDirSupported
                text: qsTr("Share play queue items via UPnP Media Server")
                onToggled: {
                    settings.contentDirSupported = !settings.contentDirSupported
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("When enabled, items in play queue are accessible " +
                                            "for other UPnP devices in your local network.")
                hoverEnabled: true
            }

            // RowLayout {
            //     Kirigami.FormData.label: qsTr("Volume level step")
            //     Controls.Slider {
            //         from: 1
            //         to: 10
            //         stepSize: 1
            //         snapMode: Controls.Slider.SnapAlways
            //         value: settings.volStep
            //         onValueChanged: {
            //             settings.volStep = value
            //         }
            //     }
            //     Controls.SpinBox {
            //         from: 1
            //         to: 10
            //         value: settings.volStep
            //         onValueChanged: {
            //             settings.volStep = value
            //         }
            //     }
            // }

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("Formats")
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Live video")
                currentIndex: {
                    switch (settings.casterVideoStreamFormat) {
                    case Settings.CasterStreamFormat_MpegTs: return 0;
                    case Settings.CasterStreamFormat_Mp4: return 1;
                    }
                    return 0;
                }

                model: [
                    "MPEG-TS",
                    "MP4",
                ]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_MpegTs; break;
                    case 1: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_Mp4; break;
                    default: settings.casterVideoStreamFormat = Settings.CasterStreamFormat_MpegTs;
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Change if you observe problems with video playback in Camera or Screen capture.")
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Live audio")
                currentIndex: {
                    switch (settings.casterAudioStreamFormat) {
                    case Settings.CasterStreamFormat_Mp3: return 0;
                    case Settings.CasterStreamFormat_Mp4: return 1;
                    case Settings.CasterStreamFormat_MpegTs: return 2;
                    }
                    return 0;
                }

                model: [
                    "MP3",
                    "MP4",
                    "MPEG-TS"
                ]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3; break;
                    case 1: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp4; break;
                    case 2: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_MpegTs; break;
                    default: settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3;
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Change if you observe problems with audio playback in Microphone or Audio capture.")
            }

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("Recorder")
                Kirigami.FormData.isSection: true
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Directory for recordings")

                Controls.TextField {
                    Layout.fillWidth: true
                    readOnly: true
                    inputMethodHints: Qt.ImhNoPredictiveText
                    text: settings.recDir
                }

                Controls.Button {
                    Layout.alignment: Qt.AlignLeft
                    icon.name: "edit-clear-symbolic"
                    onClicked: settings.recDir = ""

                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.text: qsTr("Set default")
                    hoverEnabled: true
                }

                Controls.Button {
                    text: qsTr("Change")
                    onClicked: recDirDialog.open()
                }
            }

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("Caching")
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Cache remote content")
                currentIndex: {
                    if (settings.cacheType === Settings.Cache_Auto)
                        return 0
                    if (settings.cacheType === Settings.Cache_Always)
                        return 1
                    if (settings.cacheType === Settings.Cache_Never)
                        return 2
                    return 0
                }

                model: [
                    qsTr("Auto"),
                    qsTr("Always"),
                    qsTr("Never")
                ]

                onCurrentIndexChanged: {
                    if (currentIndex == 0)
                        settings.cacheType = Settings.Cache_Auto
                    else if (currentIndex == 1)
                        settings.cacheType = Settings.Cache_Always
                    else if (currentIndex == 2)
                        settings.cacheType = Settings.Cache_Never
                    else
                        settings.cacheType = Settings.Cache_Auto
                }
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Cache cleaning")
                currentIndex: {
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Auto)
                        return 0
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Always)
                        return 1
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Never)
                        return 2
                    return 0
                }

                model: [
                    qsTr("Auto"),
                    qsTr("Always"),
                    qsTr("Never")
                ]

                onCurrentIndexChanged: {
                    if (currentIndex == 0)
                        settings.cacheCleaningType = Settings.CacheCleaning_Auto
                    else if (currentIndex == 1)
                        settings.cacheCleaningType = Settings.CacheCleaning_Always
                    else if (currentIndex == 2)
                        settings.cacheCleaningType = Settings.CacheCleaning_Never
                    else
                        settings.cacheCleaningType = Settings.CacheCleaning_Auto
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Cache size")
                Controls.Label {
                    id: sizeLabel
                    text: cserver.cacheSizeString()
                    Connections {
                        target: cserver
                        onCacheSizeChanged: sizeLabel.text = cserver.cacheSizeString()
                    }
                }
                Controls.Button {
                    text: qsTr("Delete cache")
                    onClicked: cserver.cleanCache()
                }
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

            Controls.Switch {
                checked: !settings.allowNotIsomMp4
                text: qsTr("Block fragmented MP4 audio streams")
                onToggled: {
                    settings.allowNotIsomMp4 = !settings.allowNotIsomMp4
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Some UPnP devices don't support audio stream in fragmented MP4 format. " +
                                            "This kind of stream might even hang a device. " +
                                            "To overcome this problem, Jupii tries to re-transcode stream to standard MP4. " +
                                            "When re-transcoding fails and this option is enabled, item will not be played at all.")
                hoverEnabled: true
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Video encoder")
                currentIndex: {
                    switch (settings.casterVideoEncoder) {
                    case Settings.CasterVideoEncoder_Auto: return 0;
                    case Settings.CasterVideoEncoder_X264: return 1;
                    case Settings.CasterVideoEncoder_Nvenc: return 2;
                    case Settings.CasterVideoEncoder_V4l2: return 3;
                    }
                    return 0;
                }

                model: [
                    qsTr("Auto"),
                    "x264",
                    "nvenc",
                    "V4L2"
                ]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto; break;
                    case 1: settings.casterVideoEncoder = Settings.CasterVideoEncoder_X264; break;
                    case 2: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Nvenc; break;
                    case 3: settings.casterVideoEncoder = Settings.CasterVideoEncoder_V4l2; break;
                    default: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto;
                    }
                }
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            Controls.TextField {
                Kirigami.FormData.label: qsTr("Frontier Silicon PIN")
                inputMethodHints: Qt.ImhDigitsOnly
                text: settings.fsapiPin
                placeholderText: qsTr("Enter Frontier Silicon PIN")
                onEditingFinished: {
                    settings.fsapiPin = text
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

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("All types of UPnP devices are detected " +
                                            "and shown, including unsupported devices like " +
                                            "home routers. For unsupported devices only " +
                                            "basic description information is available. " +
                                            "This option might be useful for auditing UPnP devices " +
                                            "in your local network.")
                hoverEnabled: true
            }

            Item {
                Kirigami.FormData.isSection: true
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Location of Python libraries")
                visible: !settings.isFlatpak()

                Controls.TextField {
                    id: pyTextField
                    inputMethodHints: Qt.ImhNoPredictiveText
                    text: settings.pyPath

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("Python libraries directory (%1).").arg("<i>PYTHONPATH</i>") + " " +
                                           qsTr("Leave blank to use the default value.") + " " +
                                           qsTr("This option may be useful if you use %1 module to manage Python libraries.").arg("<i>venv</i>")
                }

                Controls.Button {
                    Layout.alignment: Qt.AlignLeft
                    icon.name: "edit-clear-symbolic"
                    onClicked: settings.pyPath = ""

                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.text: qsTr("Set default")
                    hoverEnabled: true
                }

                Controls.Button {
                    text: qsTr("Save")
                    onClicked: settings.pyPath = pyTextField.text

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("Save changes")
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

                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Needed for troubleshooting purposes. " +
                                            "The log data is stored in %1 file.")
                                              .arg(settings.getCacheDir() + "/jupii.log")
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
