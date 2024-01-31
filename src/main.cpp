/* Copyright (C) 2017-2024 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef USE_SFOS
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

#ifdef USE_DESKTOP
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <memory>

#include "iconprovider.h"
#endif

#include <QFile>
#include <QList>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QTextCodec>
#include <QTranslator>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <libupnpp/control/description.hxx>
#include <libupnpp/control/discovery.hxx>
#include <variant>

#include "albummodel.h"
#include "artistmodel.h"
#include "avtransport.h"
#include "bcmodel.h"
#include "cdirmodel.h"
#include "config.h"
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
#include "notifications.h"
#include "playlistfilemodel.h"
#include "playlistmodel.h"
#include "py_executor.hpp"
#include "radionetmodel.hpp"
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
#ifndef USE_SFOS_HARBOUR
#include "mpdtools.hpp"
#include "ytmodel.h"
#endif

static void exitProgram() {
    qDebug() << "exiting";

    // workaround for python thread locking
    std::quick_exit(0);
}

static void signalHandler(int sig) {
    qDebug() << "received signal:" << sig;

    exitProgram();
}

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
#ifndef USE_SFOS_HARBOUR
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
    qmlRegisterType<RadionetModel>("harbour.jupii.RadionetModel", 1, 0,
                                   "RadionetModel");
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
    qRegisterMetaType<Settings::CasterStreamFormat>(
        "Settings::CasterStreamFormat");
    qRegisterMetaType<Settings::CasterVideoOrientation>(
        "Settings::CasterVideoOrientation");
    qRegisterMetaType<Settings::CasterVideoEncoder>(
        "Settings::CasterVideoEncoder");
    qRegisterMetaType<ContentServer::Type>("ContentServer::Type");
    qRegisterMetaType<ContentServer::CasterType>("ContentServer::CasterType");
    qRegisterMetaType<UrlInfo>("UrlInfo");
    qRegisterMetaType<std::shared_ptr<QFile>>("std::shared_ptr<QFile>");
    qRegisterMetaType<int64_t>("int64_t");
}

static void installTranslator() {
    auto* translator = new QTranslator{QCoreApplication::instance()};
#ifdef USE_SFOS
    auto transDir =
        SailfishApp::pathTo(QStringLiteral("translations")).toLocalFile();
#else
    auto transDir = QStringLiteral(":/translations");
#endif
    if (!translator->load(QLocale{}, QStringLiteral("jupii"),
                          QStringLiteral("-"), transDir,
                          QStringLiteral(".qm"))) {
        qDebug() << "failed to load translation:" << QLocale::system().name()
                 << transDir;
        if (!translator->load(QStringLiteral("jupii-en"), transDir)) {
            qDebug() << "failed to load default translation";
            delete translator;
            return;
        }
    } else {
        qDebug() << "translation:" << QLocale::system().name();
    }

    if (!QGuiApplication::installTranslator(translator)) {
        qWarning() << "failed to install translation";
    }
}

int main(int argc, char** argv) {
#ifdef USE_SFOS
    SailfishApp::application(argc, argv);
    auto* view = SailfishApp::createView();
    auto* context = view->rootContext();
    auto* engine = view->engine();
#endif
#ifdef USE_DESKTOP
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon{QStringLiteral(":/app_icon.svg")});

    auto engine = std::make_unique<QQmlApplicationEngine>();
    auto* context = engine->rootContext();
#endif
    QGuiApplication::setApplicationName(QStringLiteral(APP_ID));
    QGuiApplication::setOrganizationName(QStringLiteral(APP_ORG));
    QGuiApplication::setOrganizationDomain(QStringLiteral(APP_DOMAIN));
    QGuiApplication::setApplicationDisplayName(QStringLiteral(APP_NAME));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    registerTypes();

    engine->addImageProvider(QStringLiteral("icons"), new IconProvider{});

    context->setContextProperty(QStringLiteral("APP_NAME"), APP_NAME);
    context->setContextProperty(QStringLiteral("APP_ID"), APP_ID);
    context->setContextProperty(QStringLiteral("APP_VERSION"), APP_VERSION);
    context->setContextProperty(QStringLiteral("APP_COPYRIGHT_YEAR"),
                                APP_COPYRIGHT_YEAR);
    context->setContextProperty(QStringLiteral("APP_AUTHOR"), APP_AUTHOR);
    context->setContextProperty(QStringLiteral("APP_AUTHOR_EMAIL"),
                                APP_AUTHOR_EMAIL);
    context->setContextProperty(QStringLiteral("APP_SUPPORT_EMAIL"),
                                APP_SUPPORT_EMAIL);
    context->setContextProperty(QStringLiteral("APP_WEBPAGE"), APP_WEBPAGE);
    context->setContextProperty(QStringLiteral("APP_LICENSE"), APP_LICENSE);
    context->setContextProperty(QStringLiteral("APP_LICENSE_URL"),
                                APP_LICENSE_URL);
    context->setContextProperty(QStringLiteral("APP_LICENSE_SPDX"),
                                APP_LICENSE_SPDX);
    context->setContextProperty(QStringLiteral("APP_TRANSLATORS_STR"),
                                APP_TRANSLATORS_STR);
    context->setContextProperty(QStringLiteral("APP_LIBS_STR"), APP_LIBS_STR);

    qDebug() << "version:" << APP_VERSION;

    installTranslator();
    makeAppDirs();

    signal(SIGINT, signalHandler);

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

    py_executor::instance()->start();

#ifdef USE_SFOS
    ResourceHandler rhandler;
    QObject::connect(view, &QQuickView::focusObjectChanged, &rhandler,
                     &ResourceHandler::handleFocusChange);

    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/main.qml")));
    view->show();
    utils->setQmlRootItem(view->rootObject());
    QGuiApplication::exec();
#else
    engine->load(QUrl{QStringLiteral("qrc:/qml/main.qml")});
    QGuiApplication::exec();
#endif
    fcloseall();

#ifndef USE_SFOS_HARBOUR
    if (settings->controlMpdService()) mpdtools::stop();
#endif

    exitProgram();
}
