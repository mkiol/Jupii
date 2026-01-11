/* Copyright (C) 2020-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.14 as Kirigami
import QtGraphicalEffects 1.0

import org.mkiol.jupii.ContentServer 1.0
import org.mkiol.jupii.AVTransport 1.0
import org.mkiol.jupii.Settings 1.0

Kirigami.ScrollablePage {
    id: root

    property int itemType: utils.itemTypeFromUrl(av.currentId)
    property bool isShout: app.streamTitle.length !== 0 && itemType === ContentServer.ItemType_Url
    property var urlInfo: cserver.parseUrl(av.currentId)

    title: av.currentTitle.length > 0 ? av.currentTitle : qsTr("No media")

    actions {
        main: Kirigami.Action {
            text: qsTr("Open URL")
            iconName: "globe"
            enabled: av.currentYtdl
            visible: enabled
            onTriggered: Qt.openUrlExternally(av.currentOrigURL)
        }
        contextualActions: [
            Kirigami.Action {
                text: qsTr("Copy current title")
                iconName: "edit-copy"
                enabled: itemType === ContentServer.ItemType_Url && isShout
                visible: enabled
                onTriggered: {
                    utils.setClipboard(app.streamTitle)
                    showPassiveNotification(qsTr("Current title was copied to clipboard"))
                }
            },
            Kirigami.Action {
                text: itemType === ContentServer.ItemType_LocalFile ? qsTr("Copy path") :
                                                                      qsTr("Copy URL")
                iconName: "edit-copy"
                enabled: itemType === ContentServer.ItemType_Url ||
                         itemType === ContentServer.ItemType_LocalFile
                visible: true
                onTriggered: {
                    utils.setClipboard(itemType === ContentServer.ItemType_LocalFile ?
                                           av.currentPath : av.currentOrigURL)
                    showPassiveNotification(itemType === ContentServer.ItemType_LocalFile ?
                                                qsTr("Path was copied to clipboard") :
                                                qsTr("URL was copied to clipboard"))
                }
            }
        ]
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Image {
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth: Kirigami.Units.iconSizes.enormous
            Layout.preferredHeight: Kirigami.Units.iconSizes.enormous
//            source: {
//                switch(itemType) {
//                case ContentServer.ItemType_LocalFile:
//                case ContentServer.ItemType_Url:
//                case ContentServer.ItemType_Upnp:
//                    return av.currentAlbumArtURI
//                case ContentServer.ItemType_ScreenCapture:
//                case ContentServer.ItemType_PlaybackCapture:
//                case ContentServer.ItemType_Mic:
//                case ContentServer.ItemType_Cam:
//                case ContentServer.ItemType_Slides:
//                    break;
//                }
//                return ""
//            }
            source: av.currentAlbumArtURI
            fillMode: Image.PreserveAspectCrop
            visible: status === Image.Ready
        }

        GridLayout {
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            rowSpacing: Kirigami.Units.largeSpacing
            columnSpacing: Kirigami.Units.largeSpacing
            columns: 2

            Controls.Label {
                visible: itemTypeLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Item type")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: itemTypeLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: text.length > 0
                text: {
                    switch(itemType) {
                    case ContentServer.ItemType_LocalFile:
                        return qsTr("Local file")
                    case ContentServer.ItemType_Url:
                        return qsTr("URL")
                    case ContentServer.ItemType_Upnp:
                        return qsTr("Media Server")
                    case ContentServer.ItemType_ScreenCapture:
                        return qsTr("Screen Capture")
                    case ContentServer.ItemType_PlaybackCapture:
                        return qsTr("Audio Capture")
                    case ContentServer.ItemType_Mic:
                        return qsTr("Microphone")
                    case ContentServer.ItemType_Cam:
                        return qsTr("Camera")
                    case ContentServer.ItemType_Slides:
                        return qsTr("Slideshow")
                    default:
                        return ""
                    }
                }
            }

            Controls.Label {
                visible: videoLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Video source")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: videoLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: urlInfo.videoName.length > 0 &&
                         (itemType === ContentServer.ItemType_Cam ||
                          itemType === ContentServer.ItemType_ScreenCapture)
                wrapMode: Text.WordWrap
                text: urlInfo.videoName
            }

            Controls.Label {
                visible: audioLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Audio source")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: audioLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: urlInfo.audioName.length > 0 &&
                         (itemType === ContentServer.ItemType_Cam ||
                          itemType === ContentServer.ItemType_ScreenCapture ||
                          itemType === ContentServer.ItemType_Mic ||
                          itemType === ContentServer.ItemType_PlaybackCapture)
                wrapMode: Text.WordWrap
                text: urlInfo.audioName
            }

            Controls.Label {
                visible: videoOrientationLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Video orientation")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: videoOrientationLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType === ContentServer.ItemType_Cam ||
                         itemType === ContentServer.ItemType_ScreenCapture
                wrapMode: Text.WordWrap
                text: {
                    switch (urlInfo.videoOrientation) {
                    case Settings.CasterVideoOrientation_Auto: return qsTr("Auto")
                    case Settings.CasterVideoOrientation_Portrait: return qsTr("Portrait")
                    case Settings.CasterVideoOrientation_InvertedPortrait: return qsTr("Inverted portrait")
                    case Settings.CasterVideoOrientation_Landscape: return qsTr("Landscape")
                    case Settings.CasterVideoOrientation_InvertedLandscape: return qsTr("Inverted landscape")
                    }
                }
            }

            Controls.Label {
                visible: titleLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: isShout ? qsTr("Station name") : qsTr("Title")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: titleLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: text.length !== 0 &&
                         itemType !== ContentServer.ItemType_PlaybackCapture &&
                         itemType !== ContentServer.ItemType_ScreenCapture &&
                         itemType !== ContentServer.ItemType_Mic &&
                         itemType !== ContentServer.ItemType_Cam
                wrapMode: Text.WordWrap
                text: av.currentTitle
            }

            Controls.Label {
                visible: curTitleLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: {
                    if (itemType === ContentServer.ItemType_PlaybackCapture ||
                            itemType === ContentServer.ItemType_ScreenCapture) {
                        return qsTr("Captured application")
                    }
                    if (itemType === ContentServer.ItemType_Slides) {
                        return qsTr("Slideshow progress")
                    }
                    return qsTr("Current title")
                }
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: curTitleLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: (itemType === ContentServer.ItemType_PlaybackCapture ||
                          itemType === ContentServer.ItemType_ScreenCapture ||
                          itemType === ContentServer.ItemType_Url ||
                          itemType === ContentServer.ItemType_Slides) &&
                         text.length !== 0 && av.currentType !== AVTransport.T_Image
                wrapMode: Text.WordWrap
                text: app.streamTitle.length === 0 &&
                      (itemType === ContentServer.ItemType_PlaybackCapture ||
                       itemType === ContentServer.ItemType_ScreenCapture) ?
                          qsTr("None") : app.streamTitle
            }

            Controls.Label {
                visible: sizeLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Number of images")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: sizeLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType === ContentServer.ItemType_Slides && !curTitleLabel.visible
                wrapMode: Text.WordWrap
                text: av.currentSize
            }

            Controls.Label {
                visible: authorLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Author")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: authorLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType !== ContentServer.ItemType_Mic &&
                         itemType !== ContentServer.ItemType_Cam &&
                         itemType !== ContentServer.ItemType_PlaybackCapture &&
                         itemType !== ContentServer.ItemType_ScreenCapture &&
                         av.currentType !== AVTransport.T_Image &&
                         text.length !== 0
                wrapMode: Text.WordWrap
                text: av.currentAuthor
            }

            Controls.Label {
                visible: albumLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Album")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: albumLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType !== ContentServer.ItemType_Mic &&
                         itemType !== ContentServer.ItemType_Cam &&
                         itemType !== ContentServer.ItemType_PlaybackCapture &&
                         itemType !== ContentServer.ItemType_ScreenCapture &&
                         av.currentType !== AVTransport.T_Image &&
                         text.length !== 0
                wrapMode: Text.WordWrap
                text: av.currentAlbum
            }
            Controls.Label {
                visible: durationLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Duration")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: durationLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: av.currentType !== AVTransport.T_Image &&
                         av.currentTrackDuration !== 0 &&
                         itemType !== ContentServer.ItemType_Mic &&
                         itemType !== ContentServer.ItemType_Cam &&
                         itemType !== ContentServer.ItemType_PlaybackCapture &&
                         itemType !== ContentServer.ItemType_ScreenCapture
                wrapMode: Text.WordWrap
                text: utils.secToStr(av.currentTrackDuration)
            }
            Controls.Label {
                visible: recordingLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: itemType === ContentServer.ItemType_Slides ? qsTr("Last edit time") : qsTr("Recording date")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: recordingLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: text.length !== 0
                wrapMode: Text.WordWrap
                text: av.currentRecDate
            }

            Controls.Label {
                visible: serverLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Media Server")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: serverLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType === ContentServer.ItemType_Upnp &&
                         text.length !== 0
                wrapMode: Text.WordWrap
                text: utils.devNameFromUpnpId(av.currentId)
            }

            Controls.Label {
                visible: contentLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Content type")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: contentLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: text.length !== 0
                wrapMode: Text.WordWrap
                text: av.currentContentType
            }

            Controls.Label {
                visible: liveLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Live")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: liveLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: playlist.active && (itemType === ContentServer.ItemType_Mic ||
                         itemType === ContentServer.ItemType_Cam ||
                         itemType === ContentServer.ItemType_PlaybackCapture ||
                         itemType === ContentServer.ItemType_ScreenCapture ||
                         itemType === ContentServer.ItemType_Url)
                wrapMode: Text.WordWrap
                text: playlist.live ? qsTr("Yes") : qsTr("No")
            }

            Controls.Label {
                visible: cachedLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: qsTr("Cached")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: cachedLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: settings.isDebug() && itemType === ContentServer.ItemType_Url
                wrapMode: Text.WordWrap
                text: cserver.idCached(av.currentId) ? qsTr("Yes") : qsTr("No")
            }

            Controls.Label {
                visible: pathLabel.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
                text: av.currentPath.length !== 0 ? qsTr("Path") : qsTr("URL")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: pathLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: itemType === ContentServer.ItemType_LocalFile ||
                         itemType === ContentServer.ItemType_Url
                wrapMode: Text.WrapAnywhere
                text: av.currentPath.length !== 0 ? av.currentPath : av.currentOrigURL
            }

            Controls.Label {
                visible: mic.visible || playback.visible
                Layout.alignment: Qt.AlignRight | Qt.AlignCenter
                text: qsTr("Volume boost")
                color: Kirigami.Theme.disabledTextColor
            }

            CasterSourceVolume {
                id: mic
                visible: audioLabel.visible &&
                         (itemType === ContentServer.ItemType_Mic ||
                          itemType === ContentServer.ItemType_Cam)
                volume: settings.casterMicVolume
                onVolumeChanged: settings.casterMicVolume = volume
            }

            CasterSourceVolume {
                id: playback
                visible: audioLabel.visible &&
                         (itemType === ContentServer.ItemType_ScreenCapture ||
                          itemType === ContentServer.ItemType_PlaybackCapture)
                volume: settings.casterPlaybackVolume
                onVolumeChanged: settings.casterPlaybackVolume = volume
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: descLabel.visible
        }

        Kirigami.Heading {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            level: 1
            visible: descLabel.visible
            text: qsTr("Description")
        }

        Controls.Label {
            id: descLabel
            Layout.fillWidth: true
            visible: itemType !== ContentServer.ItemType_Mic &&
                     itemType !== ContentServer.ItemType_PlaybackCapture &&
                     itemType !== ContentServer.ItemType_ScreenCapture &&
                     itemType !== ContentServer.ItemType_Cam &&
                     text.length !== 0
            wrapMode: Text.WordWrap
            text: av.currentDescription
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: titlesRepeater.visible
        }

        Kirigami.Heading {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            level: 1
            visible: titlesRepeater.visible
            text: qsTr("Tracks history")
        }

        ColumnLayout {
            id: titlesRepeater
            visible: app.streamTitleHistory.length > 0
            spacing: 2 * Kirigami.Units.smallSpacing
            Layout.fillWidth: true
            Repeater {
                model: app.streamTitleHistory
                delegate: RowLayout {
                    Layout.fillWidth: true
                    property bool current: modelData === app.streamTitle
                    Controls.Label {
                        Layout.alignment: Qt.AlignLeft
                        text: "🎵"
                        color: current ? Kirigami.Theme.textColor : "transparent"
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                        color: current ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        text: modelData
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: itemType === ContentServer.ItemType_Slides
        }

        Kirigami.Heading {
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            level: 1
            visible: itemType === ContentServer.ItemType_Slides
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Slideshow options")
        }

        Controls.Switch {
            visible: itemType === ContentServer.ItemType_Slides
            checked: settings.slidesShowCountInd
            text: qsTr("Show slide number")
            onToggled: {
                settings.slidesShowCountInd = !settings.slidesShowCountInd;
            }
        }

        Controls.Switch {
            visible: itemType === ContentServer.ItemType_Slides
            checked: settings.slidesShowProgrInd
            text: qsTr("Show progress bar")
            onToggled: {
                settings.slidesShowProgrInd = !settings.slidesShowProgrInd;
            }
        }

        Controls.Switch {
            visible: itemType === ContentServer.ItemType_Slides
            checked: settings.slidesShowDateInd
            text: qsTr("Show date & time")
            onToggled: {
                settings.slidesShowDateInd = !settings.slidesShowDateInd;
            }
        }

        Controls.Switch {
            visible: itemType === ContentServer.ItemType_Slides
            checked: settings.slidesShowCameraInd
            text: qsTr("Show camera model")
            onToggled: {
                settings.slidesShowCameraInd = !settings.slidesShowCameraInd;
            }
        }

        GridLayout {
            visible: itemType === ContentServer.ItemType_Slides
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            rowSpacing: Kirigami.Units.largeSpacing
            columnSpacing: Kirigami.Units.largeSpacing
            columns: 2

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignCenter
                text: qsTr("Image display time (seconds)")
                color: Kirigami.Theme.disabledTextColor
            }

            RowLayout {
                property string description: qsTr("Change to adjust how long the image is displayed.")
                Controls.Slider {
                    from: 1
                    to: 120
                    stepSize: 1
                    snapMode: Controls.Slider.SnapAlways
                    value: settings.imageDuration
                    onValueChanged: {
                        settings.imageDuration = value
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
                        settings.imageDuration = value
                    }

                    Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: parent.description
                    hoverEnabled: true
                }
            }

            Controls.Label {
                Layout.alignment: Qt.AlignRight | Qt.AlignCenter
                text: qsTr("Image rotation")
                color: Kirigami.Theme.disabledTextColor
            }

            Controls.ComboBox {
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
                    return 0
                }

                model: [
                    qsTr("None"),
                    "90°",
                    "180°",
                    "270°"
                ]

                onCurrentIndexChanged: {
                    switch (currentIndex) {
                    case 0: settings.imageRotate = Settings.ImageRotate_None; break;
                    case 1: settings.imageRotate = Settings.ImageRotate_Rot90; break;
                    case 2: settings.imageRotate = Settings.ImageRotate_Rot180; break;
                    case 3: settings.imageRotate = Settings.ImageRotate_Rot270; break;
                    default: settings.imageScale = Settings.ImageScale_None; break
                    }
                }

                Controls.ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: qsTr("Specify the angle of rotation of the image.")
                hoverEnabled: true
            }
        }

        Controls.Frame {
            visible: itemType === ContentServer.ItemType_Slides &&
                     app.streamFiles.length > 0 &&
                     av.transportState === AVTransport.Playing
            Layout.fillWidth: true
            Layout.preferredHeight: slidesPanel.height + topPadding
            leftPadding: 1
            rightPadding: 1
            bottomPadding: 0

            ColumnLayout {
                id: slidesPanel
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    spacing: Kirigami.Units.largeSpacing

                    FlatButtonWithToolTip {
                        id: prevButton
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        icon.name: "media-skip-backward"
                        text: qsTr("Skip Backward")
                        onClicked: cserver.slidesSwitch(true)
                    }

                    FlatButtonWithToolTip {
                        id: pauseButton
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        icon.name: settings.imagePaused ? "media-playback-start" : "media-playback-pause"
                        text: settings.imagePaused ? qsTr("Resume slideshow") : qsTr("Pause slideshow")
                        onClicked: settings.imagePaused = !settings.imagePaused

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: settings.imagePaused && av.transportState === AVTransport.Playing
                            onRunningChanged: {
                                if (!running) {
                                    pauseButton.opacity = 1.0
                                }
                            }

                            PropertyAnimation { to: 0; duration: 500 }
                            PropertyAnimation { to: 1; duration: 500 }
                        }
                    }

                    FlatButtonWithToolTip {
                        id: nextButton
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        icon.name: "media-skip-forward"
                        text: qsTr("Skip Forward")
                        onClicked: cserver.slidesSwitch(false)
                    }

                    FlatButtonWithToolTip {
                        id: playmodeButton
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        icon.name: settings.slidesLoop ? "media-playlist-repeat" : "media-playlist-normal"
                        text: qsTr("Toggle Repeat")
                        onClicked: settings.slidesLoop = !settings.slidesLoop
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Controls.Switch {
                        id: slidesFollowSwitch
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        visible: imageView.visible
                        checked: true
                        text: qsTr("Follow current image")
                        onToggled: {
                            if (!checked) return
                             imageView.currentIndex = app.streamIdx
                        }
                    }
                }

                ListView {
                    id: imageView
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.iconSizes.enormous
                    property real itemSize: Kirigami.Units.iconSizes.enormous
                    model: app.streamFiles
                    clip: true
                    orientation: ListView.Horizontal
                    Controls.ScrollBar.horizontal: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOn
                    }
                    delegate: Image {
                        width: imageView.itemSize
                        height: imageView.itemSize
                        sourceSize { width: width; height: height }
                        source: utils.pathToUrl(modelData)
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop
                        autoTransform: true
                        z: -1
                        Rectangle {
                            visible: index === app.streamIdx
                            color: "transparent"
                            border.color: "#a0ffffff"
                            border.width: Kirigami.Units.largeSpacing
                            anchors.fill: parent
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            color: "#a0ffffff"
                            height: indexLabel.height + Kirigami.Units.smallSpacing
                            width: indexLabel.width + Kirigami.Units.smallSpacing
                            Text {
                                id: indexLabel
                                anchors.centerIn: parent
                                text: index + 1
                                color: "black"
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: cserver.slidesSwitchToIdx(index)
                        }
                    }

                    Connections {
                        target: cserver
                        enabled: slidesFollowSwitch.checked
                        onStreamFilesChanged: {
                            imageView.currentIndex = app.streamIdx
                        }
                    }
                }
            }
        }
    }
}
