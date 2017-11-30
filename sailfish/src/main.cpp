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

using namespace std;

static const char* APP_NAME = "Jupii";
static const char* AUTHOR = "Michal Kosciesza <michal@mkiol.net>";
static const char* PAGE = "https://github.com/mkiol/Jupii";
static const char* LICENSE = "http://mozilla.org/MPL/2.0/";
static const char* VERSION = "0.9.1";

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    auto context = view->rootContext();
    auto engine = view->engine();

    qmlRegisterType<DeviceModel>("harbour.jupii.DeviceModel", 1, 0, "DeviceModel");
    qmlRegisterType<RenderingControl>("harbour.jupii.RenderingControl", 1, 0, "RenderingControl");
    qmlRegisterType<AVTransport>("harbour.jupii.AVTransport", 1, 0, "AVTransport");
    qmlRegisterType<DeviceInfo>("harbour.jupii.DeviceInfo", 1, 0, "DeviceInfo");
    qmlRegisterType<PlayListModel>("harbour.jupii.PlayListModel", 1, 0, "PlayListModel");
    qRegisterMetaType<Service::ErrorType>("ErrorType");
    engine->addImageProvider(QLatin1String("icons"), new IconProvider);

    context->setContextProperty("APP_NAME", APP_NAME);
    context->setContextProperty("VERSION", VERSION);
    context->setContextProperty("AUTHOR", AUTHOR);
    context->setContextProperty("PAGE", PAGE);
    context->setContextProperty("LICENSE", LICENSE);

    Utils* utils = Utils::instance();
    Settings* settings = Settings::instance();
    Directory* dir = Directory::instance();
    ContentServer* cserver = ContentServer::instance();

    context->setContextProperty("directory", dir);
    context->setContextProperty("utils", utils);
    context->setContextProperty("settings", settings);
    context->setContextProperty("cserver", cserver);

    view->setSource(SailfishApp::pathTo("qml/main.qml"));
    view->show();
    return app->exec();
}
