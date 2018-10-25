/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0

Page {
    id: root

    property bool imgOk: image.status === Image.Ready
    property bool showPath: av.currentPath.length > 0

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: root.width
            //spacing: Theme.paddingLarge

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

                    sourceSize.width: 500
                    sourceSize.height: 500

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
                    //title: qsTr("Item details")
                    title: av.currentTitle
                }
            }

            Spacer {}

            Column {
                width: root.width
                spacing: Theme.paddingMedium

                /*DetailItem {
                    label: qsTr("Category")
                    value: {
                        switch(av.currentType) {
                        case AVTransport.T_Audio:
                            return qsTr("Audio")
                        case AVTransport.T_Video:
                            return qsTr("Video")
                        case AVTransport.T_Image:
                            return qsTr("Image")
                        default:
                            return qsTr("Unknown")
                        }
                    }
                }*/

                DetailItem {
                    label: app.streamTitle.length !== 0 ? qsTr("Name") : qsTr("Title")
                    value: av.currentTitle
                    visible: value.length !== 0
                }

                DetailItem {
                    label: qsTr("Title")
                    value: app.streamTitle
                    visible: av.currentType !== AVTransport.T_Image &&
                             app.streamTitle.length !== 0
                }

                DetailItem {
                    label: qsTr("Author")
                    value: av.currentAuthor
                    visible: av.currentType !== AVTransport.T_Image &&
                             value.length !== 0
                }

                DetailItem {
                    label: qsTr("Album")
                    value: av.currentAlbum
                    visible: av.currentType !== AVTransport.T_Image &&
                             value.length !== 0
                }

                DetailItem {
                    label: qsTr("Duration")
                    value: utils.secToStr(av.currentTrackDuration)
                    visible: av.currentType !== AVTransport.T_Image &&
                             av.currentTrackDuration !== 0
                }

                DetailItem {
                    label: qsTr("Content type")
                    value: av.currentContentType
                    visible: av.currentContentType.length !== 0
                }
            }

            SectionHeader {
                text: qsTr("Description")
                visible: av.currentDescription.length !== 0
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: av.currentDescription
                visible: av.currentDescription.length !== 0
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }

            SectionHeader {
                text: av.currentPath.length !== 0 ? qsTr("Path") : qsTr("URL")
                visible: av.currentURL.length !== 0
            }

            CopyableLabel {
                text: av.currentPath.length !== 0 ? av.currentPath : av.currentURL
                visible: av.currentURL.length !== 0
            }

            Spacer {}
        }

        VerticalScrollDecorator {}
    }
}
