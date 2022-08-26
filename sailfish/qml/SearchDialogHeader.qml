/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

FocusScope {
    id: root

    property alias searchPlaceholderText: searchField.placeholderText
    property alias sectionHeaderText: sectionHeader.text
    property var recentSearches: []
    property int noSearchCount: 10
    property bool search: true
    property var dialog
    property var view
    signal removeSearchHistoryClicked(string value)

    height: column.height

    onHeightChanged: view.scrollToTop()

    opacity: enabled ? 1.0 : 0.0
    visible: opacity > 0.0
    Behavior on opacity { FadeAnimation {} }

    Column {
        id: column
        width: parent.width
        Behavior on width { FadeAnimation {} }

        DialogHeader {
            id: header
            dialog: root.dialog
            spacing: 0

            acceptText: {
                var count = root.view.model.selectedCount
                return count > 0 ? qsTr("%n selected", "", count) : header.defaultAcceptText
            }
        }

        SearchField {
            id: searchField
            width: parent.width
            opacity: (root.search && (root.view.model.filter.length !== 0 ||
                     root.view.model.count > root.noSearchCount)) ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity { FadeAnimation {} }
            text: root.view.model.filter

            onActiveFocusChanged: {
                if (activeFocus) root.view.currentIndex = -1
            }

            onTextChanged: {
                root.view.model.filter = text.trim()
            }

            Connections {
                target: root.view.model
                onFilterChanged: searchField.text = root.view.model.filter
            }
        }

        SectionHeader {
            visible: root.search && root.recentSearches.length > 0
            text: qsTr("Recent searches")
        }

        Repeater {
            model: recentSearches
            Behavior on width { FadeAnimation {} }
            delegate: Component {
                ListItem {
                    menu: contextMenu
                    visible: root.search
                    contentHeight: Theme.itemSizeExtraSmall
                    Label {
                        font.pixelSize: Theme.fontSizeSmall
                        truncationMode: TruncationMode.Fade
                        text: modelData
                        anchors {
                            left: parent.left; right: parent.right
                            leftMargin: Theme.horizontalPageMargin
                            rightMargin: Theme.horizontalPageMargin
                            verticalCenter: parent.verticalCenter
                        }
                        color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }
                    onClicked: root.view.model.filter = modelData
                    Component {
                        id: contextMenu
                        ContextMenu {
                            MenuItem {
                                text: qsTr("Remove")
                                onClicked: root.removeSearchHistoryClicked(modelData)
                            }
                        }
                    }
                }
            }
        }

        SectionHeader {
            id: sectionHeader
            opacity: (text.length > 0 && root.search) ? 1.0 : 0.0
            visible: opacity > 0.0
            Behavior on opacity { FadeAnimation {} }
        }
    }
}
