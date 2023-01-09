/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0

Label {
    x: Theme.horizontalPageMargin
    width: parent.width - 2*x
    wrapMode: Text.WordWrap
    color: Theme.secondaryHighlightColor
    font.pixelSize: Theme.fontSizeSmall
    linkColor: Theme.primaryColor
    textFormat: Text.StyledText
    onLinkActivated: {
        Qt.openUrlExternally(link);
    }
}
