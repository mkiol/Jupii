/* Copyright (C) 2020-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.19 as Kirigami
import QtQuick.Dialogs 1.1

import org.mkiol.jupii.Settings 1.0

Kirigami.ScrollablePage {
    id: root

    property bool showAdvanced: false

    title: qsTr("Settings")

    header: Controls.ToolBar {
        RowLayout {
            anchors.fill: parent
            Controls.ComboBox {
                Layout.fillWidth: true
                currentIndex: 0
                model: [qsTr("Basic options"), qsTr("All options")]
                onCurrentIndexChanged: {
                    root.showAdvanced = (currentIndex === 1)
                }
            }
        }
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

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("User Interface")
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                checked: !settings.qtStyleAuto
                text: qsTr("Use custom graphical style")
                onToggled: {
                    settings.qtStyleAuto = !checked;
                }

                hoverEnabled: true
            }

            Controls.ComboBox {
                enabled: !settings.qtStyleAuto
                Kirigami.FormData.label: qsTr("Graphical style")
                currentIndex: settings.qtStyleIdx
                model: settings.qtStyles()
                onActivated: settings.qtStyleIdx = index

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Application graphical interface style.") + " " + qsTr("Change if you observe problems with incorrect colors under a dark theme.")
                hoverEnabled: true
            }

            Kirigami.Separator {
                Kirigami.FormData.label: "UPnP"
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                checked: settings.contentDirSupported
                text: qsTr("Share play queue items via UPnP Media Server")
                onToggled: {
                    settings.contentDirSupported = !settings.contentDirSupported;
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("When enabled, items in play queue are accessible " + "for other UPnP devices in your local network.")
                hoverEnabled: true
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Gapless mode %1").arg("(<i>setNextURI</i>)")
                currentIndex: {
                    switch (settings.avNextUriPolicy) {
                    case Settings.AvNextUriPolicy_Auto:
                        return 0
                    case Settings.AvNextUriPolicy_DisableOnlyIfNotSupported:
                        return 1
                    case Settings.AvNextUriPolicy_NeverDisable:
                        return 2
                    case Settings.AvNextUriPolicy_AlwaysDisable:
                        return 3
                    }
                    return 0
                }

                model: [qsTr("Auto"), qsTr("Disable only if not supported"), qsTr("Always enabled"), qsTr("Always disabled")]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.avNextUriPolicy = Settings.AvNextUriPolicy_Auto;
                        break
                    case 1:
                        settings.avNextUriPolicy = Settings.AvNextUriPolicy_DisableOnlyIfNotSupported;
                        break
                    case 2:
                        settings.avNextUriPolicy = Settings.AvNextUriPolicy_NeverDisable;
                        break
                    case 3:
                        settings.avNextUriPolicy = Settings.AvNextUriPolicy_AlwaysDisable;
                        break
                    default:
                        settings.avNextUriPolicy = Settings.AvNextUriPolicy_Auto;
                    }
                }
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Relay policy")
                currentIndex: {
                    switch (settings.relayPolicy) {
                    case Settings.RelayPolicy_Auto:
                        return 0
                    case Settings.RelayPolicy_AlwaysRelay:
                        return 1
                    case Settings.RelayPolicy_DontRelayUpnp:
                        return 2
                    }
                    return 0
                }

                model: [qsTr("Auto"), qsTr("Always relay"), qsTr("Don't relay from Media Server")]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.relayPolicy = Settings.RelayPolicy_Auto;
                        break
                    case 1:
                        settings.relayPolicy = Settings.RelayPolicy_AlwaysRelay;
                        break
                    case 2:
                        settings.relayPolicy = Settings.RelayPolicy_DontRelayUpnp;
                        break
                    default:
                        settings.relayPolicy = Settings.RelayPolicy_Auto;
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Determines whether multimedia are routed through Jupii.")
                hoverEnabled: true
            }

            Kirigami.Separator {
                Kirigami.FormData.label: qsTr("Slideshow")
                Kirigami.FormData.isSection: true
            }

            Controls.Switch {
                checked: settings.slidesShowCountInd
                text: qsTr("Show slide number")
                onToggled: {
                    settings.slidesShowCountInd = !settings.slidesShowCountInd;
                }
            }

            Controls.Switch {
                checked: settings.slidesShowProgrInd
                text: qsTr("Show progress bar")
                onToggled: {
                    settings.slidesShowProgrInd = !settings.slidesShowProgrInd;
                }
            }

            Controls.Switch {
                checked: settings.slidesShowDateInd
                text: qsTr("Show date & time")
                onToggled: {
                    settings.slidesShowDateInd = !settings.slidesShowDateInd;
                }
            }

            Controls.Switch {
                checked: settings.slidesShowCameraInd
                text: qsTr("Show camera model")
                onToggled: {
                    settings.slidesShowCameraInd = !settings.slidesShowCameraInd;
                }
            }

            Controls.Switch {
                checked: settings.imagePaused
                text: qsTr("Pause slideshow")
                onToggled: {
                    settings.imagePaused = !settings.imagePaused;
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Image display time (seconds)")
                property string description: qsTr("Change to adjust how long the image is displayed.")
                Controls.Slider {
                    from: 1
                    to: 120
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.imageDuration
                    onValueChanged: {
                        settings.imageDuration = value;
                    }

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: parent.description
                    hoverEnabled: true
                }
                Controls.SpinBox {
                    from: 0
                    to: 120
                    value: settings.imageDuration
                    onValueChanged: {
                        settings.imageDuration = value;
                    }

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: parent.description
                    hoverEnabled: true
                }
            }

            Controls.Switch {
                checked: settings.slidesLoop
                text: qsTr("Repeat slideshow")
                onToggled: {
                    settings.slidesLoop = !settings.slidesLoop;
                }

                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("When enabled, slideshow will be restarted after the last image.")
                hoverEnabled: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Image rotation")
                currentIndex: {
                    switch (settings.imageRotate) {
                    case Settings.ImageRotate_None:
                        return 0;
                    case Settings.ImageRotate_Rot90:
                        return 1;
                    case Settings.ImageRotate_Rot180:
                        return 2;
                    case Settings.ImageRotate_Rot270:
                        return 3;
                    }
                    return 0;
                }

                model: [qsTr("None"), "90°", "180°", "270°"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.imageRotate = Settings.ImageRotate_None;
                        break;
                    case 1:
                        settings.imageRotate = Settings.ImageRotate_Rot90;
                        break;
                    case 2:
                        settings.imageRotate = Settings.ImageRotate_Rot180;
                        break;
                    case 3:
                        settings.imageRotate = Settings.ImageRotate_Rot270;
                        break;
                    default:
                        settings.imageScale = Settings.ImageScale_None;
                        break;
                    }
                }

                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Specify the angle of rotation of the image.")
                hoverEnabled: true
            }

            RowLayout {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Image FPS")
                property string description: qsTr("The frame rate of a video stream used in a slideshow.")
                Controls.Slider {
                    from: 1
                    to: 60
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.imageFps
                    onValueChanged: {
                        settings.imageFps = value;
                    }

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: parent.description
                    hoverEnabled: true
                }
                Controls.SpinBox {
                    from: 1
                    to: 60
                    value: settings.imageFps
                    onValueChanged: {
                        settings.imageFps = value;
                    }

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: parent.description
                    hoverEnabled: true
                }
            }

             Controls.Switch {
                 visible: root.showAdvanced
                 checked: settings.imageAsVideo
                 text: qsTr("Always add images as a slideshow")
                 onToggled: {
                     settings.imageAsVideo = !settings.imageAsVideo
                 }

                 Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                 Controls.ToolTip.visible: hovered
                 Controls.ToolTip.text: qsTr("When enabled, every image is added as its own one-item slideshow.")
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
                Kirigami.FormData.label: qsTr("Multimedia")
                Kirigami.FormData.isSection: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Video format")
                currentIndex: {
                    switch (settings.casterVideoStreamFormat) {
                    case Settings.CasterStreamFormat_MpegTs:
                        return 0
                    case Settings.CasterStreamFormat_Mp4:
                        return 1
                    case Settings.CasterStreamFormat_Avi:
                        return 2
                    }
                    return 0
                }

                model: ["H.264/MPEG-TS", "H.264/MP4", "MJPEG/AVI"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.casterVideoStreamFormat = Settings.CasterStreamFormat_MpegTs
                        break
                    case 1:
                        settings.casterVideoStreamFormat = Settings.CasterStreamFormat_Mp4
                        break
                    case 2:
                        settings.casterVideoStreamFormat = Settings.CasterStreamFormat_Avi
                        break
                    default:
                        settings.casterVideoStreamFormat = Settings.CasterStreamFormat_MpegTs
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Format used for real-time streaming.") + " " +
                                       qsTr("Change if you observe problems with video playback in Camera or Screen capture.")
                hoverEnabled: true
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Slideshow video format")
                currentIndex: {
                    switch (settings.casterVideoStreamFormatSlides) {
                    case Settings.CasterStreamFormat_MpegTs:
                        return 0
                    case Settings.CasterStreamFormat_Mp4:
                        return 1
                    case Settings.CasterStreamFormat_Avi:
                        return 2
                    }
                    return 2
                }

                model: ["H.264/MPEG-TS", "H.264/MP4", "MJPEG/AVI"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.casterVideoStreamFormatSlides = Settings.CasterStreamFormat_MpegTs
                        break
                    case 1:
                        settings.casterVideoStreamFormatSlides = Settings.CasterStreamFormat_Mp4
                        break
                    case 2:
                        settings.casterVideoStreamFormatSlides = Settings.CasterStreamFormat_Avi
                        break
                    default:
                        settings.casterVideoStreamFormatSlides = Settings.CasterStreamFormat_Avi
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Format used for real-time streaming.") + " " +
                                       qsTr("Change if you observe problems with video playback in Slideshow.")
                hoverEnabled: true
            }


            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Audio format")
                currentIndex: {
                    switch (settings.casterAudioStreamFormat) {
                    case Settings.CasterStreamFormat_Mp3:
                        return 0
                    case Settings.CasterStreamFormat_Mp4:
                        return 1
                    case Settings.CasterStreamFormat_MpegTs:
                        return 2
                    case Settings.CasterStreamFormat_Avi:
                        return 3
                    }
                    return 0
                }

                model: ["MP3", "AAC/MP4", "AAC/MPEG-TS", "AAC/AVI"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3
                        break
                    case 1:
                        settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp4
                        break
                    case 2:
                        settings.casterAudioStreamFormat = Settings.CasterStreamFormat_MpegTs
                        break
                    case 3:
                        settings.casterVideoStreamFormatSlides = Settings.CasterStreamFormat_Avi
                        break
                    default:
                        settings.casterAudioStreamFormat = Settings.CasterStreamFormat_Mp3
                    }
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Format used for real-time streaming.") + " " +
                                       qsTr("Change if you observe problems with audio playback in Microphone or Audio capture.")
                hoverEnabled: true
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Maximum image size")
                currentIndex: {
                    switch (settings.imageScale) {
                    case Settings.ImageScale_None:
                        return 0;
                    case Settings.ImageScale_2160:
                        return 1;
                    case Settings.ImageScale_1080:
                        return 2;
                    case Settings.ImageScale_720:
                        return 3;
                    }
                    return 2;
                }

                model: [qsTr("Unlimited"), "UHD", "FHD", "HD"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.imageScale = Settings.ImageScale_None;
                        break;
                    case 1:
                        settings.imageScale = Settings.ImageScale_2160;
                        break;
                    case 2:
                        settings.imageScale = Settings.ImageScale_1080;
                        break;
                    case 3:
                        settings.imageScale = Settings.ImageScale_720;
                        break;
                    default:
                        settings.imageScale = Settings.ImageScale_1080;
                    }
                }
            }

            RowLayout {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("MJPEG quality")
                Controls.Slider {
                    from: 0
                    to: 100
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.mjpegQuality
                    onValueChanged: {
                        settings.mjpegQuality = value;
                    }
                }
                Controls.SpinBox {
                    from: 0
                    to: 100
                    value: settings.mjpegQuality
                    onValueChanged: {
                        settings.mjpegQuality = value;
                    }
                }
            }

            RowLayout {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("H.264 quality")
                Controls.Slider {
                    from: 0
                    to: 100
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.x264Quality
                    onValueChanged: {
                        settings.x264Quality = value;
                    }
                }
                Controls.SpinBox {
                    from: 0
                    to: 100
                    value: settings.x264Quality
                    onValueChanged: {
                        settings.x264Quality = value;
                    }
                }
            }

            Controls.Switch {
                visible: root.showAdvanced
                checked: !settings.allowNotIsomMp4
                text: qsTr("Block fragmented MP4 audio streams")
                onToggled: {
                    settings.allowNotIsomMp4 = !settings.allowNotIsomMp4;
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("Some UPnP devices don't support audio stream in fragmented MP4 format. " + "This kind of stream might even hang a device. " + "To overcome this problem, Jupii tries to re-transcode stream to standard MP4. " + "When re-transcoding fails and this option is enabled, item will not be played at all.")
                hoverEnabled: true
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Video encoder")
                currentIndex: {
                    switch (settings.casterVideoEncoder) {
                    case Settings.CasterVideoEncoder_Auto:
                        return 0
                    case Settings.CasterVideoEncoder_X264:
                        return 1
                    case Settings.CasterVideoEncoder_Nvenc:
                        return 2
                    case Settings.CasterVideoEncoder_V4l2:
                        return 3
                    }
                    return 0
                }

                model: [qsTr("Auto"), "x264", "nvenc", "V4L2"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto;
                        break
                    case 1:
                        settings.casterVideoEncoder = Settings.CasterVideoEncoder_X264;
                        break
                    case 2:
                        settings.casterVideoEncoder = Settings.CasterVideoEncoder_Nvenc;
                        break
                    case 3:
                        settings.casterVideoEncoder = Settings.CasterVideoEncoder_V4l2;
                        break
                    default:
                        settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto;
                    }
                }
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Video scaling algorithm")
                currentIndex: {
                    switch (settings.videoScaleAlgo) {
                    case Settings.VideoScaleAlgo_FastBilinear:
                        return 0
                    case Settings.VideoScaleAlgo_Bilinear:
                        return 1
                    case Settings.VideoScaleAlgo_Bicubic:
                        return 2
                    case Settings.VideoScaleAlgo_Neighbor:
                        return 3
                    case Settings.VideoScaleAlgo_Lanczos:
                        return 4
                    }
                    return 0
                }

                model: ["Fast Bilinear", "Bilinear", "Bicubic", "Neighbor", "Lanczos"]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_FastBilinear;
                        break
                    case 1:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_Bilinear;
                        break
                    case 2:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_Bicubic;
                        break
                    case 3:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_Neighbor;
                        break
                    case 4:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_Lanczos;
                        break
                    default:
                        settings.videoScaleAlgo = Settings.VideoScaleAlgo_FastBilinear;
                    }
                }
            }

            CasterSourceVolume {
                Kirigami.FormData.label: qsTr("Microphone volume boost")
                volume: settings.casterMicVolume
                onVolumeChanged: settings.casterMicVolume = volume
            }

            CasterSourceVolume {
                Kirigami.FormData.label: qsTr("Audio capture volume boost")
                volume: settings.casterPlaybackVolume
                onVolumeChanged: settings.casterPlaybackVolume = volume
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
                        return 0;
                    if (settings.cacheType === Settings.Cache_Always)
                        return 1;
                    if (settings.cacheType === Settings.Cache_Never)
                        return 2;
                    return 0;
                }

                model: [qsTr("Auto"), qsTr("Always"), qsTr("Never")]

                onCurrentIndexChanged: {
                    if (currentIndex == 0)
                        settings.cacheType = Settings.Cache_Auto;
                    else if (currentIndex == 1)
                        settings.cacheType = Settings.Cache_Always;
                    else if (currentIndex == 2)
                        settings.cacheType = Settings.Cache_Never;
                    else
                        settings.cacheType = Settings.Cache_Auto;
                }
            }

            Controls.ComboBox {
                Kirigami.FormData.label: qsTr("Cache cleaning")
                currentIndex: {
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Auto)
                        return 0;
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Always)
                        return 1;
                    if (settings.cacheCleaningType === Settings.CacheCleaning_Never)
                        return 2;
                    return 0;
                }

                model: [qsTr("Auto"), qsTr("Always"), qsTr("Never")]

                onCurrentIndexChanged: {
                    if (currentIndex == 0)
                        settings.cacheCleaningType = Settings.CacheCleaning_Auto;
                    else if (currentIndex == 1)
                        settings.cacheCleaningType = Settings.CacheCleaning_Always;
                    else if (currentIndex == 2)
                        settings.cacheCleaningType = Settings.CacheCleaning_Never;
                    else
                        settings.cacheCleaningType = Settings.CacheCleaning_Auto;
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
                Kirigami.FormData.label: qsTr("Other options")
                Kirigami.FormData.isSection: true
            }

            Controls.TextField {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Frontier Silicon PIN")
                inputMethodHints: Qt.ImhDigitsOnly
                text: settings.fsapiPin
                placeholderText: qsTr("Enter Frontier Silicon PIN")
                onEditingFinished: {
                    settings.fsapiPin = text;
                }
            }

            Controls.ComboBox {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Preferred network interface")
                currentIndex: utils.prefNetworkInfIndex()
                model: utils.networkInfs()
                onCurrentIndexChanged: {
                    utils.setPrefNetworkInfIndex(currentIndex);
                }

                Connections {
                    target: settings
                    onPrefNetInfChanged: {
                        parent.model = utils.networkInfs();
                        parent.currentIndex = utils.prefNetworkInfIndex();
                    }
                }
            }

            Controls.Switch {
                visible: root.showAdvanced
                checked: settings.showAllDevices
                text: qsTr("All devices visible")
                onToggled: {
                    settings.showAllDevices = !settings.showAllDevices;
                }

                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.text: qsTr("All types of UPnP devices are detected " + "and shown, including unsupported devices like " + "home routers. For unsupported devices only " + "basic description information is available. " + "This option might be useful for auditing UPnP devices " + "in your local network.")
                hoverEnabled: true
            }

            RowLayout {
                visible: root.showAdvanced
                Kirigami.FormData.label: qsTr("Location of Python libraries")

                Controls.TextField {
                    id: pyTextField
                    inputMethodHints: Qt.ImhNoPredictiveText
                    text: settings.pyPath

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: qsTr("Python libraries directory (%1).").arg("<i>PYTHONPATH</i>") + " " + qsTr("Leave blank to use the default value.") + " " + qsTr("This option may be useful if you use %1 module to manage Python libraries.").arg("<i>venv</i>")
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

            Controls.Switch {
                checked: settings.logToFile
                text: qsTr("Enable logging")
                onToggled: {
                    settings.logToFile = !settings.logToFile;
                }

                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Needed for troubleshooting purposes. " + "The log data is stored in %1 file.").arg(settings.getCacheDir() + "/jupii.log")
            }

            Controls.Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset settings")
                onClicked: {
                    resetDialog.open();
                }
            }
        }
    }

    FileDialog {
        id: recDirDialog

        title: qsTr("Choose a directory for recordings")
        selectMultiple: false
        selectFolder: true
        selectExisting: true
        folder: utils.pathToUrl(_settings.recDir)
        onAccepted: {
            recDirDialog.fileUrls.forEach(function (url) {
                _settings.recDir = utils.urlToPath(url);
            });
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
}
