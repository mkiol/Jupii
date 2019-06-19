#
# sailfishapp.prf: Qt Feature file for libsailfishapp
# Usage: CONFIG += sailfishapp
#
# Copyright (c) 2013 - 2014 Jolla Ltd.
# Contact: Thomas Perl <thomas.perl@jollamobile.com>
# All rights reserved.
#
# This file is part of libsailfishapp
#
# You may use this file under the terms of BSD license as follows:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the Jolla Ltd nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

QT += quick qml

target.path = /usr/bin

!sailfishapp_no_deploy_qml {
    qml.files = qml
    qml.path = /usr/share/$${TARGET}

    INSTALLS += qml
}

desktop.files = $${TARGET}.desktop
desktop.path = /usr/share/applications

INSTALLS += target desktop

!isEmpty(SAILFISHAPP_ICONS):for(size, SAILFISHAPP_ICONS) {
    icon$${size}.files = icons/$${size}/$${TARGET}.png
    icon$${size}.path = /usr/share/icons/hicolor/$${size}/apps
    INSTALLS += icon$${size}
} else {
    icon.files = $${TARGET}.png
    icon.path = /usr/share/icons/hicolor/86x86/apps
    INSTALLS += icon
}

CONFIG += link_pkgconfig
PKGCONFIG += sailfishapp
INCLUDEPATH += /usr/include/sailfishapp

QMAKE_RPATHDIR += /usr/share/$${TARGET}/lib

OTHER_FILES += $$files(rpm/*)