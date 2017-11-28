/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0
import org.nemomobile.notifications 1.0

Notification {
    id: root

    expireTimeout: 4000
    maxContentLines: 10

    function show(bodyText, summaryText, clickedHandler) {
        if (!bodyText || bodyText === "")
            return

        if (bodyText === root.body)
            close()

        if (clickedHandler)
            root.connect.clicked = clickedHandler
        summaryText = summaryText ? summaryText : ""
        replacesId = 0
        body = bodyText
        previewBody = bodyText
        summary = summaryText
        previewSummary = summaryText
        publish()
    }
}
