/* Copyright (C) 2020-2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

import QtQuick 2.4
import QtQuick.Controls 2.2 as Controls
import QtQuick.Layouts 1.2
import org.kde.kirigami 2.14 as Kirigami

Kirigami.ScrollablePage {
    id: root
    objectName: "fosdem"

    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0
    topPadding: 0

    title: qsTr("FOSDEM Conferences")

    ListView {
        model: ListModel {
            ListElement {
                title: "FOSDEM 2023"
                year: 2023
            }
            ListElement {
                title: "FOSDEM 2022"
                year: 2022
            }
            ListElement {
                title: "FOSDEM 2021"
                year: 2021
            }
            ListElement {
                title: "FOSDEM 2020"
                year: 2020
            }
            ListElement {
                title: "FOSDEM 2019"
                year: 2019
            }
            ListElement {
                title: "FOSDEM 2018"
                year: 2018
            }
            ListElement {
                title: "FOSDEM 2017"
                year: 2017
            }
            ListElement {
                title: "FOSDEM 2016"
                year: 2016
            }
            ListElement {
                title: "FOSDEM 2015"
                year: 2015
            }
            ListElement {
                title: "FOSDEM 2014"
                year: 2014
            }
            ListElement {
                title: "FOSDEM 2013"
                year: 2013
            }
        }

        delegate: DoubleListItem {
            label: model.title
            iconSource: "qrc:/images/fosdem.svg"
            iconSize: Kirigami.Units.iconSizes.medium
            highlighted: pageStack.currentItem !== root ? app.rightPage(root).year === model.year : false
            next: true
            onClicked: {
                pageStack.push(Qt.resolvedUrl("FosdemPage.qml"), {"year" : model.year})
            }
        }
    }
}
