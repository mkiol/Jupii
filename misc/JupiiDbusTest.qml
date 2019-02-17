import QtQuick 2.0
import Nemo.DBus 2.0
import Sailfish.Silica 1.0

ApplicationWindow
{
    initialPage: Component{
        Page {
            id: root

            DBusInterface {
                id: dbus

                property bool canControl: false

                service: "org.jupii"
                path: "/"
                iface: "org.jupii.Player"

                signalsEnabled: true

                Component.onCompleted: getCanControl()

                function canControlPropertyChanged(value) {
                    console.log("canControlPropertyChanged: " + value)
                    canControl = value
                }

                function getCanControl() {
                    canControl = (getProperty('canControl') === true)
                }

                function addPath() {
                    var path = "/media/sdcard/SailfishX/music/soma/lackluster-one_cycle_more.mp3"
                    var name = "";
                    call('addPath', [path, name])
                }

                function addPathOnce() {
                    var path = "/media/sdcard/SailfishX/music/soma/lackluster-one_cycle_more.mp3"
                    var name = "";
                    call('addPathOnce', [path, name])
                }

                function addPathOnceAndPlay() {
                    var path = "/media/sdcard/SailfishX/music/soma/lackluster-one_cycle_more.mp3"
                    var name = "";
                    call('addPathOnceAndPlay', [path, name])
                }

                function addUrl() {
                    var url = "http://somafm.com/groovesalad.pls"
                    var name = "";
                    call('addUrl', [url, name])
                }

                function addUrlOnce() {
                    var url = "http://somafm.com/groovesalad.pls"
                    var name = "";
                    call('addUrlOnce', [url, name])
                }

                function addUrlOnceAndPlay() {
                    var url = "http://somafm.com/groovesalad.pls"
                    var name = "";
                    call('addUrlOnceAndPlay', [url, name])
                }

                function addUrlOnceHls() {
                    var url = "http://svtplay2r-f.akamaihd.net/i/world/open/20190114/1392573-009A/PG-1392573-009A-VADER_,988,240,348,456,636,1680,2796,.mp4.csmil/master.m3u8"
                    var name = "HLS url";
                    call('addUrlOnce', [url, name])
                }

                function clearPlaylist() {
                    call('clearPlaylist')
                }
            }

            SilicaFlickable {
                anchors.fill: parent
                contentHeight: column.height

                Column {
                    id: column

                    x: Theme.paddingLarge
                    width: root.width - x
                    spacing: Theme.paddingLarge

                    PageHeader {
                        title: qsTr("Jupii DBus tester")
                    }

                    Label {
                        text: dbus.canControl ? "OK" : "NOK"
                    }

                    Button {
                        text: "Add path"
                        onClicked: {
                            dbus.addPath()
                        }
                    }

                    Button {
                        text: "Add path once"
                        onClicked: {
                            dbus.addPathOnce()
                        }
                    }

                    Button {
                        text: "Add path once and play"
                        onClicked: {
                            dbus.addPathOnceAndPlay()
                        }
                    }

                    Button {
                        text: "Add URL"
                        onClicked: {
                            dbus.addUrl()
                        }
                    }

                    Button {
                        text: "Add URL once"
                        onClicked: {
                            dbus.addUrlOnce()
                        }
                    }

                    Button {
                        text: "Add URL once and play"
                        onClicked: {
                            dbus.addUrlOnceAndPlay()
                        }
                    }

                    Button {
                        text: "Add HLS URL once"
                        onClicked: {
                            dbus.addUrlOnceHls()
                        }
                    }
                }
            }
        }
    }
}
