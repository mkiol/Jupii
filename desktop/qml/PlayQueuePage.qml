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

import harbour.jupii.AVTransport 1.0
import harbour.jupii.RenderingControl 1.0
import harbour.jupii.PlayListModel 1.0
import harbour.jupii.ContentServer 1.0

Kirigami.ScrollablePage {
    id: root

    objectName: "queue"

    property bool devless: av.deviceId.length === 0 && rc.deviceId.length === 0
    property int itemType: utils.itemTypeFromUrl(av.currentId)
    property bool inited: av.inited || rc.busy
    property bool busy: playlist.busy || playlist.refreshing || av.busy || rc.busy
    property bool canCancel: !av.busy && !rc.busy && (playlist.busy || playlist.refreshing)
    property bool networkEnabled: directory.inited
    property bool selectionMode: false

    onSelectionModeChanged: {
        if (!selectionMode) playlist.clearSelection()
    }

    Kirigami.Theme.colorSet: Kirigami.Theme.View

    implicitWidth: Kirigami.Units.gridUnit * 20

    supportsRefreshing: false

    title: av.deviceFriendlyName.length > 0 ? av.deviceFriendlyName : qsTr("Play queue")

    actions {
        main: Kirigami.Action {
            id: addAction
            text: root.selectionMode ? qsTr("Exit selection mode") : qsTr("Add")
            checked: root.selectionMode ? true : app.addMediaPageAction.checked
            iconName: root.selectionMode ? "dialog-cancel" : "list-add"
            enabled: !root.busy
            onTriggered: {
                if (root.selectionMode)
                    root.selectionMode = false
                else
                    app.addMediaPageAction.trigger()
            }
        }

        contextualActions: [
            Kirigami.Action {
                text: qsTr("Cancel")
                checked: addMediaPageAction.checked
                iconName: "dialog-cancel"
                enabled: root.canCancel && !root.selectionMode
                visible: enabled
                onTriggered: {
                    if (playlist.busy)
                        playlist.cancelAdd()
                    else if (playlist.refreshing)
                        playlist.cancelRefresh()
                }
            },
            Kirigami.Action {
                text: qsTr("Track info")
                checked: app.trackInfoAction.checked
                iconName: "documentinfo"
                enabled: av.controlable && !root.selectionMode
                visible: !root.selectionMode
                onTriggered: app.trackInfoAction.trigger()
            },
            Kirigami.Action {
                text: qsTr("Select")
                iconName: "checkbox"
                visible: !root.busy && itemList.count !== 0 && !root.selectionMode
                onTriggered: root.selectionMode = !root.selectionMode
            },
            Kirigami.Action {
                text: playlist.selectedCount == itemList.count ? qsTr("Unselect all") : qsTr("Select all")
                iconName: "checkbox"
                visible: !playlist.refreshing && !playlist.busy && itemList.count !== 0 &&
                         root.selectionMode
                onTriggered: {
                    if (playlist.selectedCount == itemList.count)
                        playlist.clearSelection()
                    else
                        playlist.setAllSelected(true)
                }
            },
            Kirigami.Action {
                text: qsTr("Save selected items")
                iconName: "document-save"
                enabled: playlist.selectedCount > 0
                visible: !playlist.refreshing && !playlist.busy && itemList.count !== 0 &&
                         root.selectionMode
                onTriggered: saveDialog.open()
            },
            Kirigami.Action {
                text: qsTr("Remove selected items")
                iconName: "delete"
                enabled: playlist.selectedCount > 0
                visible: !root.busy && itemList.count !== 0 && root.selectionMode
                onTriggered: clearDialog.open()
            },
            Kirigami.Action {
                text: qsTr("Refresh")
                iconName: "view-refresh"
                enabled: root.networkEnabled && !root.selectionMode
                visible: !root.busy && playlist.refreshable && itemList.count !== 0 && !root.selectionMode
                onTriggered: playlist.refresh()
            }
        ]
    }

    function showActiveItem() {
        if (playlist.activeItemIndex >= 0)
            itemList.positionViewAtIndex(playlist.activeItemIndex, ListView.Contain)
        else
            itemList.positionViewAtBeginning()
    }

    function showLastItem() {
        itemList.positionViewAtEnd();
    }

    Connections {
        target: playlist

        onItemsAdded: root.showLastItem()
        onItemsLoaded: root.showActiveItem()
        onActiveItemChanged: root.showActiveItem()
        onBcMainUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("BcPage.qml"))
        }
        onBcAlbumUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("BcPage.qml"), {albumPage: url})
        }
        onBcArtistUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("BcPage.qml"), {artistPage: url})
        }
        onSoundcloudMainUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"))
        }
        onSoundcloudAlbumUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {albumPage: url})
        }
        onSoundcloudArtistUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("SoundcloudPage.qml"), {artistPage: url})
        }
        onUnknownTypeUrlAdded: {
            pageStack.pop(root)
            pageStack.push(Qt.resolvedUrl("AddMediaPage.qml"), {openUrlDialog: true, url: url, name: name})
        }

        onBusyChanged: {
            refreshing = playlist.busy
        }

        onError: {
            if (code === PlayListModel.E_FileExists)
                notifications.show(qsTr("Item is already in play queue"))
            else if (code === PlayListModel.E_ItemNotAdded)
                notifications.show(qsTr("Item cannot be added"))
            else if (code === PlayListModel.E_SomeItemsNotAdded)
                notifications.show(qsTr("Some items cannot be added"))
            else if (code === PlayListModel.E_AllItemsNotAdded)
                notifications.show(qsTr("Items cannot be added"))
            else if (code === PlayListModel.E_ProxyError)
                notifications.show(qsTr("Unable to play item"))
            else
                notifications.show(qsTr("Unknown error"))
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save selected items")
        selectMultiple: false
        selectFolder: false
        selectExisting: false
        defaultSuffix: "pls"
        nameFilters: [ "Playlist (*.pls)" ]
        folder: shortcuts.music
        onAccepted: {
            if (playlist.saveSelectedToUrl(saveDialog.fileUrls[0]))
                showPassiveNotification(qsTr("Playlist has been saved"))
            root.selectionMode = false
        }
    }

    MessageDialog {
        id: clearDialog
        title: qsTr("Remove selected items")
        icon: StandardIcon.Question
        text: qsTr("Remove selected items from play queue?")
        standardButtons: StandardButton.Ok | StandardButton.Cancel
        onAccepted: {
            playlist.removeSelectedItems()
            root.selectionMode = false
        }
    }

    Component {
        id: listItemComponent
        DoubleListItem {
            id: listItem

            property bool isImage: model.type === AVTransport.T_Image
            property bool playing: model.id == av.currentId &&
                                   av.transportState === AVTransport.Playing
            enabled: !root.busy
            label: model.name
            busy: model.toBeActive
            subtitle: {
                switch (model.itemType) {
                case ContentServer.ItemType_Mic:
                case ContentServer.ItemType_PlaybackCapture:
                    return model.audioSource
                case ContentServer.ItemType_Cam:
                case ContentServer.ItemType_ScreenCapture:
                    return model.videoSource + " · " + model.videoOrientation + (model.audioSource.length !== 0 ? (" · " + model.audioSource) : "")
                default:
                    return model.artist.length !== 0 ? model.artist : ""
                }
            }
            defaultIconSource: {
                if (model.itemType === ContentServer.ItemType_Mic)
                    return "audio-input-microphone"
                else if (model.itemType === ContentServer.ItemType_PlaybackCapture)
                    return "player-volume"
                else if (model.itemType === ContentServer.ItemType_ScreenCapture)
                    return "computer"
                else if (model.itemType === ContentServer.ItemType_Cam)
                    return "camera-web"
                else
                    switch (model.type) {
                    case AVTransport.T_Image:
                        return "emblem-photos-symbolic"
                    case AVTransport.T_Audio:
                        return "emblem-music-symbolic"
                    case AVTransport.T_Video:
                        return "emblem-videos-symbolic"
                    default:
                        return "unknown"
                    }
            }
            attachedIconName: model.itemType === ContentServer.ItemType_Url ?
                                  "folder-remote" :
                              model.itemType === ContentServer.ItemType_Upnp ?
                                  "network-server" : ""
            attachedIcon2Name: {
                if (iconSource.length === 0)
                    return ""
                return defaultIconSource
            }

            iconSource: {
                if (model.itemType === ContentServer.ItemType_Mic ||
                        model.itemType === ContentServer.ItemType_PlaybackCapture ||
                        model.itemType === ContentServer.ItemType_ScreenCapture ||
                        model.itemType === ContentServer.ItemType_Cam) {
                    return ""
                }
                return model.icon
            }
            iconSize: Kirigami.Units.iconSizes.medium

            onClicked: {
                if (root.selectionMode) {
                    var selected = model.selected
                    playlist.setSelected(model.index, !selected)
                    return
                }

                if (!listItem.enabled)
                    return

                if (!root.inited)
                    return

                if (model.active)
                    playlist.togglePlay()
                else
                    playlist.play(model.id)
            }

            active: model.active

            highlighted: model.selected

            actions: [
                Kirigami.Action {
                    iconName: listItem.playing ?
                                  "media-playback-pause" : "media-playback-start"
                    text: listItem.playing ?
                              qsTr("Pause") : listItem.isImage ? qsTr("Show") : qsTr("Play")
                    visible: root.inited && !root.selectionMode
                    onTriggered: {
                        if (!model.active)
                            playlist.play(model.id)
                        else
                            playlist.togglePlay()
                    }
                },
                Kirigami.Action {
                    iconName: "delete"
                    text: qsTr("Remove")
                    visible: !root.selectionMode
                    onTriggered: {
                        playlist.remove(model.id)
                    }
                }
            ]
        }
    }

    ListView {
        id: itemList
        model: playlist

        delegate: Kirigami.DelegateRecycler {
            width: parent.width
            sourceComponent: listItemComponent
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: !root.busy && itemList.count === 0
            text: qsTr("No items")
            helpfulAction: Kirigami.Action {
                enabled: root.networkEnabled
                iconName: "list-add"
                text: qsTr("Add")
                onTriggered: addAction.trigger()
            }
        }
    }

    BusyIndicatorWithLabel {
        id: busyIndicator
        parent: root.overlay
        running: root.busy
        text: (av.busy || rc.busy) ? "" : playlist.refreshing ?
                  (playlist.progressTotal > 1 ?
                      qsTr("Preparing item %1 of %2...").arg(playlist.progressValue + 1).arg(playlist.progressTotal) :
                      qsTr("Preparing item...")) + (cserver.caching ? " " + cserver.cacheProgressString : "") :
                  playlist.progressTotal > 1 ?
                      qsTr("Adding item %1 of %2...").arg(playlist.progressValue + 1).arg(playlist.progressTotal) :
                      qsTr("Adding item...")
    }

    footer: PlayerPanel {
        id: ppanel

        width: root.width

        open: root.inited && av.controlable
        inited: root.inited

        title: av.currentTitle.length === 0 ? qsTr("Unknown") : av.currentTitle
        subtitle: app.streamTitle.length === 0 ?
                      (root.itemType !== ContentServer.ItemType_Mic &&
                       root.itemType !== ContentServer.ItemType_PlaybackCapture &&
                       root.itemType !== ContentServer.ItemType_ScreenCapture ?
                           av.currentAuthor : "") : app.streamTitle
        itemType: root.itemType

        prevEnabled: playlist.prevSupported &&
                     !playlist.refreshing
        nextEnabled: playlist.nextSupported &&
                     !playlist.refreshing

        forwardEnabled: av.seekSupported &&
                        av.transportState === AVTransport.Playing &&
                        av.currentType !== AVTransport.T_Image
        backwardEnabled: forwardEnabled
        recordEnabled: app.streamRecordable
        recordActive: app.streamToRecord

        playMode: playlist.playMode

        onNextClicked: playlist.next()
        onPrevClicked: playlist.prev()
        onTogglePlayClicked: playlist.togglePlay()

        onForwardClicked: {
            var pos = av.relativeTimePosition + settings.forwardTime
            var max = av.currentTrackDuration
            av.seek(pos > max ? max : pos)
        }

        onBackwardClicked: {
            var pos = av.relativeTimePosition - settings.forwardTime
            av.seek(pos < 0 ? 0 : pos)
        }

        onRepeatClicked: {
            playlist.togglePlayMode()
        }

        onRecordClicked: {
            cserver.setStreamToRecord(av.currentId, !app.streamToRecord)
        }

        onClicked: {
            full = !full
        }

        onIconClicked: {
            app.trackInfoAction.trigger()
        }
    }
}
