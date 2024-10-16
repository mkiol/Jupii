/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.11 as Kirigami

import org.mkiol.jupii.AVTransport 1.0
import org.mkiol.jupii.RenderingControl 1.0

Kirigami.ApplicationWindow {
    id: app

    property alias devicesAction: _devicesAction
    property alias addMediaPageAction: _addMediaPageAction
    property alias trackInfoAction: _trackInfoAction

    function showToast(text) {
        showPassiveNotification(text)
    }

    globalDrawer: Kirigami.GlobalDrawer {
        header: RowLayout {
            Layout.fillWidth: true
            Controls.ToolButton {
                icon.name: "application-menu"
                visible: globalDrawer.collapsible
                checked: !globalDrawer.collapsed
                onClicked: globalDrawer.collapsed = !globalDrawer.collapsed
            }
        }

        actions: [
            Kirigami.Action {
                text: qsTr("Play queue")
                iconName: "view-media-playlist"
                onTriggered: homeAction.trigger()
            },
            Kirigami.PagePoolAction {
                id: _devicesAction
                text: av.inited && av.deviceFriendlyName.length > 0 ?
                          qsTr("Devices (connected to %1)").arg(av.deviceFriendlyName) :
                          qsTr("Devices")
                iconName: "computer"
                shortcut: "Ctrl+D"
                pagePool: mainPagePool
                basePage: pageStack.get(0)
                page: Qt.resolvedUrl("DevicesPage.qml")
            },
            Kirigami.PagePoolAction {
                id: _addMediaPageAction
                text: qsTr("Add")
                iconName: "list-add"
                shortcut: "Alt+A"
                pagePool: mainPagePool
                basePage: pageStack.get(0)
                page: Qt.resolvedUrl("AddMediaPage.qml")
            },
            Kirigami.PagePoolAction {
                id: _trackInfoAction
                visible: enabled
                text: qsTr("Track info")
                iconName: "documentinfo"
                shortcut: "Alt+T"
                enabled: av.controlable
                pagePool: mainPagePool
                basePage: pageStack.get(0)
                page: Qt.resolvedUrl("MediaInfoPage.qml")
            },
            Kirigami.PagePoolAction {
                text: qsTr("Settings")
                iconName: "configure"
                shortcut: "Ctrl+S"
                pagePool: mainPagePool
                basePage: pageStack.get(0)
                page: Qt.resolvedUrl("SettingsPage.qml")
            },
            Kirigami.PagePoolAction {
                text: qsTr("About %1").arg(APP_NAME)
                iconSource: "qrc:/images/jupii.svg"
                pagePool: mainPagePool
                basePage: pageStack.get(0)
                page: Qt.resolvedUrl("AboutPage.qml")
            },
            Kirigami.Action {
                text: qsTr("Quit")
                iconName: "application-exit"
                shortcut: "Ctrl+Q"
                onTriggered: Qt.quit()
            }
        ]
    }

    Kirigami.PagePool {
        id: mainPagePool
    }

    Kirigami.PagePoolAction {
        id: homeAction
        pagePool: mainPagePool
        page: Qt.resolvedUrl("PlayQueuePage.qml")
    }

    Component.onCompleted: {
        console.log("qml completed");
        if (APP_VERSION !== settings.prevAppVer) {
            console.log("app update detected:", settings.prevAppVer, "=>", APP_VERSION)
            settings.prevAppVer = APP_VERSION
        }

        homeAction.trigger()
        conn.update()
    }

    // -- stream --
    property string streamTitle: ""
    property var streamTitleHistory: []
    property bool streamRecordable: false
    property bool streamToRecord: false
    function updateStreamInfo() {
        streamTitle = cserver.streamTitle(av.currentId)
        streamTitleHistory = cserver.streamTitleHistory(av.currentId)
        streamRecordable = cserver.isStreamRecordable(av.currentId)
        streamToRecord = cserver.isStreamToRecord(av.currentId)
    }

    Connections {
        target: settings
        onRestartRequiredChanged: {
            if (settings.restartRequired)
                app.showToast(qsTr("Restart is required for the changes to take effect."))
        }
    }

    Connections {
        target: pageStack
        onCurrentItemChanged: {
            var idx = pageStack.currentIndex
            var depth = pageStack.depth

            var page = mainPagePool.pageForUrl(Qt.resolvedUrl("PlayQueuePage.qml"))
            if (page !== undefined && pageStack.columnView.containsItem(page))
                page.selectionMode = false

            if (depth > 2) {
                if (idx < depth - 1) {
                    removePagesAfter(idx)
                } else {
                    // ugly workaround for partially pushed page in kirigami
                    var width = app.width
                    app.width = width / 2
                    app.width = width
                }
            }
        }
    }

    function removePagesAfterPlayQueue() {
        var page = mainPagePool.pageForUrl(Qt.resolvedUrl("PlayQueuePage.qml"))
        if (page !== undefined && pageStack.columnView.containsItem(page))
            pageStack.pop(page)
    }

    function removePagesAfterAddMedia() {
        var page = mainPagePool.pageForUrl(Qt.resolvedUrl("AddMediaPage.qml"))
        if (page !== undefined && pageStack.columnView.containsItem(page)) {
            pageStack.pop(page)
            pageStack.flickBack()
        } else { // fallback
            removePagesAfterPlayQueue()
        }
    }

    function removePagesAfter(idx) {
        var page = pageStack.get(idx)
        if (page !== undefined)
            pageStack.pop(page)
    }

    function findPageIdx(page) {
        var depth = pageStack.depth
        for (var i = 0; i < depth; i++) {
            if (page === pageStack.get(i))
                return i
        }
        return -1
    }

    function rightPage(page) {
        var idx = findPageIdx(page)
        if (idx < 0)
            return null // no page on stack
        var lastIdx = pageStack.depth - 1
        if (idx >= lastIdx) {
            return null // last page
        }
        return pageStack.get(idx + 1)
    }

    Connections {
        target: av
        onCurrentIdChanged: updateStreamInfo()
        onUpdated: rc.asyncUpdate()
        onInitedChanged: {
            if (av.inited && av.deviceFriendlyName.length > 0) {
                notifications.show(qsTr("Connected to %1").arg(av.deviceFriendlyName), "", av.deviceIconPath)
            }
        }

        onError: {
            switch(code) {
            case AVTransport.E_LostConnection:
            case AVTransport.E_NotInited:
            case RenderingControl.E_LostConnection:
                app.showToast(qsTr("Cannot connect to device"))
                break
            case AVTransport.E_ServerError:
            case RenderingControl.E_ServerError:
                app.showToast(qsTr("Device responded with an error"))
                playlist.resetToBeActive()
                break
            case AVTransport.E_InvalidPath:
                app.showToast(qsTr("Cannot play the file"))
                playlist.resetToBeActive()
                break
            }
        }
    }

    Connections {
        target: cserver
        onStreamTitleChanged: updateStreamInfo()
        onStreamRecordableChanged: updateStreamInfo()
        onStreamToRecordChanged: updateStreamInfo()
        onStreamRecorded: {
            var text = qsTr("Track \"%1\" saved").arg(title)
            app.showToast(text )
            notifications.show(text)
        }
    }

    Connections {
        target: directory

        onError: {
            switch(code) {
            case 1:
                app.showToast(qsTr("Cannot connect to a local network"))
                break
            default:
                app.showToast(qsTr("An internal error occurred"))
            }
        }

        onInitedChanged: {
            if (!directory.inited) {
                homeAction.trigger()
            }
        }
    }
}
