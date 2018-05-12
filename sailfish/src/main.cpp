/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QGuiApplication>
#include <QScopedPointer>
#include <QtQml>
#include <QQmlEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QLocale>
#include <QTranslator>
#include <QTextCodec>

#include <sailfishapp.h>

#include <iostream>

#include <libupnpp/control/discovery.hxx>
#include <libupnpp/control/description.hxx>

#include "utils.h"
#include "directory.h"
#include "devicemodel.h"
#include "renderingcontrol.h"
#include "avtransport.h"
#include "contentserver.h"
#include "filemetadata.h"
#include "settings.h"
#include "deviceinfo.h"
#include "iconprovider.h"
#include "playlistmodel.h"
#include "dbusapp.h"
#include "albummodel.h"
#include "artistmodel.h"
#include "playlistfilemodel.h"
#include "trackmodel.h"

using namespace std;

static const char* APP_NAME = "Jupii";
static const char* APP_VERSION = "0.9.4";
static const char* AUTHOR = "Michal Kosciesza <michal@mkiol.net>";
static const char* PAGE = "https://github.com/mkiol/Jupii";
static const char* LICENSE = "http://mozilla.org/MPL/2.0/";

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    auto context = view->rootContext();
    auto engine = view->engine();

    app->setApplicationDisplayName(APP_NAME);
    app->setApplicationVersion(APP_VERSION);

    qmlRegisterType<DeviceModel>("harbour.jupii.DeviceModel", 1, 0, "DeviceModel");
    qmlRegisterType<RenderingControl>("harbour.jupii.RenderingControl", 1, 0, "RenderingControl");
    qmlRegisterType<AVTransport>("harbour.jupii.AVTransport", 1, 0, "AVTransport");
    qmlRegisterType<DeviceInfo>("harbour.jupii.DeviceInfo", 1, 0, "DeviceInfo");
    qmlRegisterType<PlayListModel>("harbour.jupii.PlayListModel", 1, 0, "PlayListModel");
    qmlRegisterType<AlbumModel>("harbour.jupii.AlbumModel", 1, 0, "AlbumModel");
    qmlRegisterType<ArtistModel>("harbour.jupii.ArtistModel", 1, 0, "ArtistModel");
    qmlRegisterType<PlaylistFileModel>("harbour.jupii.PlaylistFileModel", 1, 0, "PlaylistFileModel");
    qmlRegisterType<TrackModel>("harbour.jupii.TrackModel", 1, 0, "TrackModel");
    qRegisterMetaType<Service::ErrorType>("ErrorType");

    engine->addImageProvider(QLatin1String("icons"), new IconProvider);

    context->setContextProperty("APP_NAME", APP_NAME);
    context->setContextProperty("APP_VERSION", APP_VERSION);
    context->setContextProperty("AUTHOR", AUTHOR);
    context->setContextProperty("PAGE", PAGE);
    context->setContextProperty("LICENSE", LICENSE);

    QTranslator translator;
    const QString locale = QLocale::system().name();
    const QString transDir = SailfishApp::pathTo("translations").toLocalFile();
    if(translator.load("harbour-jupii_" + locale, transDir)) {
        app->installTranslator(&translator);
    } else {
        qWarning() << "Couldn't load translation for" << locale;
        if (translator.load("harbour-jupii", transDir)) {
            app->installTranslator(&translator);
        } else {
            qWarning() << "Couldn't load default translation";
        }
    }

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    Utils* utils = Utils::instance();
    context->setContextProperty("utils", utils);

    Settings* settings = Settings::instance();
    context->setContextProperty("settings", settings);

    Directory* dir = Directory::instance();
    context->setContextProperty("directory", dir);

    ContentServer* cserver = ContentServer::instance();
    context->setContextProperty("cserver", cserver);

    DbusProxy dbusProxy;
    context->setContextProperty("dbus", &dbusProxy);

    view->setSource(SailfishApp::pathTo("qml/main.qml"));
    view->show();
    return app->exec();
}
