/* Copyright (C) 2020-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami
import QtGraphicalEffects 1.0

import harbour.jupii.ContentServer 1.0
import harbour.jupii.AVTransport 1.0
import harbour.jupii.Settings 1.0

Kirigami.ScrollablePage {
    id: root

    property int itemType: utils.itemTypeFromUrl(av.currentId)
    property bool isShout: app.streamTitle.length !== 0
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
                        return isShout ? qsTr("Icecast URL") : qsTr("URL")
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
                text: itemType === ContentServer.ItemType_PlaybackCapture ||
                      itemType === ContentServer.ItemType_ScreenCapture ?
                          qsTr("Application") : qsTr("Current title")
                color: Kirigami.Theme.disabledTextColor
            }
            Controls.Label {
                id: curTitleLabel
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                visible: (itemType === ContentServer.ItemType_PlaybackCapture ||
                          itemType === ContentServer.ItemType_ScreenCapture ||
                          itemType === ContentServer.ItemType_Url) &&
                         text.length !== 0 && av.currentType !== AVTransport.T_Image
                wrapMode: Text.WordWrap
                text: app.streamTitle.length === 0 &&
                      (itemType === ContentServer.ItemType_PlaybackCapture ||
                       itemType === ContentServer.ItemType_ScreenCapture) ?
                          qsTr("None") : app.streamTitle
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
                text: qsTr("Recording date")
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
                visible: itemType === ContentServer.ItemType_Url
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
                text: qsTr("Volume")
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
            visible: descLabel.visible || titlesRepeater.visible
        }

        Kirigami.Heading {
            Layout.fillWidth: true
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
                     text.length !== 0
            wrapMode: Text.WordWrap
            text: av.currentDescription
        }

        Kirigami.Heading {
            Layout.fillWidth: true
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
    }
}
