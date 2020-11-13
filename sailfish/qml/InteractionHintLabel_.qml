/****************************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Joona Petrell <joona.petrell@jollamobile.com>
** All rights reserved.
**
** This file is part of Sailfish Silica UI component package.
**
** You may use this file under the terms of BSD license as follows:
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the Jolla Ltd nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR
** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************************/

import QtQuick 2.0
import Sailfish.Silica 1.0

Rectangle {
    property bool invert
    property alias text: label.text
    property alias subtext: sublabel.text
    property alias textFontSize: label.font.pixelSize
    property alias subtextFontSize: sublabel.font.pixelSize
    property color backgroundColor: Theme.rgba(palette.highlightDimmerColor, 0.9)

    height: parent.height / 2
    width: parent.width
    gradient: Gradient {
        GradientStop { position: invert ? 0.8 : 0.0; color: "transparent" }
        GradientStop { position: invert ? 0.3 : 0.6; color: backgroundColor }
    }

    Column {
        anchors {
            top: invert ? parent.top : undefined
            bottom: invert ? undefined : parent.bottom
            left: parent.left
            right: parent.right
            bottomMargin: Theme.paddingLarge*2
            topMargin: Theme.paddingLarge*2
        }
        InfoLabel {
            id: label
            color: Theme.highlightColor
        }
        InfoLabel {
            id: sublabel
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeLarge
        }
    }
}
