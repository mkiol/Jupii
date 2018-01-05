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

            width: root.width
            spacing: Theme.paddingLarge

            Item {
                width: parent.width
                height: Math.max(image.height, header.height)

                Image {
                    id: image

                    width: parent.width
                    height: {
                        if (!root.imgOk)
                            return 1

                        var w = parent.width
                        var ratio = w / implicitWidth
                        var h = implicitHeight * ratio
                        return h > w ? w : h
                    }

                    fillMode: Image.PreserveAspectCrop

                    source: av.currentAlbumArtURI
                }

                OpacityRampEffect {
                    sourceItem: image
                    direction: OpacityRamp.BottomToTop
                    offset: root.imgOk ? 0.5 : -0.5
                    slope: 1.8
                }

                PageHeader {
                    id: header
                    title: qsTr("Current item details")
                }
            }

            Column {
                width: root.width
                spacing: Theme.paddingMedium

                DetailItem {
                    label: qsTr("Title")
                    value: root.av.currentTitle
                    visible: value.length > 0
                }

                DetailItem {
                    label: qsTr("Author")
                    value: root.av.currentAuthor
                    visible: value.length > 0
                }

                DetailItem {
                    label: qsTr("Album")
                    value: root.av.currentAlbum
                    visible: value.length > 0
                }

                DetailItem {
                    label: qsTr("Duration")
                    value: utils.secToStr(root.av.currentTrackDuration)
                    visible: root.av.currentTrackDuration > 0
                }

                DetailItem {
                    label: qsTr("Description")
                    value: root.av.currentDescription
                    visible: value.length > 0
                }
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
