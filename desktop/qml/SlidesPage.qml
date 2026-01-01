/* Copyright (C) 2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.20 as Kirigami
import QtQuick.Dialogs 1.1

import org.mkiol.jupii.SlidesModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "slides"

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: qsTr("Slideshows")

    supportsRefreshing: false
    Component.onCompleted: {
        itemModel.updateModel()
    }

    actions {
        main: Kirigami.Action {
            text: qsTr("Add selected")
            enabled: itemModel.selectedCount > 0 && !itemModel.busy
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
        text: qsTr("Delete %n slideshow(s)?", "", itemModel.selectedCount)
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
                    qsTr("Last edit time"),
                    qsTr("Name"),
                ]
                onCurrentIndexChanged: {
                    itemModel.queryType = currentIndex
                }
            }
        }
    }

    footer: Controls.Button {
        action: Kirigami.Action {
            id: createAction
            text: qsTr("Create a new slideshow")
            enabled: !itemModel.busy
            iconName: "special-effects-symbolic"
            onTriggered: {
                itemModel.setAllSelected(false)
                app.openPopupFile(Qt.resolvedUrl("CreateSlidesDialog.qml"), {"model": itemModel})
            }
        }
    }

    SlidesModel {
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
                if (!model) return ""
                var date = model.friendlyDate
                return qsTr("%n image(s)", "", model.size) + (date.length > 0 ? " · " + date : "")
            }
            defaultIconSource: "edit-group-symbolic"
            attachedIconName: "emblem-videos-symbolic"
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
                },
                Kirigami.Action {
                    text: qsTr("Edit")
                    iconName: "edit-entry-symbolic"
                    onTriggered: {
                        itemModel.setAllSelected(false)
                        app.openPopupFile(Qt.resolvedUrl("CreateSlidesDialog.qml"), 
                            {"model": itemModel, "slidesId": model.id})
                    }
                },
                Kirigami.Action {
                    text: qsTr("Delete")
                    iconName: "edit-delete-symbolic"
                    onTriggered: {
                        itemModel.setAllSelected(false)
                        itemModel.deleteItem(model.id)
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
            text: qsTr("No slideshows")
            helpfulAction: createAction
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: itemModel.busy
    }
}
