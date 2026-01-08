/* Copyright (C) 2025-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.15 as Controls
import QtQuick.Layouts 1.3
import org.kde.kirigami 2.14 as Kirigami
import QtQuick.Dialogs 1.1

import org.mkiol.jupii.Settings 1.0

PopupDialog {
    id: root

    required property var model
    property string slidesId: ""
    property alias name: nameTextField.text

    property var _urls: []
    readonly property bool _creatingNew: slidesId.length === 0

    standardButtons: _urls.length === 0 ? Controls.Dialog.Cancel :
                        Controls.Dialog.Ok | Controls.Dialog.Cancel
    title: _creatingNew ? qsTr("Create slideshow") : qsTr("Edit slideshow")
    columnWidth: app.width * 0.9

    onAccepted: {
        model.createItem(slidesId, name, _urls)
    }

    onOpenedChanged: {
        if (opened && !_creatingNew) {
            var item = model.parseItem(slidesId)
            _urls = item["urls"]
            name = item["name"]
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a file")
        selectMultiple: true
        selectFolder: false
        selectExisting: true
        folder: shortcuts.pictures
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: {
            for (var i = 0, len = fileDialog.fileUrls.length; i < len; i++) {
                root._urls.push(fileDialog.fileUrls[i]);
            }
            root._urls = root._urls // needed to refresh list view
        }
    }

    Kirigami.CardsListView {
        id: cardView

        property real maxHeight: 0
        Layout.fillWidth: true
        Layout.preferredHeight: maxHeight
        model: root._urls
        clip: true
        orientation: ListView.Horizontal
        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            policy: Controls.ScrollBar.AlwaysOn
        }
        spacing: Kirigami.Units.mediumSpacing

        header: ColumnLayout {
            Controls.Button {
                text: root._urls.length > 0 ? qsTr("Add more images") : qsTr("Add images")
                action: Kirigami.Action {
                    iconName: "list-add"
                    onTriggered: fileDialog.open()
                }
            }
            Controls.Button {
                visible: root._urls.length > 0
                text: qsTr("Remove all images")
                action: Kirigami.Action {
                    iconName: "remove-symbolic"
                    onTriggered: {
                        root._urls = []
                    }
                }
            }
            function updateHeight() {
                var h = height
                if (cardView.maxHeight < h) {
                    cardView.maxHeight = h
                }
            }
            Component.onCompleted: updateHeight()
            onHeightChanged: updateHeight()
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: cardView.count === 0
            text: qsTr("There are no images in this slideshow")
        }

        delegate: Kirigami.Card {
            id: card
            width: 100
            banner.source: modelData
            actions: [
                Kirigami.Action {
                    text: qsTr("Remove")
                    iconName: "remove-symbolic"
                    displayHint: Kirigami.DisplayHint.KeepVisible
                    onTriggered: {
                        root._urls.splice(index, 1)
                        root._urls = root._urls // needed to refresh list view
                    }
                },
                Kirigami.Action {
                    text: qsTr("Move back")
                    iconName: "go-previous-symbolic"
                    displayHint: Kirigami.DisplayHint.IconOnly
                    enabled: index > 0
                    onTriggered: {
                        if (index < 1) return
                        root._urls.splice(index - 1, 0, root._urls.splice(index, 1)[0]);
                        root._urls = root._urls // needed to refresh list view
                    }
                },
                Kirigami.Action {
                    text: qsTr("Move forward")
                    iconName: "go-next-symbolic"
                    displayHint: Kirigami.DisplayHint.IconOnly
                    enabled: index < (root._urls.length - 1)
                    onTriggered: {
                        if (index >= (root._urls.length - 1)) return
                        root._urls.splice(index + 1, 0, root._urls.splice(index, 1)[0]);
                        root._urls = root._urls // needed to refresh list view
                    }
                }
            ]
            function updateHeight() {
                var h = height + 4 * Kirigami.Units.largeSpacing
                if (cardView.maxHeight < h) {
                    cardView.maxHeight = h
                }
            }
            Component.onCompleted: updateHeight()
            onHeightChanged: updateHeight()
        }
    }

    Controls.TextField {
        id: nameTextField
        Layout.fillWidth: true
        Kirigami.FormData.label: qsTr("Title")
        placeholderText: qsTr("Enter slideshow title (optional)")
    }
}
