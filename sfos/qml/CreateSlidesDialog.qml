/* Copyright (C) 2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Gallery 1.0
import Sailfish.Pickers 1.0
import Nemo.Thumbnailer 1.0

import harbour.jupii.Settings 1.0

Dialog {
    id: root

    property var model
    property string slidesId: ""
    property alias name: nameTextField.text

    property var _urls: []
    readonly property bool _creatingNew: slidesId.length === 0

    allowedOrientations: Orientation.All
    canAccept: _urls.length > 0
    acceptDestinationAction: PageStackAction.Pop

    Component.onCompleted: {
        if (!_creatingNew) {
            var item = model.parseItem(slidesId)
            _urls = item["urls"]
            name = item["name"]
        }
    }

    onAccepted: {
        model.createItem(slidesId, name, _urls)
    }

    Component {
        id: imagePickerDialog
        MultiImagePickerDialog {
            allowedOrientations: Orientation.All
            onDone: {
                if (result === DialogResult.Accepted) {
                    for (var i = 0, len = selectedContent.count; i < len; i++) {
                        root._urls.push(utils.pathToUrl(selectedContent.get(i).filePath));
                    }
                    root._urls = root._urls // needed to refresh list view
                }
            }
        }
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: root.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: root._creatingNew ? qsTr("Create a new slideshow") : qsTr("Save slideshow")
            }

            PullDownMenu {
                MenuItem {
                    text: root._urls.length > 0 ? qsTr("Add more images") : qsTr("Add images")
                    onClicked: pageStack.push(imagePickerDialog)
                }
                MenuItem {
                    visible: root._urls.length > 0
                    text: qsTr("Remove all")
                    onClicked: {
                        root._urls = []
                    }
                }
            }

            TextField {
                id: nameTextField
                width: parent.width
                placeholderText: qsTr("Enter slideshow name (optional)")
                label: qsTr("Slideshow name")
                wrapMode: TextInput.WrapAnywhere
                EnterKey.iconSource: root.canAccept ? "image://theme/icon-m-enter-accept" :
                                                      "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (root.canAccept)
                        root.accept();
                    else
                        urlField.forceActiveFocus()
                }

                rightItem: IconButton {
                    onClicked: nameTextField.text = ""
                    width: icon.width
                    height: icon.height
                    icon.source: "image://theme/icon-m-input-clear"
                    opacity: nameTextField.text.length > 0 ? 1.0 : 0.0
                    Behavior on opacity { FadeAnimation {} }
                }
            }

            ImageGridView {
                id: imageView

                property real itemSize: width / columnCount
                width: root.width
                cellSize: itemSize
                height: Math.max(itemSize, itemSize * Math.ceil(imageView.count / 4))
                columnCount: 4
                model: root._urls
                clip: true
                delegate: GridItem {
                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                text: qsTr("Remove")
                                onClicked: {
                                    root._urls.splice(index, 1)
                                    root._urls = root._urls // needed to refresh list view
                                }
                            }
                            MenuItem {
                                text: qsTr("Move back")
                                enabled: index > 0
                                onClicked: {
                                    if (index < 1) return
                                    root._urls.splice(index - 1, 0, root._urls.splice(index, 1)[0]);
                                    root._urls = root._urls // needed to refresh list view
                                }
                            }
                            MenuItem {
                                text: qsTr("Move forward")
                                enabled: index < (root._urls.length - 1)
                                onClicked: {
                                    if (index >= (root._urls.length - 1)) return
                                    root._urls.splice(index + 1, 0, root._urls.splice(index, 1)[0]);
                                    root._urls = root._urls // needed to refresh list view
                                }
                            }
                        }
                    }

                    Image {
                        width: imageView.itemSize
                        height: imageView.itemSize
                        sourceSize { width: width; height: height }
                        source: "image://nemoThumbnail/" + modelData
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop
                        //z: -1
                    }
                }
            }

            InfoLabel {
                visible: imageView.count === 0
                text: qsTr("There are no images in this slideshow.")
            }

            Text {
                visible: imageView.count === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font {
                    pixelSize: Theme.fontSizeLarge
                    family: Theme.fontFamilyHeading
                }
                color: Theme.highlightColor
                opacity: Theme.opacityLow
                text: qsTr("Pull down to add images.")
            }
        }
    }

//    ViewPlaceholder {
//        enabled: imageView.count === 0
//        text: qsTr("There are no images in this slideshow.")
//        hintText: qsTr("Pull down to add images.")
//    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}

