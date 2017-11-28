/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0

Page {
    id: root

    property var av

    property bool imgOk: image.status === Image.Ready
    property bool showPath: av.currentPath.length > 0

    SilicaFlickable {
        anchors.fill: parent

        contentHeight: column.height

        Column {
            id: column

            anchors {
                left: parent.left
                right: parent.right
            }
            spacing: Theme.paddingLarge

            Item {
                width: parent.width
                height: Math.max(image.height, header.height)

                Image {
                    id: image

                    width: parent.width
                    height: {
                        if (!root.imgOk)
                            return 0

                        var w = parent.width
                        var ratio = w / implicitWidth
                        var h = implicitHeight * ratio
                        return h > w ? w : h
                    }
                    Behavior on height { FadeAnimation {} }

                    fillMode: Image.PreserveAspectCrop

                    source: av.currentAlbumArtURI
                }

                OpacityRampEffect {
                    sourceItem: image
                    direction: OpacityRamp.BottomToTop
                    offset: root.imgOk ? 0.5 : -0.5
                    slope: 1.8
                    Behavior on offset { FadeAnimation {} }
                }

                PageHeader {
                    id: header

                    title: qsTr("Media details")
                }
            }

            Column {
                anchors {
                    left: parent.left
                    right: parent.right
                }
                spacing: Theme.paddingMedium

                AttValue {
                    att: qsTr("Title")
                    value: root.av.currentTitle
                }

                AttValue {
                    att: qsTr("Author")
                    value: root.av.currentAuthor
                }

                AttValue {
                    att: qsTr("Album")
                    value: root.av.currentAlbum
                }

                AttValue {
                    att: qsTr("Duration")
                    value: utils.secToStr(root.av.currentTrackDuration)
                }

                AttValue {
                    att: qsTr("Description")
                    value: root.av.currentDescription
                }

                /*AttValue {
                    att: qsTr("Path")
                    value: root.av.currentPath
                    visible: root.showPath
                }

                AttValue {
                    att: qsTr("URL")
                    value: root.av.currentURI
                    visible: !root.showPath
                }*/
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
