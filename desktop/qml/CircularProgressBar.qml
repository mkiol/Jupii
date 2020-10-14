// source: https://github.com/rafzby/circular-progressbar/blob/master/CircularProgressBar.qml
// licence: GNU Lesser General Public License v3.0
// author: Rafal Zbytniewski

import QtQuick 2.9

Item {
    id: root

    property int size: 150
    property int lineWidth: 5
    property real value: 0

    property color color: "red"

    property int animationDuration: 1000

    width: size
    height: size

    onValueChanged: {
        canvas.degree = value * 360
    }

    onColorChanged: {
        canvas.requestPaint()
    }

    Canvas {
        id: canvas

        property real degree: 0

        anchors.fill: parent
        antialiasing: true

        onDegreeChanged: {
            requestPaint()
        }

        onPaint: {
            var ctx = getContext("2d");

            var x = root.width/2;
            var y = root.height/2;

            var radius = root.size/2 - root.lineWidth
            var startAngle = (Math.PI/180) * 270;
            var fullAngle = (Math.PI/180) * (270 + 360);
            var progressAngle = (Math.PI/180) * (270 + degree);

            ctx.reset()

            ctx.lineCap = 'round';
            ctx.lineWidth = root.lineWidth;

            ctx.beginPath();
            ctx.arc(x, y, radius, startAngle, progressAngle);
            ctx.strokeStyle = root.color;
            ctx.stroke();
        }

        Behavior on degree {
            NumberAnimation {
                duration: root.animationDuration
            }
        }
    }
}
