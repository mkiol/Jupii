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

import org.mkiol.jupii.TuneinModel 1.0

Kirigami.ScrollablePage {
    id: root
    objectName: "tunein"

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: "TuneIn"

    supportsRefreshing: false
    Component.onCompleted: {
        itemModel.updateModel()
    }
    Component.onDestruction: addHistory()

    actions {
        main: Kirigami.Action {
            text: itemModel.selectedCount > 0 ? qsTr("Add %n selected", "", itemModel.selectedCount) : qsTr("Add selected")
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
            }
        ]
    }

    function addHistory() {
        var filter = itemModel.filter
        if (filter.length > 0) settings.addTuneinSearchHistory(filter)
    }

    header: Controls.ToolBar {
        RowLayout {
            spacing: 0
            anchors.fill: parent
            SearchDialogHeader {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.smallSpacing
                view: itemList
                recentSearches: settings.tuneinSearchHistory
            }
        }
    }

    TuneinModel {
        id: itemModel
        onError: {
            notifications.show(qsTr("Error in getting data"))
        }
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem
            highlighted: model.selected
            enabled: !itemModel.busy
            label: model.name
            subtitle: model.description
            defaultIconSource: "audio-x-generic"
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
            visible: itemList.count === 0 && !itemModel.busy && !itemModel.refreshing
            text: itemModel.filter.length == 0 ?
                      qsTr("Type the words to search") : qsTr("No stations")
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: itemModel.busy
    }
}
