/* Copyright (C) 2025-2026 Michal Kosciesza <michal@mkiol.net>
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
    property string name: ""

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

    ImageGridView {
        id: imageView
        anchors.fill: parent

        header: Column {
            width: imageView.width
            PageHeader {
                title: root._creatingNew ? qsTr("Create slideshow") : qsTr("Save slideshow")
            }
            TextField {
                id: nameTextField
                width: parent.width
                placeholderText: qsTr("Enter slideshow title (optional)")
                label: qsTr("Title")
                text: root.name
                onTextChanged: root.name = text
                wrapMode: TextInput.WrapAnywhere
                EnterKey.iconSource: root.canAccept ? "image://theme/icon-m-enter-accept" :
                                                      "image://theme/icon-m-enter-next"
                EnterKey.onClicked: {
                    if (root.canAccept)
                        root.accept();
                    else
                        root.forceActiveFocus()
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
        }

        model: root._urls

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

        ViewPlaceholder {
            text: qsTr("There are no images in this slideshow")
            hintText: qsTr("Pull down to add images")
            enabled: imageView.count === 0
        }

        delegate: ThumbnailImage {
            source: modelData
            size: imageView.cellWidth
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
        }
    }

    focus: true
    Keys.onVolumeUpPressed: {
        rc.volUpPressed()
    }
    Keys.onVolumeDownPressed: {
        rc.volDownPressed()
    }
}

