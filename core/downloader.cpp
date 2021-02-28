/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "downloader.h"

#include <QDebug>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QThread>

Downloader::Downloader(std::shared_ptr<QNetworkAccessManager> nam, QObject *parent) :
    QObject(parent), nam(nam)
{
}

QByteArray Downloader::downloadData(const QUrl &url)
{
#ifdef QT_DEBUG
    qDebug() << "Download data:" << url;
#endif

    QNetworkRequest request;
    request.setUrl(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply* reply;
    std::unique_ptr<QNetworkAccessManager> priv_nam;

    if (nam) {
        reply = nam->get(request);
    } else {
        priv_nam = std::make_unique<QNetworkAccessManager>();
        reply = priv_nam->get(request);
    }

    QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, this, [&loop] {
        loop.quit();
    });

    loop.exec();

#ifdef QT_DEBUG
    qDebug() << "Download finished";
#endif

    QByteArray data;

    if (auto err = reply->error(); err != QNetworkReply::NoError)
        qWarning() << "Error:" << err << reply->url();
    else
        data = reply->readAll();

    reply->deleteLater();

    return data;
}
