/* Copyright (C) 2017-2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef SAILFISH
#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif
#include <QGuiApplication>
#include <QtQml>
#include <QQmlEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QQuickItem>
#include <sailfishapp.h>
#include "iconprovider.h"
#include "resourcehandler.h"
#endif // SAILFISH

#ifdef KIRIGAMI
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "iconprovider.h"
#endif // KIRIGAMI

#include <QLocale>
#include <QTranslator>
#include <QTextCodec>
#include <QProcess>
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
#include "playlistmodel.h"
#include "dbusapp.h"
#include "albummodel.h"
#include "artistmodel.h"
#include "playlistfilemodel.h"
#include "playlistmodel.h"
#include "trackmodel.h"
#include "services.h"
#include "info.h"
#include "somafmmodel.h"
#include "fosdemmodel.h"
#include "gpoddermodel.h"
#include "icecastmodel.h"
#include "dirmodel.h"
#include "recmodel.h"
#include "devicemodel.h"
#include "contentdirectory.h"
#include "cdirmodel.h"
#include "youtubedl.h"
#include "bcmodel.h"
#include "notifications.h"

int main(int argc, char *argv[])
{
#ifdef SAILFISH
    auto app = SailfishApp::application(argc, argv);
    auto view = SailfishApp::createView();
    auto context = view->rootContext();
    auto engine = view->engine();
#endif // SAILFISH

#ifdef KIRIGAMI
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    auto app = new QApplication(argc, argv);
    auto engine = new QQmlApplicationEngine();
    auto context = engine->rootContext();
    //app->setOrganizationName(app->applicationName());
    QApplication::setWindowIcon(QIcon::fromTheme(Jupii::APP_ID));
#endif // KIRIGAMI

    qmlRegisterUncreatableType<DeviceModel>("harbour.jupii.DeviceModel", 1, 0,
                                            "DeviceModel", "DeviceModel is a singleton");
    qmlRegisterType<RenderingControl>("harbour.jupii.RenderingControl", 1, 0,
                                      "RenderingControl");
    qmlRegisterType<AVTransport>("harbour.jupii.AVTransport", 1, 0, "AVTransport");
    qmlRegisterType<DeviceInfo>("harbour.jupii.DeviceInfo", 1, 0, "DeviceInfo");
    qmlRegisterType<AlbumModel>("harbour.jupii.AlbumModel", 1, 0, "AlbumModel");
    qmlRegisterType<ArtistModel>("harbour.jupii.ArtistModel", 1, 0, "ArtistModel");
    qmlRegisterType<PlaylistFileModel>("harbour.jupii.PlaylistFileModel", 1, 0,
                                       "PlaylistFileModel");
    qmlRegisterType<TrackModel>("harbour.jupii.TrackModel", 1, 0, "TrackModel");
    qmlRegisterUncreatableType<PlaylistModel>("harbour.jupii.PlayListModel", 1, 0,
                                              "PlayListModel", "Playlist is a singleton");
    qmlRegisterUncreatableType<ContentServer>("harbour.jupii.ContentServer", 1, 0,
                                              "ContentServer", "ContentServer is a singleton");
    qmlRegisterType<SomafmModel>("harbour.jupii.SomafmModel", 1, 0, "SomafmModel");
    qmlRegisterType<FosdemModel>("harbour.jupii.FosdemModel", 1, 0, "FosdemModel");
    qmlRegisterType<BcModel>("harbour.jupii.BcModel", 1, 0, "BcModel");
    qmlRegisterType<IcecastModel>("harbour.jupii.IcecastModel", 1, 0, "IcecastModel");
    qmlRegisterType<GpodderEpisodeModel>("harbour.jupii.GpodderEpisodeModel", 1, 0,
                                         "GpodderEpisodeModel");
    qmlRegisterType<DirModel>("harbour.jupii.DirModel", 1, 0, "DirModel");
    qmlRegisterType<RecModel>("harbour.jupii.RecModel", 1, 0, "RecModel");
    qmlRegisterType<CDirModel>("harbour.jupii.CDirModel", 1, 0, "CDirModel");
    qmlRegisterUncreatableType<YoutubeDl>("harbour.jupii.YoutubeDl", 1, 0, "YoutubeDl",
                                          "YoutubeDl is a singleton");

    engine->addImageProvider(QLatin1String("icons"), new IconProvider);

    context->setContextProperty("APP_NAME", Jupii::APP_NAME);
    context->setContextProperty("APP_ID", Jupii::APP_ID);
    context->setContextProperty("APP_VERSION", Jupii::APP_VERSION);
    context->setContextProperty("COPYRIGHT_YEAR", Jupii::COPYRIGHT_YEAR);
    context->setContextProperty("AUTHOR", Jupii::AUTHOR);
    context->setContextProperty("AUTHOR_EMAIL", Jupii::AUTHOR_EMAIL);
    context->setContextProperty("SUPPORT_EMAIL", Jupii::SUPPORT_EMAIL);
    context->setContextProperty("PAGE", Jupii::PAGE);
    context->setContextProperty("LICENSE", Jupii::LICENSE);
    context->setContextProperty("LICENSE_URL", Jupii::LICENSE_URL);
    context->setContextProperty("LICENSE_SPDX", Jupii::LICENSE_SPDX);

    app->setApplicationDisplayName(Jupii::APP_NAME);
    app->setApplicationVersion(Jupii::APP_VERSION);

    qRegisterMetaType<Service::ErrorType>("ErrorType");
    qRegisterMetaType<QList<ListItem*>>("QListOfListItem");
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");

    auto settings = Settings::instance();

#ifdef SAILFISH
    QTranslator translator;
    auto transDir = SailfishApp::pathTo("translations").toLocalFile();
    auto locale = QLocale::system().name();
    if(!translator.load(locale, "harbour-jupii", "-", transDir, ".qm")) {
        qDebug() << "Cannot load translation:" << locale << transDir;
        if (!translator.load("harbour-jupii-en", transDir)) {
            qDebug() << "Cannot load default translation";
        }
    }
    app->installTranslator(&translator);
#endif // SAILFISH

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

    auto utils = Utils::instance();
    auto dir = Directory::instance();
    auto cserver = ContentServer::instance();
    auto services = Services::instance();
    auto playlist = PlaylistModel::instance();
    auto devmodel = DeviceModel::instance();
    auto ytdl = YoutubeDl::instance();
    new DbusProxy();

    services->avTransport->registerExternalConnections();
    cserver->registerExternalConnections();

    context->setContextProperty("utils", utils);
    context->setContextProperty("settings", settings);
    context->setContextProperty("_settings", settings);
    context->setContextProperty("directory", dir);
    context->setContextProperty("cserver", cserver);
    context->setContextProperty("rc", services->renderingControl.get());
    context->setContextProperty("av", services->avTransport.get());
    context->setContextProperty("cdir", services->contentDir.get());
    context->setContextProperty("playlist", playlist);
    context->setContextProperty("devmodel", devmodel);
    context->setContextProperty("ytdl", ytdl);

#ifdef KIRIGAMI
    auto notifications = Notifications::instance();
    context->setContextProperty("notifications", notifications);
#endif

#ifdef SAILFISH
    view->setSource(SailfishApp::pathTo("qml/main.qml"));

    auto rhandler = new ResourceHandler();
    QObject::connect(view, &QQuickView::focusObjectChanged,
                     rhandler, &ResourceHandler::handleFocusChange);

    view->show();
    utils->setQmlRootItem(view->rootObject());
#endif // SAILFISH

#ifdef KIRIGAMI
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    engine->load(url);
#endif // KIRIGAMI

    int ret = app->exec();
    fcloseall();
    return ret;
}
