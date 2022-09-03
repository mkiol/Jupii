/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef SAILFISH
#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif
#include <sailfishapp.h>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtQml>

#include "iconprovider.h"
#include "resourcehandler.h"
#endif

#ifdef KIRIGAMI
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "iconprovider.h"
#endif

#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QTextCodec>
#include <QTranslator>
#include <cstdint>
#include <iostream>
#include <libupnpp/control/description.hxx>
#include <libupnpp/control/discovery.hxx>
#include <variant>

#include "albummodel.h"
#include "artistmodel.h"
#include "avtransport.h"
#include "bcmodel.h"
#include "cdirmodel.h"
#include "connectivitydetector.h"
#include "contentdirectory.h"
#include "contentserver.h"
#include "dbusapp.h"
#include "deviceinfo.h"
#include "devicemodel.h"
#include "directory.h"
#include "dirmodel.h"
#include "fosdemmodel.h"
#include "gpoddermodel.h"
#include "icecastmodel.h"
#include "info.h"
#include "notifications.h"
#include "playlistfilemodel.h"
#include "playlistmodel.h"
#include "recmodel.h"
#include "renderingcontrol.h"
#include "services.h"
#include "settings.h"
#include "somafmmodel.h"
#include "soundcloudmodel.h"
#include "trackmodel.h"
#include "tuneinmodel.h"
#include "utils.h"
#include "xc.h"
#ifndef HARBOUR
#include "ytmodel.h"
#endif

