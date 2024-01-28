/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami
import QtQuick.Dialogs 1.1

import harbour.jupii.RecModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "rec"

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: qsTr("Recordings")

    supportsRefreshing: false
    Component.onCompleted: {
        itemModel.updateModel()
    }

    actions {
        main: Kirigami.Action {
            text: itemModel.selectedCount > 0 ? qsTr("Add %n selected", "", itemModel.selectedCount) : qsTr("Add selected")
            enabled: itemModel.selectableCount > 0 && !itemModel.busy
            iconName: "list-add"
            onTriggered: {
                playlist.addItemUrls(itemModel.selectedItems())
                app.removePagesAfterAddMedia()
            }
        }
        contextualActions: [
            Kirigami.Action {
                text: itemModel.count === itemModel.selectedCount ?
                          qsTr("Unselect all") : qsTr("Select all")
                iconName: itemModel.count === itemModel.selectedCount ?
                              "dialog-cancel" : "checkbox"
                enabled: itemList.count !== 0 && !itemModel.busy
                displayHint: Kirigami.Action.DisplayHint.AlwaysHide
                onTriggered: {
                    if (itemModel.count === itemModel.selectedCount)
                        itemModel.setAllSelected(false)
                    else
                        itemModel.setAllSelected(true)
                }
            },
            Kirigami.Action {
                displayHint: Kirigami.Action.DisplayHint.AlwaysHide
                text: qsTr("Delete selected")
                iconName: "delete"
                enabled: itemModel.selectedCount > 0
                onTriggered: {
                    deleteDialog.open()
                }
            }
        ]
    }

    MessageDialog {
        id: deleteDialog
        title: qsTr("Delete selected")
        icon: StandardIcon.Question
        text: qsTr("Delete %n recording(s)?", "", itemModel.selectedCount)
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onAccepted: {
            itemModel.deleteSelected()
        }
    }

    header: Controls.ToolBar {
        RowLayout {
            anchors.fill: parent
            Kirigami.SearchField {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                onTextChanged: {
                    itemModel.filter = text
                }
            }
            Controls.Label {
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: Kirigami.Units.smallSpacing
                text: qsTr("Sort by:")
            }
            Controls.ComboBox {
                Layout.alignment: Qt.AlignLeft
                Layout.rightMargin: Kirigami.Units.smallSpacing
                currentIndex: itemModel.queryType
                model: [
                    qsTr("Recording time"),
                    qsTr("Title"),
                    qsTr("Author")
                ]
                onCurrentIndexChanged: {
                    itemModel.queryType = currentIndex
                }
            }
        }
    }

    RecModel {
        id: itemModel
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem
            highlighted: model.selected
            enabled: !itemModel.busy
            label: model.title
            subtitle: {
                var date = model.friendlyDate
                var author = model.author
                if (author.length > 0 && date.length > 0)
                    return author + " · " + date
                if (date.length > 0)
                    return date
                if (author.length > 0)
                    return author
            }
            defaultIconSource: "emblem-music-symbolic"
            iconSource: model.icon
            iconSize: Kirigami.Units.iconSizes.medium

            onClicked: {
                var selected = model.selected
                itemModel.setSelected(model.index, !selected);
            }

            actions: [
                Kirigami.Action {
                    iconName: "checkbox"
                    text: qsTr("Toggle selection")
                    onTriggered: {
                        var selected = model.selected
                        itemModel.setSelected(model.index, !selected);
                    }
                }
            ]
        }
    }

    ListView {
        id: itemList
        model: itemModel

        delegate: Kirigami.DelegateRecycler {
            width: parent.width
            sourceComponent: listItemComponent
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: itemList.count === 0 && !itemModel.busy
            text: qsTr("No recordings")
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: itemModel.busy
    }
}
