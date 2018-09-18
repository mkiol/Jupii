/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import Nemo.DBus 2.0

DBusInterface {
    id: root

    property int volume: -1

    service: "com.nokia.profiled"
    path: "/com/nokia/profiled"
    iface: "com.nokia.profiled"

    signalsEnabled: true

    Component.onCompleted: getVolume()

    function updateDbusVolume(obj) {
        if (obj.length > 1) {
            if (obj[0] === "ringing.alert.volume") {
                var volume = obj[1]
                if (volume >= 0 && volume <= 100) {
                    //console.log("D-Bus volume is " + volume)
                    root.volume = volume
                }
            }
        }
    }

    function profile_changed(changed, active, name, obj) {
        /*console.log("com.nokia.profiled profile_changed signal")
        console.log(" changed: " + changed)
        console.log(" active: " + active)
        console.log(" name: " + name)
        console.log(" obj: " + obj)*/

        if (name === "general" && obj.length === 1)
            updateDbusVolume(obj[0])
    }

    function getVolume() {
        typedCall("get_values", {"type": "s", "value": "general"},
          function(result) {
              console.log("D-Bus getVolume call completed")
              var l = result.length
              for (var i = 0; i < l; i++)
                  updateDbusVolume(result[i])
          },
          function() {
              console.log("D-Bus getVolume call failed")
          })
    }
}
