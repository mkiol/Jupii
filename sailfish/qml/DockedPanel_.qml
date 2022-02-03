/****************************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Andrew den Exter <andrew.den.exter@jollamobile.com>
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

SilicaFlickable {
    id: panel

    readonly property bool expanded: open || horizontalAnimation.running || verticalAnimation.running
    readonly property bool running: horizontalAnimation.running || verticalAnimation.running
    readonly property bool moving: horizontalAnimation.running || verticalAnimation.running || mouseArea.drag.active
    readonly property bool dragging: mouseArea.drag.active
    readonly property alias pressed: mouseArea.pressed

    property bool open

    property int dock: Dock.Bottom
    property bool modal
    property int animationDuration: 500
    property bool _immediate

    signal requestFull
    signal requestCompact

    function _visibleSize() {
        if (_managedDock == Dock.Left) {
            return panel.x + panel.width - contentX
        } else if (_managedDock == Dock.Top)  {
            return panel.y + panel.height - contentY
        } else if (_managedDock == Dock.Right && panel.parent) {
            return panel.parent.width - panel.x + contentX
        } else if (_managedDock == Dock.Bottom && panel.parent) {
            return panel.parent.height - panel.y + contentY
        } else {
            return 0
        }
    }

    readonly property real visibleSize: _visibleSize()

    property real _lastPos
    property real _direction
    default property alias _data: mouseArea.data

    property int _managedDock: dock
    property bool _isVertical: _managedDock == Dock.Top || _managedDock == Dock.Bottom
    property real _threshold: Math.min((_isVertical ? height / 3 : width / 3), Theme.pixelRatio*90)
    property bool _initialized

    property Component background: PanelBackground {
        position: dock
    }

    visible: expanded

    onOpenChanged: _initialized = true
    function show(immediate) {
        _initialized = true
        _immediate = !!immediate
        open = true
        _immediate = false
    }

    function hide(immediate) {
        _immediate = !!immediate
        open = false
        _immediate = false
    }

    Component.onCompleted: {
        if (parent === __silica_applicationwindow_instance.contentItem) {
            // The panel is most probably a direct child of ApplicationWindow and should not be
            // parented on a resizing item, i.e. contentItem, but a non-resizing (but still
            // orientation aware) item instead, i.e. _rotatingItem.
            parent = __silica_applicationwindow_instance._rotatingItem
        }
    }

    onDockChanged: {
        _immediate = true
        _managedDock = dock
        _immediate = false
    }

    function reportRequest() {
        if (!running) {
            if (full) panel.requestCompact()
            else panel.requestFull()
        }
    }

    InverseMouseArea {
        anchors.fill: parent
        enabled: open && modal
        stealPress: true
        onPressedOutside: open = false
    }

    Binding {
        when: open && modal
        target: __silica_applicationwindow_instance
        property: "_dimScreen"
        value: open
    }

    // Effect to eliminate the opacity applied by application window dimming
    layer.enabled: modal && expanded && __silica_applicationwindow_instance._dimmingActive
    layer.smooth: true
    layer.effect: ShaderEffect {
        fragmentShader: "
            uniform sampler2D source;
            varying highp vec2 qt_TexCoord0;
            void main(void)
            {
                highp vec4 pixelColor = texture2D(source, qt_TexCoord0);
                gl_FragColor = pixelColor;
            }
            "
    }

    Binding {
        target: panel
        property: "x"
        value: {
            if (_managedDock == Dock.Left) {
                return open ? 0 : -panel.width
            } else if (_managedDock == Dock.Right && panel.parent) {
                return open ? panel.parent.width - panel.width : panel.parent.width
            } else {
                return 0
            }
        }
        when: horizontalBehavior.enabled || panel._immediate || !panel._initialized
    }
    Behavior on x {
        id: horizontalBehavior
        enabled: !mouseArea.drag.active && !panel._immediate && panel._initialized
        NumberAnimation {
            id: horizontalAnimation
            duration: animationDuration; easing.type: Easing.OutQuad
        }
    }

    Binding {
        target: panel
        property: "y"
        value: {
            if (_managedDock == Dock.Top) {
                return open ? 0 : -panel.height
            } else if (_managedDock == Dock.Bottom && panel.parent) {
                return open ? panel.parent.height - panel.height : panel.parent.height
            } else {
                return 0
            }
        }
        when: verticalBehavior.enabled || panel._immediate || !panel._initialized
    }
    Behavior on y {
        id: verticalBehavior
        enabled: !mouseArea.drag.active && !panel._immediate && panel._initialized
        NumberAnimation {
            id: verticalAnimation
            duration: animationDuration; easing.type: Easing.OutQuad
        }
    }

    Loader {
        sourceComponent: background
        anchors.fill: mouseArea
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        drag {
            target: panel
            axis: Drag.YAxis
            minimumY: panel.parent.height - panel.height -
                      (panel.full ? 0 : Theme.paddingLarge)
            maximumY: panel.parent.height - panel.height +
                      (panel.full ? Theme.paddingLarge : 0)
            onActiveChanged: {
                if (mouseArea.drag.active) panel.reportRequest()
            }
        }

        onClicked: {
            panel.reportRequest()
        }
    }
}
