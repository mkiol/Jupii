/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef AVTRANSPORT_H
#define AVTRANSPORT_H

#include <QString>
#include <QTimer>
#include <QMutex>
#include <QMetaMethod>
#include <QUrl>

#include "libupnpp/control/avtransport.hxx"
#include "libupnpp/control/service.hxx"
#include "libupnpp/control/cdircontent.hxx"

#include "service.h"
#include "contentserver.h"

class AVTransport : public Service
{
    Q_OBJECT
    Q_PROPERTY (QString currentURI READ getCurrentURI NOTIFY currentURIChanged)
    Q_PROPERTY (QUrl currentId READ getCurrentId NOTIFY currentURIChanged)
    Q_PROPERTY (QUrl currentOrigURL READ getCurrentOrigUrl NOTIFY currentURIChanged)
    Q_PROPERTY (QString currentPath READ getCurrentPath NOTIFY currentURIChanged)
    Q_PROPERTY (QString currentURL READ getCurrentURL NOTIFY currentURIChanged)
    Q_PROPERTY (QString currentContentType READ getContentType NOTIFY currentURIChanged)
    Q_PROPERTY (bool currentYtdl READ getCurrentYtdl NOTIFY currentURIChanged)
    Q_PROPERTY (QString nextURI READ getNextURI NOTIFY nextURIChanged)
    Q_PROPERTY (QString nextPath READ getNextPath NOTIFY nextURIChanged)
    Q_PROPERTY (bool nextURISupported READ getNextURISupported NOTIFY nextURISupportedChanged)

    Q_PROPERTY (int transportState READ getTransportState NOTIFY transportStateChanged)
    Q_PROPERTY (int transportStatus READ getTransportStatus NOTIFY transportStatusChanged)
    Q_PROPERTY (int playMode READ getPlayMode NOTIFY playModeChanged)
    Q_PROPERTY (int numberOfTracks READ getNumberOfTracks NOTIFY numberOfTracksChanged)
    Q_PROPERTY (int currentTrack READ getCurrentTrack NOTIFY currentTrackChanged)
    Q_PROPERTY (int currentTrackDuration READ getCurrentTrackDuration NOTIFY currentTrackDurationChanged)
    Q_PROPERTY (int relativeTimePosition READ getRelativeTimePosition NOTIFY relativeTimePositionChanged)
    Q_PROPERTY (int absoluteTimePosition READ getAbsoluteTimePosition NOTIFY absoluteTimePositionChanged)
    Q_PROPERTY (int speed READ getSpeed WRITE setSpeed NOTIFY speedChanged)

    Q_PROPERTY (Type currentType READ getCurrentType NOTIFY currentMetaDataChanged)
    Q_PROPERTY (QString currentClass READ getCurrentClass NOTIFY currentMetaDataChanged)
    Q_PROPERTY (QString currentTitle READ getCurrentTitle NOTIFY currentMetaDataChanged)
    Q_PROPERTY (QString currentAuthor READ getCurrentAuthor NOTIFY currentMetaDataChanged)
    Q_PROPERTY (QString currentDescription READ getCurrentDescription NOTIFY currentMetaDataChanged)
    Q_PROPERTY (QUrl currentAlbumArtURI READ getCurrentAlbumArtURI NOTIFY currentAlbumArtChanged)
    Q_PROPERTY (QString currentAlbum READ getCurrentAlbum NOTIFY currentMetaDataChanged)

    Q_PROPERTY (bool nextSupported READ getNextSupported NOTIFY transportActionsChanged)
    Q_PROPERTY (bool pauseSupported READ getPauseSupported NOTIFY transportActionsChanged)
    Q_PROPERTY (bool playSupported READ getPlaySupported NOTIFY transportActionsChanged)
    Q_PROPERTY (bool previousSupported READ getPreviousSupported NOTIFY transportActionsChanged)
    Q_PROPERTY (bool seekSupported READ getSeekSupported NOTIFY transportActionsChanged)
    Q_PROPERTY (bool stopSupported READ getStopSupported NOTIFY transportActionsChanged)
    //Q_PROPERTY (bool playModeSupported READ getStopSupported NOTIFY transportActionsChanged)

    Q_PROPERTY (bool playable READ getPlayable NOTIFY controlableChanged)
    Q_PROPERTY (bool stopable READ getStopable NOTIFY controlableChanged)
    Q_PROPERTY (bool controlable READ getControlable NOTIFY controlableChanged)

public:
    enum TransportState {
        Unknown,
        Stopped,
        Playing,
        Transitioning,
        PausedPlayback,
        PausedRecording,
        Recording,
        NoMediaPresent
    };
    Q_ENUM(TransportState)

    enum TransportStatus {
        TPS_Unknown,
        TPS_Ok,
        TPS_Error
    };
    Q_ENUM(TransportStatus)

