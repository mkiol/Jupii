/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

ApplicationWindow
{
    id: app

    cover: Qt.resolvedUrl("CoverPage.qml")
    allowedOrientations: Orientation.PortraitMask

    initialPage: Component {
        DevicesPage {}
    }

    Notification {
        id: notification
    }

    // -- stream title --
    property string streamTitle: ""
    function updateStreamTitle() {
        streamTitle = cserver.streamTitle(av.currentId)
    }

    Connections {
        target: av
        onCurrentIdChanged: updateStreamTitle()
    }

    Connections {
        target: cserver
        onStreamTitleChanged: updateStreamTitle()
    }
}
