/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2

RowLayout {
    id: root

    property double volume

    Controls.Slider {
        from: -50
        to: 50
        stepSize: 1
        snapMode: Controls.Slider.SnapAlways
        value: root.volume
        onValueChanged: {
            root.volume = value
        }
    }
    Controls.SpinBox {
        from: -50
        to: 50
        stepSize: 1
        value: root.volume
        textFromValue: function(value) {
            return value.toString()
        }
        valueFromText: function(text) {
            return parseInt(text);
        }
        onValueChanged: {
            root.volume = value;
        }
    }
}