static void makeAppDirs() {
    auto root = QDir::root();
    root.mkpath(
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
    root.mkpath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    root.mkpath(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

static void registerTypes() {
    qmlRegisterUncreatableType<DeviceModel>(
        "harbour.jupii.DeviceModel", 1, 0, "DeviceModel",
        QStringLiteral("DeviceModel is a singleton"));
    qmlRegisterType<RenderingControl>("harbour.jupii.RenderingControl", 1, 0,
                                      "RenderingControl");
    qmlRegisterType<AVTransport>("harbour.jupii.AVTransport", 1, 0,
                                 "AVTransport");
    qmlRegisterType<DeviceInfo>("harbour.jupii.DeviceInfo", 1, 0, "DeviceInfo");
    qmlRegisterType<AlbumModel>("harbour.jupii.AlbumModel", 1, 0, "AlbumModel");
    qmlRegisterType<ArtistModel>("harbour.jupii.ArtistModel", 1, 0,
                                 "ArtistModel");
    qmlRegisterType<PlaylistFileModel>("harbour.jupii.PlaylistFileModel", 1, 0,
                                       "PlaylistFileModel");
    qmlRegisterType<TrackModel>("harbour.jupii.TrackModel", 1, 0, "TrackModel");
    qmlRegisterUncreatableType<PlaylistModel>(
        "harbour.jupii.PlayListModel", 1, 0, "PlayListModel",
        QStringLiteral("Playlist is a singleton"));
    qmlRegisterUncreatableType<ContentServer>(
        "harbour.jupii.ContentServer", 1, 0, "ContentServer",
        QStringLiteral("ContentServer is a singleton"));
    qmlRegisterType<SomafmModel>("harbour.jupii.SomafmModel", 1, 0,
                                 "SomafmModel");
    qmlRegisterType<FosdemModel>("harbour.jupii.FosdemModel", 1, 0,
                                 "FosdemModel");
    qmlRegisterType<BcModel>("harbour.jupii.BcModel", 1, 0, "BcModel");
    qmlRegisterType<SoundcloudModel>("harbour.jupii.SoundcloudModel", 1, 0,
                                     "SoundcloudModel");
#ifndef HARBOUR
    qmlRegisterType<YtModel>("harbour.jupii.YtModel", 1, 0, "YtModel");
#endif
    qmlRegisterType<IcecastModel>("harbour.jupii.IcecastModel", 1, 0,
                                  "IcecastModel");
    qmlRegisterType<GpodderEpisodeModel>("harbour.jupii.GpodderEpisodeModel", 1,
                                         0, "GpodderEpisodeModel");
    qmlRegisterType<DirModel>("harbour.jupii.DirModel", 1, 0, "DirModel");
    qmlRegisterType<RecModel>("harbour.jupii.RecModel", 1, 0, "RecModel");
    qmlRegisterType<CDirModel>("harbour.jupii.CDirModel", 1, 0, "CDirModel");
    qmlRegisterType<TuneinModel>("harbour.jupii.TuneinModel", 1, 0,
                                 "TuneinModel");
    qmlRegisterUncreatableType<Settings>(
        "harbour.jupii.Settings", 1, 0, "Settings",
        QStringLiteral("Settings is a singleton"));

    qRegisterMetaType<Service::ErrorType>("Service::ErrorType");
    qRegisterMetaType<QList<ListItem*>>("QListOfListItem");
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<XC::ErrorType>("XC::ErrorType");
    qRegisterMetaType<Settings::CacheType>("Settings::CacheType");
    qRegisterMetaType<Settings::CacheCleaningType>(
        "Settings::CacheCleaningType");
    qRegisterMetaType<Settings::YtPreferredType>("Settings::YtPreferredType");
    qRegisterMetaType<ContentServer::Type>("ContentServer::Type");
    qRegisterMetaType<std::shared_ptr<QFile>>("std::shared_ptr<QFile>");
    qRegisterMetaType<int64_t>("int64_t");
}

static void installTranslator() {
    auto* translator = new QTranslator{qApp};
#ifdef SAILFISH
    auto transDir =
        SailfishApp::pathTo(QStringLiteral("translations")).toLocalFile();
#else
    auto transDir = QStringLiteral(":/translations");
#endif
    if (!translator->load(QLocale{}, QStringLiteral("jupii"),
                          QStringLiteral("-"), transDir,
                          QStringLiteral(".qm"))) {
        qDebug() << "cannot load translation:" << QLocale::system().name()
                 << transDir;
        if (!translator->load(QStringLiteral("jupii-en"), transDir)) {
            qDebug() << "cannot load default translation";
            delete translator;
            return;
        }
    }

    QGuiApplication::installTranslator(translator);
}

int main(int argc, char** argv) {
#ifdef SAILFISH
    SailfishApp::application(argc, argv);
    auto* view = SailfishApp::createView();
    auto* context = view->rootContext();
    auto* engine = view->engine();
#endif
#ifdef KIRIGAMI
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    auto* app = new QApplication{argc, argv};
    auto* engine = new QQmlApplicationEngine();
    auto* context = engine->rootContext();
    QGuiApplication::setWindowIcon(QIcon::fromTheme(Jupii::APP_ID));
#endif
    QGuiApplication::setApplicationName(Jupii::APP_ID);
    QGuiApplication::setOrganizationName(Jupii::ORG);
    QGuiApplication::setApplicationDisplayName(Jupii::APP_NAME);
    QGuiApplication::setApplicationVersion(Jupii::APP_VERSION);

    registerTypes();

    engine->addImageProvider(QStringLiteral("icons"), new IconProvider{});

    context->setContextProperty(QStringLiteral("APP_NAME"), Jupii::APP_NAME);
    context->setContextProperty(QStringLiteral("APP_ID"), Jupii::APP_ID);
    context->setContextProperty(QStringLiteral("APP_VERSION"),
                                Jupii::APP_VERSION);
    context->setContextProperty(QStringLiteral("COPYRIGHT_YEAR"),
                                Jupii::COPYRIGHT_YEAR);
    context->setContextProperty(QStringLiteral("AUTHOR"), Jupii::AUTHOR);
    context->setContextProperty(QStringLiteral("AUTHOR_EMAIL"),
                                Jupii::AUTHOR_EMAIL);
    context->setContextProperty(QStringLiteral("SUPPORT_EMAIL"),
                                Jupii::SUPPORT_EMAIL);
    context->setContextProperty(QStringLiteral("PAGE"), Jupii::PAGE);
    context->setContextProperty(QStringLiteral("LICENSE"), Jupii::LICENSE);
    context->setContextProperty(QStringLiteral("LICENSE_URL"),
                                Jupii::LICENSE_URL);
    context->setContextProperty(QStringLiteral("LICENSE_SPDX"),
                                Jupii::LICENSE_SPDX);

    installTranslator();
    makeAppDirs();

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    auto* settings = Settings::instance();
    auto* utils = Utils::instance();
    auto* dir = Directory::instance();
    auto* cserver = ContentServer::instance();
    auto* services = Services::instance();
    auto* playlist = PlaylistModel::instance();
    auto* devmodel = DeviceModel::instance();
    auto* notifications = Notifications::instance();
    auto* conn = ConnectivityDetector::instance();
    DbusProxy dbus;

    services->avTransport->registerExternalConnections();
    cserver->registerExternalConnections();

    context->setContextProperty(QStringLiteral("utils"), utils);
    context->setContextProperty(QStringLiteral("settings"), settings);
    context->setContextProperty(QStringLiteral("_settings"), settings);
    context->setContextProperty(QStringLiteral("directory"), dir);
    context->setContextProperty(QStringLiteral("cserver"), cserver);
    context->setContextProperty(QStringLiteral("rc"),
                                services->renderingControl.get());
    context->setContextProperty(QStringLiteral("av"),
                                services->avTransport.get());
    context->setContextProperty(QStringLiteral("cdir"),
                                services->contentDir.get());
    context->setContextProperty(QStringLiteral("playlist"), playlist);
    context->setContextProperty(QStringLiteral("devmodel"), devmodel);
    context->setContextProperty(QStringLiteral("notifications"), notifications);
    context->setContextProperty(QStringLiteral("conn"), conn);
    context->setContextProperty(QStringLiteral("dbus"), &dbus);

#ifdef SAILFISH
    ResourceHandler rhandler;
    QObject::connect(view, &QQuickView::focusObjectChanged, &rhandler,
                     &ResourceHandler::handleFocusChange);

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/main.qml")));
    view->show();
    utils->setQmlRootItem(view->rootObject());
    int ret = QGuiApplication::exec();
#else
    engine->load(QUrl{QStringLiteral("qrc:/qml/main.qml")});
    int ret = QGuiApplication::exec();

    delete engine;
    delete app;
#endif
    fcloseall();
    return ret;
}