    enum PlayMode {
        PM_Unknown,
        PM_Normal,
        PM_Shuffle,
        PM_RepeatOne,
        PM_RepeatAll,
        PM_Random,
        PM_Direct1
    };
    Q_ENUM(PlayMode)

    enum Type {
        T_Unknown = 0,
        T_Image = 1,
        T_Audio = 2,
        T_Video = 4
    };
    Q_ENUM(Type)

    explicit AVTransport(QObject *parent = nullptr);
    void registerExternalConnections();

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(int value);
    Q_INVOKABLE void setLocalContent(const QString &cid, const QString &nid);
    Q_INVOKABLE void asyncUpdate(int initDelay = 0, int postDelay = 500);
    Q_INVOKABLE void setPlayMode(int value);
    int getTransportState();
    int getTransportStatus();
    int getPlayMode();
    int getNumberOfTracks();
    int getCurrentTrack();
    int getCurrentTrackDuration();
    int getRelativeTimePosition();
    int getAbsoluteTimePosition();
    int getSpeed();
    Type getCurrentType();
    QString getCurrentURI();
    QString getNextURI();
    QString getCurrentPath();
    QString getCurrentURL();
    QString getContentType();
    QString getNextPath();
    QString getCurrentClass();
    QString getCurrentAuthor();
    QString getCurrentTitle();
    QString getCurrentDescription();
    QString getCurrentAlbum();
    QUrl getCurrentAlbumArtURI();
    QUrl getCurrentId();
    QUrl getCurrentOrigUrl();
    bool getNextSupported();
    bool getPauseSupported();
    bool getPlaySupported();
    bool getPreviousSupported();
    bool getSeekSupported();
    bool getStopSupported();
    bool getNextURISupported();
    bool getPlayable();
    bool getStopable();
    bool getControlable();
    bool getCurrentYtdl();
    void setSpeed(int value);
    bool updating();

signals:
    void transportStateChanged();
    void transportStatusChanged();
    void playModeChanged();
    void numberOfTracksChanged();
    void currentTrackChanged();
    void currentTrackDurationChanged();
    void relativeTimePositionChanged();
    void absoluteTimePositionChanged();
    void currentMetaDataChanged();
    void currentAlbumArtChanged();
    void currentURIChanged();
    void nextURIChanged();
    void speedChanged();
    void transportActionsChanged();
    void updated();
    void preControlableChanged();
    void controlableChanged();
    void nextURISupportedChanged();
    void trackEnded();

private slots:
    void timerEvent();
    void transportStateHandler();
    void trackChangedHandler();
    void seekTimeout();
    void handleApplicationStateChanged(Qt::ApplicationState state);
    void handleActiveItemChanged();

private:
    int m_transportState = Unknown;
    int m_oldTransportState = Unknown;
    int m_transportStatus = TPS_Unknown;
    int m_playmode = PM_Unknown;
    int m_numberOfTracks = 0;
    int m_currentTrack = 0;
    int m_currentTrackDuration = 0;
    int m_relativeTimePosition = 0;
    int m_absoluteTimePosition = 0;
    int m_speed = 1;
    int m_currentTransportActions = 0;
    int m_oldTimePosition = 0;
    QString m_id;
    QString m_currentClass;
    QString m_currentTitle;
    QString m_currentAuthor;
    QString m_currentDescription;
    QUrl m_currentAlbumArtURI;
    QString m_currentAlbum;
    QString m_streamTitle;
    QUrl m_streamId;

    QString m_currentURI;
    QString m_nextURI;
    bool m_nextURISupported = true;
    bool m_emitCurrentUriChanged = false;
    bool m_emitNextUriChanged = false;
    bool m_blockEmitUriChanged = false;
    bool m_pendingControlableSignal = false;
    bool m_stopCalled = false;

    QTimer m_seekTimer;
    int m_futureSeek = 0;

    QMutex m_updateMutex;

    void changed(const QString &name, const QVariant &value);
    UPnPClient::Service* createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                           const UPnPClient::UPnPServiceDesc &sdesc);
    void postInit();
    void reset();

    UPnPClient::AVTransport* s();
    void fakeUpdateRelativeTimePosition();
    void updateTransportInfo();
    void updateTransportSettings();
    void updatePositionInfo();
    void updateMediaInfo();
    void updateCurrentTransportActions();
    void update(int initDelay = 500, int postDelay = 500);
    void asyncUpdateTransportInfo();
    void asyncUpdateTransportSettings();
    void asyncUpdatePositionInfo();
    void asyncUpdateMediaInfo();
    void asyncUpdateCurrentTransportActions();
    void updateTrackMeta(const UPnPClient::UPnPDirObject &trackmeta);
    void needTimerCheck();
    void controlableChangedHandler();
    void blockUriChanged(int time = 500);
    void setNextURISupported(bool value);
    bool isIgnoreActions();
    std::string type() const;
};

#endif // AVTRANSPORT_H
