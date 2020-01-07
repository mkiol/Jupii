/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow {
    id: app

    cover: Qt.resolvedUrl("CoverPage.qml")
    allowedOrientations: Orientation.All

    initialPage: Component {
        DevicesPage {}
    }

    Notification {
        id: notification
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
        target: av
        onCurrentIdChanged: updateStreamInfo()
    }
    Connections {
        target: cserver

        onStreamTitleChanged: updateStreamInfo()
        onStreamRecordableChanged: updateStreamInfo()
        onStreamToRecordChanged: updateStreamInfo()
        onStreamRecorded: {
            notification.show(qsTr("Track \"%1\" saved").arg(title))
        }
    }
}
