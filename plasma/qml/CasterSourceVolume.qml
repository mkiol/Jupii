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
        from: 0
        to: 10
        stepSize: 0.1
        snapMode: Controls.Slider.SnapAlways
        value: root.volume
        onValueChanged: {
            root.volume = value
        }
    }
    Controls.SpinBox {
        function round(num) {
            var exp = Math.pow(10, 1);
            return Math.round(num * exp) / exp
        }

        from: 0
        to: 100
        stepSize: 1
        value: round(root.volume) * 10
        textFromValue: function(value) {
            var v = round(root.volume).toString()
            return v.indexOf(".") === -1 ? (v + ".0") : v
        }
        valueFromText: function(text) {
            return parseFloat(text) * 10;
        }
        onValueChanged: {
            root.volume = round(value / 10);
        }
    }
}
