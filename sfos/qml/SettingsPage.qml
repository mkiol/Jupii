/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.Settings 1.0

Page {
    id: root

    property bool showAdvanced: false

    allowedOrientations: Orientation.All

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            InlineMessage {
                visible: settings.restartRequired
                width: parent.width
                text: qsTr("Restart is required for the changes to take effect.")
            }

            ComboBox {
                label: qsTr("Show")
                currentIndex: 0

                menu: ContextMenu {
                    MenuItem { text: qsTr("Basic options") }
                    MenuItem { text: qsTr("All options") }
                }

                onCurrentIndexChanged: {
                    root.showAdvanced = (currentIndex === 1)
                }
            }

            ExpandingSectionGroup {
                ExpandingSection {
                    title: "UPnP"

                    content.sourceComponent: Column {
                        TextSwitch {
                            automaticCheck: false
                            checked: settings.contentDirSupported
                            text: qsTr("Share play queue items via UPnP Media Server")
                            description: qsTr("When enabled, items in play queue are accessible " +
                                              "for other UPnP devices in your local network.")
                            onClicked: {
                                settings.contentDirSupported = !settings.contentDirSupported
                            }
                        }

                        ComboBox {
                            visible: root.showAdvanced
                            label: qsTr("Gapless mode (%1)").arg("setNextURI")
                            currentIndex: {
                                switch (settings.avNextUriPolicy) {
                                case Settings.AvNextUriPolicy_Auto:
                                    return 0;
                                case Settings.AvNextUriPolicy_DisableOnlyIfNotSupported:
                                    return 1;
                                case Settings.AvNextUriPolicy_NeverDisable:
                                    return 2;
                                case Settings.AvNextUriPolicy_AlwaysDisable:
                                    return 3;
                                }
                                return 0;
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: qsTr("Disable only if not supported") }
                                MenuItem { text: qsTr("Always enabled") }
                                MenuItem { text: qsTr("Always disabled") }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0:
                                    settings.avNextUriPolicy = Settings.AvNextUriPolicy_Auto;
                                    break;
                                case 1:
                                    settings.avNextUriPolicy = Settings.AvNextUriPolicy_DisableOnlyIfNotSupported;
                                    break;
                                case 2:
                                    settings.avNextUriPolicy = Settings.AvNextUriPolicy_NeverDisable;
                                    break;
                                case 3:
                                    settings.avNextUriPolicy = Settings.AvNextUriPolicy_AlwaysDisable;
                                    break;
                                default:
                                    settings.avNextUriPolicy = Settings.AvNextUriPolicy_Auto;
                                }
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Slideshow")

                    content.sourceComponent: Column {
                        // TextSwitch {
                        //     automaticCheck: false
                        //     checked: settings.imageAsVideo
                        //     text: qsTr("Always convert images into video")
                        //     description: qsTr("When enabled, image elements are always converted into a video stream with a low frame rate.")
                        //     onClicked: {
                        //         settings.imageAsVideo = !settings.imageAsVideo
                        //     }
                        // }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.slidesShowCountInd
                            text: qsTr("Show slide numbers")
                            onClicked: {
                                settings.slidesShowCountInd = !settings.slidesShowCountInd
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.slidesShowProgrInd
                            text: qsTr("Show progress bar")
                            onClicked: {
                                settings.slidesShowProgrInd = !settings.slidesShowProgrInd
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.slidesShowDateInd
                            text: qsTr("Show date & time")
                            onClicked: {
                                settings.slidesShowDateInd = !settings.slidesShowDateInd
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.slidesShowCameraInd
                            text: qsTr("Show camera model")
                            onClicked: {
                                settings.slidesShowCameraInd = !settings.slidesShowCameraInd
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.imagePaused
                            text: qsTr("Pause slideshow")
                            onClicked: {
                                settings.imagePaused = !settings.imagePaused
                            }
                        }

                        SliderWithDescription {
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 120
                            value: settings.imageDuration
                            label: qsTr("Image display time (seconds)")
                            description: qsTr("Change to adjust how long the image is displayed.")
                            onValueChanged: {
                                settings.imageDuration = value
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.slidesLoop
                            text: qsTr("Repeat slideshow")
                            description: qsTr("When enabled, slideshow will be restarted after the last image.")
                            onClicked: {
                                settings.slidesLoop = !settings.slidesLoop
                            }
                        }

                        ComboBox {
                            label: qsTr("Image rotation")
                            currentIndex: {
                                switch(settings.imageRotate) {
                                case Settings.ImageRotate_None:
                                    return 0
                                case Settings.ImageRotate_Rot90:
                                    return 1
                                case Settings.ImageRotate_Rot180:
                                    return 2
                                case Settings.ImageRotate_Rot270:
                                    return 3
                                }
                                return 0;
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("None") }
                                MenuItem { text: "90°" }
                                MenuItem { text: "180°" }
                                MenuItem { text: "270°" }
                            }

                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0: settings.imageRotate = Settings.ImageRotate_None; break;
                                case 1: settings.imageRotate = Settings.ImageRotate_Rot90; break;
                                case 2: settings.imageRotate = Settings.ImageRotate_Rot180; break;
                                case 3: settings.imageRotate = Settings.ImageRotate_Rot270; break;
                                default: settings.imageScale = Settings.ImageScale_None; break
                                }
                            }
                            description: qsTr("Specify the angle of rotation of the image.")
                        }

                        SliderWithDescription {
                            visible: root.showAdvanced
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 60
                            value: settings.imageFps
                            label: qsTr("Image FPS")
                            description: qsTr("The frame rate of a video stream used when converting an image to video format.")

                            onValueChanged: {
                                settings.imageFps = value
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Hardware keys")

                    content.sourceComponent: Column {

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.useHWVolume
                            text: qsTr("Volume control with hardware keys")
                            onClicked: {
                                settings.useHWVolume = !settings.useHWVolume
                            }
                        }

                        Slider {
                            enabled: settings.useHWVolume
                            opacity: enabled ? 1.0 : Theme.opacityLow
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 10
                            stepSize: 1
                            handleVisible: true
                            value: settings.volStep
                            valueText: value
                            label: qsTr("Volume level step")

                            onValueChanged: {
                                settings.volStep = value
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Multimedia")

                    content.sourceComponent: Column {
                        ComboBox {
                            label: qsTr("Video format")
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

                            menu: ContextMenu {
                                MenuItem { text: "H.264/MPEG-TS" }
                                MenuItem { text: "H.264/MP4" }
                                MenuItem { text: "MJPEG/AVI" }
                            }

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
                            description: qsTr("Format used for real-time streaming.") + " " +
                                         qsTr("Change if you observe problems with video playback in Camera or Screen capture.")
                        }

                        ComboBox {
                            label: qsTr("Slideshow video format")
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

                            menu: ContextMenu {
                                MenuItem { text: "H.264/MPEG-TS" }
                                MenuItem { text: "H.264/MP4" }
                                MenuItem { text: "MJPEG/AVI" }
                            }

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
                            description: qsTr("Format used for real-time streaming.") + " " +
                                         qsTr("Change if you observe problems with video playback in Slideshow.")
                        }

                        ComboBox {
                            label: qsTr("Audio format")
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

                            menu: ContextMenu {
                                MenuItem { text: "MP3" }
                                MenuItem { text: "H.264/MPEG-TS" }
                                MenuItem { text: "H.264/MP4" }
                                MenuItem { text: "MJPEG/AVI" }
                            }

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

                            description: qsTr("Format used for real-time streaming.") + " " +
                                         qsTr("Change if you observe problems with audio playback in Microphone or Audio capture.")
                        }

                        ComboBox {
                            visible: root.showAdvanced
                            label: qsTr("Maximum image size")
                            currentIndex: {
                                switch (settings.imageScale) {
                                case Settings.ImageScale_None: return 0;
                                case Settings.ImageScale_2160: return 1;
                                case Settings.ImageScale_1080: return 2;
                                case Settings.ImageScale_720: return 3;
                                }
                                return 2;
                            }
                            menu: ContextMenu {
                                MenuItem { text:  qsTr("Unlimited") }
                                MenuItem { text: "UHD" }
                                MenuItem { text: "FHD" }
                                MenuItem { text: "HD" }
                            }
                            onCurrentIndexChanged: {
                                switch (currentIndex) {
                                case 0: settings.imageScale = Settings.ImageScale_None; break;
                                case 1: settings.imageScale = Settings.ImageScale_2160; break;
                                case 2: settings.imageScale = Settings.ImageScale_1080; break;
                                case 3: settings.imageScale = Settings.ImageScale_720; break;
                                default: settings.imageScale = Settings.ImageScale_1080;
                                }
                            }
                        }

                        SliderWithDescription {
                            visible: root.showAdvanced
                            width: parent.width
                            minimumValue: 0
                            maximumValue: 100
                            stepSize: 1
                            value: settings.mjpegQuality
                            label: qsTr("MJPEG quality")
                            onValueChanged: {
                                settings.mjpegQuality = value
                            }
                        }

                        SliderWithDescription {
                            visible: root.showAdvanced
                            width: parent.width
                            minimumValue: 0
                            maximumValue: 100
                            stepSize: 1
                            value: settings.x264Quality
                            label: qsTr("H.264 quality")
                            onValueChanged: {
                                settings.x264Quality = value
                            }
                        }

                        TextSwitch {
                            visible: root.showAdvanced
                            automaticCheck: false
                            checked: !settings.allowNotIsomMp4
                            text: qsTr("Block fragmented MP4 audio streams")
                            description: qsTr("Some UPnP devices don't support audio stream in fragmented MP4 format. " +
                                              "This kind of stream might even hang a device. " +
                                              "To overcome this problem, Jupii tries to re-transcode stream to standard MP4. " +
                                              "When re-transcoding fails and this option is enabled, item will not be played at all.")
                            onClicked: {
                                settings.allowNotIsomMp4 = !settings.allowNotIsomMp4
                            }
                        }

                        // ComboBox {
                        //     visible: root.showAdvanced
                        //     label: qsTr("Video encoder")
                        //     currentIndex: {
                        //         switch (settings.casterVideoEncoder) {
                        //         case Settings.CasterVideoEncoder_Auto: return 0;
                        //         case Settings.CasterVideoEncoder_X264: return 1;
                        //         case Settings.CasterVideoEncoder_Nvenc: return 2;
                        //         case Settings.CasterVideoEncoder_V4l2: return 3;
                        //         }
                        //         return 0;
                        //     }

                        //     menu: ContextMenu {
                        //         MenuItem { text: qsTr("Auto") }
                        //         MenuItem { text: "x264" }
                        //         MenuItem { text: "nvenc" }
                        //         MenuItem { text: "V4L2" }
                        //     }

                        //     onCurrentIndexChanged: {
                        //         switch (currentIndex) {
                        //         case 0: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto; break;
                        //         case 1: settings.casterVideoEncoder = Settings.CasterVideoEncoder_X264; break;
                        //         case 2: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Nvenc; break;
                        //         case 3: settings.casterVideoEncoder = Settings.CasterVideoEncoder_V4l2; break;
                        //         default: settings.casterVideoEncoder = Settings.CasterVideoEncoder_Auto;
                        //         }
                        //     }
                        // }

                        Slider {
                            opacity: enabled ? 1.0 : Theme.opacityLow
                            width: parent.width
                            minimumValue: -50
                            maximumValue: 50
                            stepSize: 1
                            handleVisible: true
                            value: settings.casterMicVolume
                            valueText: value
                            label: qsTr("Microphone volume boost")
                            onValueChanged: {
                                settings.casterMicVolume = value
                            }
                        }

                        Slider {
                            opacity: enabled ? 1.0 : Theme.opacityLow
                            width: parent.width
                            minimumValue: -50
                            maximumValue: 50
                            stepSize: 1
                            handleVisible: true
                            value: settings.casterPlaybackVolume
                            valueText: value
                            label: qsTr("Audio capture volume boost")
                            onValueChanged: {
                                settings.casterPlaybackVolume = value
                            }
                        }

                        SliderWithDescription {
                            visible: root.showAdvanced
                            width: parent.width
                            minimumValue: 1
                            maximumValue: 60
                            value: settings.lipstickFps
                            label: qsTr("Screen capture FPS")
                            description: qsTr("The frame rate of a video stream in Screen capture.")

                            onValueChanged: {
                                settings.lipstickFps = value
                            }
                        }

                    }
                }

                ExpandingSection {
                    title: qsTr("Recorder")

                    content.sourceComponent: Column {
                        ListItem {
                            contentHeight: visible ? recflow.height + 2*Theme.paddingLarge : 0

                            Flow {
                                id: recflow
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    left: parent.left; right: parent.right
                                    leftMargin: Theme.paddingLarge
                                    rightMargin: Theme.paddingLarge
                                }
                                spacing: Theme.paddingMedium

                                Label {
                                    text: qsTr("Directory for recordings")
                                }

                                Label {
                                    color: Theme.highlightColor
                                    text: utils.dirNameFromPath(settings.recDir)
                                }
                            }

                            onClicked: openMenu();

                            menu: ContextMenu {
                                MenuItem {
                                    text: qsTr("Change")
                                    onClicked: {
                                        var obj = pageStack.push(Qt.resolvedUrl("DirPage.qml"));
                                        obj.accepted.connect(function() {
                                            settings.recDir = obj.selectedPath
                                        })
                                    }
                                }
                                MenuItem {
                                    text: qsTr("Set default")
                                    onClicked: {
                                        settings.recDir = ""
                                    }
                                }
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Caching")

                    content.sourceComponent: Column {
                        ComboBox {
                            label: qsTr("Cache remote content")
                            currentIndex: {
                                if (settings.cacheType === Settings.Cache_Auto)
                                    return 0
                                if (settings.cacheType === Settings.Cache_Always)
                                    return 1
                                if (settings.cacheType === Settings.Cache_Never)
                                    return 2
                                return 0
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: qsTr("Always") }
                                MenuItem { text: qsTr("Never") }
                            }

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

                        ComboBox {
                            label: qsTr("Cache cleaning")

                            currentIndex: {
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Auto)
                                    return 0
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Always)
                                    return 1
                                if (settings.cacheCleaningType === Settings.CacheCleaning_Never)
                                    return 2
                                return 0
                            }

                            menu: ContextMenu {
                                MenuItem { text: qsTr("Auto") }
                                MenuItem { text: qsTr("Always") }
                                MenuItem { text: qsTr("Never") }
                            }

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

                        ListItem {
                            contentHeight: flow0.height + 2 * Theme.paddingLarge
                            visible: settings.isDebug()
                            onClicked: openMenu()

                            Flow {
                                id: flow0
                                spacing: Theme.paddingMedium
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left; anchors.right: parent.right
                                anchors.leftMargin: Theme.paddingLarge
                                anchors.rightMargin: Theme.paddingLarge

                                Label {
                                    text: qsTr("Cache size")
                                }

                                Label {
                                    id: sizeLabel
                                    color: Theme.secondaryColor
                                    text: cserver.cacheSizeString()

                                    Connections {
                                        target: cserver
                                        onCacheSizeChanged: sizeLabel.text = cserver.cacheSizeString()
                                    }
                                }
                            }

                            menu: ContextMenu {
                                MenuItem {
                                    text: qsTr("Delete cache")
                                    onClicked: cserver.cleanCache()
                                }
                            }
                        }
                    }
                }

                ExpandingSection {
                    title: qsTr("Other options")
                    content.sourceComponent: Column {
                        TextField {
                            visible: root.showAdvanced
                            anchors {
                                left: parent.left; right: parent.right
                            }
                            label: qsTr("Frontier Silicon PIN")
                            text: settings.fsapiPin
                            inputMethodHints: Qt.ImhDigitsOnly
                            placeholderText: qsTr("Enter Frontier Silicon PIN")
                            onTextChanged: {
                                settings.fsapiPin = text
                            }
                        }

                        ComboBox {
                            visible: settings.isDebug() && root.showAdvanced
                            label: qsTr("Preferred network interface")
                            currentIndex: utils.prefNetworkInfIndex()
                            menu: ContextMenu {
                                Repeater {
                                    model: utils.networkInfs()
                                    MenuItem { text: modelData }
                                }
                            }
                            onCurrentIndexChanged: {
                                utils.setPrefNetworkInfIndex(currentIndex)
                                currentIndex = utils.prefNetworkInfIndex()
                            }
                        }

                        TextSwitch {
                            visible: root.showAdvanced
                            automaticCheck: false
                            checked: settings.showAllDevices
                            text: qsTr("All devices visible")
                            description: qsTr("All types of UPnP devices are detected " +
                                              "and shown, including unsupported devices like " +
                                              "home routers. For unsupported devices only " +
                                              "basic description information is available. " +
                                              "This option might be useful for auditing UPnP devices " +
                                              "in your local network.")
                            onClicked: {
                                settings.showAllDevices = !settings.showAllDevices
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.controlMpdService
                            text: qsTr("Start/stop local MPD and upmpdcli services")
                            description: qsTr("When MPD and upmpdcli are installed they will be started " +
                                              "together with Jupii and stopped on exit.")
                            onClicked: {
                                settings.controlMpdService = !settings.controlMpdService
                            }
                        }

                        TextSwitch {
                            automaticCheck: false
                            checked: settings.logToFile
                            text: qsTr("Enable logging")
                            description: qsTr("Needed for troubleshooting purposes. " +
                                              "The log data is stored in %1 file.")
                                                .arg(settings.getCacheDir() + "/jupii.log")
                            onClicked: {
                                settings.logToFile = !settings.logToFile
                            }
                        }

                        Spacer {}

                        Button {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Reset tips and hints")
                            onClicked: {
                                remorse.execute(qsTr("Resetting tips and hints"), function() { settings.resetHints() } )
                            }
                        }

                        Spacer {}

                        Button {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("Reset settings")
                            onClicked: {
                                remorse.execute(qsTr("Resetting settings"), function() { settings.reset() } )
                            }
                        }

                        Spacer {}
                        Spacer {}
                    }
                }
            }
        }
    }

    RemorsePopup {
        id: remorse
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
