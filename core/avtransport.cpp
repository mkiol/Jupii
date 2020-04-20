/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QUrl>
#include <QThread>
#include <QGuiApplication>
#include <QTime>
#include <QFileInfo>

#include "avtransport.h"
#include "directory.h"
#include "devicemodel.h"
#include "utils.h"
#include "taskexecutor.h"
#include "contentserver.h"
#include "playlistmodel.h"


AVTransport::AVTransport(QObject *parent) :
    Service(parent),
    m_seekTimer(parent)
{
    connect(this, &AVTransport::transportStateChanged,
                     this, &AVTransport::transportStateHandler);
    connect(this, &AVTransport::currentURIChanged,
                     this, &AVTransport::trackChangedHandler);
    connect(this, &AVTransport::preControlableChanged,
                     this, &AVTransport::controlableChangedHandler);

    m_seekTimer.setInterval(500);
    m_seekTimer.setSingleShot(true);
    connect(&m_seekTimer, &QTimer::timeout, this, &AVTransport::seekTimeout);
}

void AVTransport::registerExternalConnections()
{
    connect(PlaylistModel::instance(), &PlaylistModel::activeItemChanged,
            this, &AVTransport::handleActiveItemChanged, Qt::QueuedConnection);
}

QUrl AVTransport::getCurrentId()
{
    auto cs = ContentServer::instance();
    return QUrl(cs->idFromUrl(m_currentURI));
}

QUrl AVTransport::getCurrentOrigUrl()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->origUrl() : QUrl(getCurrentURL());
}

void AVTransport::changed(const QString& name, const QVariant& _value)
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    if (_value.canConvert(QVariant::Int)) {
        int value = _value.toInt();

        if (name == "TransportState") {
            qDebug() << "TransportState changed:" << m_oldTransportState << m_transportState << value;
            if (m_transportState != value) {
                m_oldTransportState = m_transportState;
                m_transportState = value;
                emit transportStateChanged();
                emit preControlableChanged();
            }
        } else if (name == "TransportStatus") {
            if (m_transportStatus != value) {
                m_transportStatus = value;
                emit transportStatusChanged();
                emit preControlableChanged();
            }
        } else if (name == "NumberOfTracks") {
            if (m_numberOfTracks != value) {
                m_numberOfTracks = value;
                emit numberOfTracksChanged();
            }
        } else if (name == "CurrentTrack") {
            if (m_currentTrack != value) {
                m_currentTrack = value;
                emit currentTrackChanged();
            }
        } else if (name == "CurrentTrackDuration") {
            if (m_currentTrackDuration != value) {
                m_currentTrackDuration = value;
                emit currentTrackDurationChanged();
            }
        } else if (name == "RelativeTimePosition") {
            if (!m_seekTimer.isActive()) {
                if (m_relativeTimePosition != value) {
                    m_relativeTimePosition = value;
                    m_oldTimePosition = value;
                    emit relativeTimePositionChanged();
                }
            }
        } else if (name == "AbsoluteTimePosition") {
            if (m_absoluteTimePosition != value) {
                m_absoluteTimePosition = value;
                emit absoluteTimePositionChanged();
            }
        }
    }

    if (_value.canConvert(QVariant::String)) {
        QString value = _value.toString();

        if (name == "AVTransportURI" || name == "CurrentTrackURI") {
            if (m_currentURI != value) {

                if (m_blockEmitUriChanged) {
                    qDebug() << "currentURI change blocked";
                    return;
                }

                m_currentURI = value;

                if (m_emitCurrentUriChanged) {
                    qDebug() << "pending currentURI change exists";
                    return;
                }

                m_emitCurrentUriChanged = true;

                startTask([this]() {
                    tsleep();
                    if (m_emitCurrentUriChanged) {
                        m_emitCurrentUriChanged = false;
                        qDebug() << "emitting currentURI change";
                        emit currentURIChanged();
                        emit preControlableChanged();
                    }
                });
            }
        }

        if (name == "NextAVTransportURI") {
            if (value == "NOT_IMPLEMENTED") {
                setNextURISupported(false);
            } else if (m_nextURI != value) {
                setNextURISupported(true);

                if (m_blockEmitUriChanged) {
                    qDebug() << "nextURI change blocked";
                    return;
                }

                m_nextURI = value;

                if (m_emitNextUriChanged) {
                    qDebug() << "pending nextURI change exists";
                    return;
                }

                m_emitNextUriChanged = true;

                qDebug() << "nextURI change delayed";

                startTask([this]() {
                    tsleep();
                    if (m_emitNextUriChanged) {
                        m_emitNextUriChanged = false;
                        qDebug() << "emiting nextURIChanged";
                        emit nextURIChanged();
                        emit transportActionsChanged();
                        //emit preControlableChanged();
                    }
                });
            }
        }
    }
}

UPnPClient::Service* AVTransport::createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                                    const UPnPClient::UPnPServiceDesc &sdesc)
{
    return new UPnPClient::AVTransport(ddesc, sdesc);
}

void AVTransport::postInit()
{
    qDebug() << "--> UPDATE postInit";
    update();
}

void AVTransport::reset()
{
    qDebug() << "reset";

    m_currentURI.clear();
    m_nextURI.clear();
    m_transportState = Unknown;
    m_transportStatus = TPS_Unknown;
    m_numberOfTracks = 0;
    m_currentTrack = 0;
    m_currentTrackDuration = 0;
    m_relativeTimePosition  = 0;
    m_absoluteTimePosition = 0;
    m_speed = 1;
    m_currentClass.clear();
    m_currentTitle.clear();
    m_currentAuthor.clear();
    m_currentDescription.clear();
    m_currentAlbumArtURI.clear();
    m_currentAlbum.clear();
    m_currentTransportActions = 0;
    m_futureSeek = 0;
    m_nextURISupported = true;
    m_stopCalled = false;

    emit currentURIChanged();
    emit nextURIChanged();
    emit transportStateChanged();
    emit transportStatusChanged();
    emit currentTrackChanged();
    emit currentTrackDurationChanged();
    emit relativeTimePositionChanged();
    emit absoluteTimePositionChanged();
    emit speedChanged();
    emit currentMetaDataChanged();
    emit currentAlbumArtChanged();
    emit transportActionsChanged();
    emit preControlableChanged();
    emit nextURISupportedChanged();
}


std::string AVTransport::type() const
{
    return "urn:schemas-upnp-org:service:AVTransport:1";
}

void AVTransport::timerEvent()
{
    if (!m_seekTimer.isActive()) {
        fakeUpdateRelativeTimePosition();
    }
}

UPnPClient::AVTransport* AVTransport::s()
{
    if (!m_ser) {
        qWarning() << "AVTransport service is not inited";
        //emit error(E_NotInited);
        return nullptr;
    }

    return static_cast<UPnPClient::AVTransport*>(m_ser);
}

void AVTransport::transportStateHandler()
{
    emit transportActionsChanged();
    emit preControlableChanged();

    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    if (m_transportState == Stopped &&
        m_oldTransportState == Playing &&
        getCurrentType() != T_Image && !m_stopCalled) {
        qDebug() << "Track has ended";
        emit trackEnded();
    }

    m_stopCalled = false;

    //qDebug() << "--> aUPDATE transportStateHandler";
    asyncUpdate();
}

void AVTransport::handleActiveItemChanged()
{
    qDebug() << "Active item changed";
    emit currentMetaDataChanged();
    emit currentAlbumArtChanged();
}

void AVTransport::handleApplicationStateChanged(Qt::ApplicationState state)
{
    //qDebug() << "State changed:" << state;
    //qDebug() << "--> aUPDATE handleApplicationStateChanged";
    Q_UNUSED(state)

    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    asyncUpdate();
}

void AVTransport::trackChangedHandler()
{
    emit transportActionsChanged();
    //qDebug() << "--> aUPDATE trackChangedHandler";

    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    asyncUpdate();
}

void AVTransport::controlableChangedHandler()
{
    qDebug() << "controlableChangedHandler";

    if (m_pendingControlableSignal) {
        qDebug() << "Pending controlable signal exists";
        return;
    }

    qDebug() << "New pending controlable signal";
    m_pendingControlableSignal = true;

    if (!startTask([this]() {
        tsleep(2000);
        m_pendingControlableSignal = false;
        qDebug() << "Emit delayed controlable signal";
        emit controlableChanged();
    })) {
        m_pendingControlableSignal = false;
    }
}

int AVTransport::getTransportState()
{
    return m_transportState;
}

int AVTransport::getTransportStatus()
{
    return m_transportStatus;
}

int AVTransport::getPlayMode()
{
    return m_playmode;
}

int AVTransport::getNumberOfTracks()
{
    return m_numberOfTracks;
}

int AVTransport::getCurrentTrack()
{
    return m_currentTrack;
}

int AVTransport::getCurrentTrackDuration()
{
    return m_currentTrackDuration > 0 ? m_currentTrackDuration : 0;
}

int AVTransport::getRelativeTimePosition()
{
    return m_relativeTimePosition > 0 ? m_relativeTimePosition : 0;
}

int AVTransport::getSpeed()
{
    return m_speed;
}

int AVTransport::getAbsoluteTimePosition()
{
    if (m_absoluteTimePosition > m_currentTrackDuration) {
        qWarning() << "Track position is greater that currentTrackDuration";
        return m_currentTrackDuration;
    }
    return m_absoluteTimePosition;
}

QString AVTransport::getCurrentURI()
{
    return m_currentURI;
}

QString AVTransport::getNextURI()
{
    return m_nextURI;
}

QString AVTransport::getCurrentPath()
{
    auto cs = ContentServer::instance();
    return cs->pathFromUrl(m_currentURI);
}

QString AVTransport::getCurrentURL()
{
    auto cs = ContentServer::instance();
    return cs->urlFromUrl(m_currentURI);
}

QString AVTransport::getContentType()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->ctype() : QString();
}

bool AVTransport::getCurrentYtdl()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->ytdl() : false;
}

QString AVTransport::getNextPath()
{
    auto cs = ContentServer::instance();
    return cs->pathFromUrl(m_nextURI);
}

AVTransport::Type AVTransport::getCurrentType()
{
    // Reference: http://upnp.org/specs/av/UPnP-av-ContentDirectory-v1-Service.pdf

    const QStringList list = m_currentClass.split('.');

    if (list.size() > 2) {
        const QString c = list.at(2);
        if (c == "audioItem")
            return T_Audio;
        if (c == "videoItem")
            return T_Video;
        if (c == "imageItem")
            return T_Image;
    }

    qWarning() << "Unknown meta data class:" << m_currentClass;
    return T_Unknown;
}

QString AVTransport::getCurrentClass()
{
    return m_currentClass;
}

QString AVTransport::getCurrentTitle()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->name() : m_currentTitle;
}

QString AVTransport::getCurrentAuthor()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->artist() : m_currentAuthor;
}

QString AVTransport::getCurrentDescription()
{
    return m_currentDescription;
}

QUrl AVTransport::getCurrentAlbumArtURI()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->icon() : m_currentAlbumArtURI;
}

QString AVTransport::getCurrentAlbum()
{
    const auto ai = PlaylistModel::instance()->getActiveItem();
    return ai ? ai->album() : m_currentAlbum;
}

bool AVTransport::getNextSupported()
{
    if (isIgnoreActions()) {
        return !m_nextURI.isEmpty();
    } else {
        return (m_currentTransportActions & UPnPClient::AVTransport::TPA_Next) &&
                !m_nextURI.isEmpty();
    }
}

bool AVTransport::getPauseSupported()
{
    if (isIgnoreActions()) {
        return true;
    } else {
        return m_currentTransportActions & UPnPClient::AVTransport::TPA_Pause;
    }
}

bool AVTransport::getPlaySupported()
{
    if (isIgnoreActions()) {
        return !m_currentURI.isEmpty();
    } else {
        return (m_currentTransportActions & UPnPClient::AVTransport::TPA_Play) &&
                !m_currentURI.isEmpty();
    }
}

bool AVTransport::getPreviousSupported()
{
    if (isIgnoreActions()) {
        return true;
    } else {
        return m_currentTransportActions & UPnPClient::AVTransport::TPA_Previous;
    }
}

bool AVTransport::getSeekSupported()
{
    if (isIgnoreActions()) {
        return m_transportState == Playing ? m_currentTrackDuration > 0 : false;
    } else {
        if (m_transportState == Playing)
            return m_currentTrackDuration > 0 &&
                    (m_currentTransportActions & UPnPClient::AVTransport::TPA_Seek);
        else
            return false;
    }
}

bool AVTransport::getStopSupported()
{
    if (isIgnoreActions()) {
        return true;
    } else {
        return m_currentTransportActions & UPnPClient::AVTransport::TPA_Stop;
    }
}

bool AVTransport::getPlayable()
{
    return getInited() && m_transportStatus == TPS_Ok &&
           getPlaySupported() &&
           (m_transportState == Stopped ||
            m_transportState == PausedPlayback ||
            m_transportState == PausedRecording);
}

bool AVTransport::getStopable()
{
    return getInited() && m_transportStatus == TPS_Ok &&
           m_transportState == Playing &&
           !m_currentURI.isEmpty() &&
           (getPauseSupported() || getStopSupported());
}

bool AVTransport::getControlable()
{
    return getPlayable() || getStopable();
}

bool AVTransport::getNextURISupported()
{
    return m_nextURISupported;
}

void AVTransport::setNextURISupported(bool value)
{
    if (m_nextURISupported != value) {
        m_nextURISupported = value;
        emit nextURISupportedChanged();
    }
}

bool AVTransport::isIgnoreActions()
{
    // ignore supported actions when they don't make any sense
    // workaround for some creapy DLNA implementations
    bool ignore = false;
    if (m_transportState == Playing &&
            m_currentTransportActions & UPnPClient::AVTransport::TPA_Play &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Pause) &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Stop)) {
        ignore = true;
    } else if (m_transportState == Stopped &&
            m_currentTransportActions & UPnPClient::AVTransport::TPA_Stop &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Pause) &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Play)) {
        ignore = true;
    } else if ((m_transportState == PausedPlayback || m_transportState == PausedRecording) &&
            m_currentTransportActions & UPnPClient::AVTransport::TPA_Pause &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Stop) &&
            !(m_currentTransportActions & UPnPClient::AVTransport::TPA_Play)) {
        ignore = true;
    }

    if (ignore)
        qWarning() << "Ignoring supported actions";

    return ignore;
}

void AVTransport::setLocalContent(const QString &cid, const QString &nid)
{
    qDebug() << "setLocalContent:" << cid << nid;

    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    if (!Directory::instance()->isNetworkConnected()) {
        qWarning() << "Cannot find valid network interface";
        emit error(E_LostConnection);
        return;
    }

    //qDebug() << ">>> setLocalContent thread:" << QThread::currentThreadId();

    startTask([this, cid, nid](){
        auto cs = ContentServer::instance();

        bool do_current = !cid.isEmpty();
        bool do_play = false;
        bool do_next = m_nextURISupported && !nid.isEmpty();
        bool do_clearNext = m_nextURISupported && !do_next;
        QUrl cURL, nURL;
        QString cmeta, nmeta, s_cURI, s_nURI;

        //qDebug() << "2| do_current, do_play, do_next, do_clearNext:" << do_current << do_play << do_next << do_clearNext;
        //qDebug() << "2| cid, nid:" << cid << nid;

        if (do_current) {
            if (!cs->getContentUrl(cid, cURL, cmeta, m_currentURI)) {
                qWarning() << "Id" << cid << "cannot be converted to URL";
                emit error(E_InvalidPath);
                return;
            }

            s_cURI = cURL.toString();

            do_current = s_cURI != m_currentURI;
            if (!do_current) {
                qWarning() << "Content URL is the same as currentURI";
                if (m_transportState != Playing)
                    do_play = true;
            }
        }

        if (do_next) {
            if (!cs->getContentUrl(nid, nURL, nmeta, m_nextURI)) {
                qWarning() << "Id" << nid << "cannot be converted to URL";
                emit error(E_InvalidPath);
                return;
            }

            s_nURI = nURL.toString();

            do_next = s_nURI != m_nextURI;
            if (!do_next) {
                qWarning() << "Content URL is the same as nextURI";
            } /*else {
                // Don't update nextURI if the same as currentURI
                do_next = s_nURI != m_currentURI;
                if (!do_next)
                    qWarning() << "Next content URL is the same as currentURI";
            }*/
        }

        if (do_next || do_current || do_play || do_clearNext) {
            blockUriChanged(1500);
        } else {
            qWarning() << "Nothing to update";
            return;
        }

        m_updateMutex.lock();

        auto srv = s();

        qDebug() << "cmeta:" << cmeta;
        qDebug() << "s_cURI" << s_cURI;

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        if (m_transportState != Playing) {
            if (!handleError(srv->stop())) {
                qWarning() << "Error response for stop()";
                if (!getInited()) {
                    m_updateMutex.unlock();
                    qWarning() << "AVTransport service is not inited";
                    return;
                }
            }
            tsleep();
        }

        if (do_next) {
            qDebug() << "Calling setNextAVTransportURI with id:" << nid;
            if (!handleError(srv->setNextAVTransportURI(s_nURI.toStdString(), nmeta.toStdString()))) {
                qWarning() << "Error response for setNextAVTransportURI()";
                if (!getInited()) {
                    m_updateMutex.unlock();
                    qWarning() << "AVTransport service is not inited";
                    return;
                }
            }
        }

        if (do_clearNext) {
            if (!m_nextURI.isEmpty()) {
                qWarning() << "Clearing nextURI";
                if (!handleError(srv->setNextAVTransportURI("", ""))) {
                    qWarning() << "Error response for setNextAVTransportURI()";
                    if (!getInited()) {
                        m_updateMutex.unlock();
                        qWarning() << "AVTransport service is not inited";
                        return;
                    }
                }
            }
        }

        if (do_current) {
            // Clearing nextURI on next action on last item in the queue
            if (m_nextURISupported && nid.isEmpty() && m_nextURI == s_cURI) {
                qDebug() << "Clearing nextURI";
                if (!handleError(srv->setNextAVTransportURI("", ""))) {
                    qWarning() << "Error response for setNextAVTransportURI()";
                    if (!getInited()) {
                        m_updateMutex.unlock();
                        qWarning() << "AVTransport service is not inited";
                        return;
                    }
                }
            }

            qDebug() << "Calling setAVTransportURI with id:" << cid;
            if (!handleError(srv->setAVTransportURI(s_cURI.toStdString(),cmeta.toStdString()))) {
                qWarning() << "Error response for setAVTransportURI()";
                if (!getInited()) {
                    m_updateMutex.unlock();
                    qWarning() << "AVTransport service is not inited";
                    return;
                }
            }
            tsleep();
            do_play = true;
        }

        if (do_play) {
            if (!handleError(srv->play())) {
                qWarning() << "Error response for play()";
                if (!getInited()) {
                    m_updateMutex.unlock();
                    qWarning() << "AVTransport service is not inited";
                    return;
                }
            }
        }

        tsleep();

        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE setLocalContent";

        update();
    });
}

void AVTransport::setSpeed(int value)
{
    if (m_speed != value) {
        m_speed = value;
        emit speedChanged();
    }
}

void AVTransport::play()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        m_updateMutex.lock();    

        qDebug() << "Calling: play";

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        if (!handleError(srv->play(m_speed))) {
            qWarning() << "Error response for play()";
            if (!getInited()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE play";
        update();
    });
}

void AVTransport::pause()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    if (m_currentTrackDuration <= 0) {
        qDebug() << "Possibly live streaming content, so stopping instead pausing";
        stop();
        return;
    }

    startTask([this](){
        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: pause";

        if (!handleError(srv->pause())) {
            qWarning() << "Error response for pause()";
            if (!getInited()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE pause";

        update();
    });
}

void AVTransport::stop()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    m_relativeTimePosition = 0;
    m_absoluteTimePosition = 0;
    emit relativeTimePositionChanged();
    emit absoluteTimePositionChanged();

    startTask([this](){
        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: stop";

        m_stopCalled = true;
        if (!handleError(srv->stop())) {
            qWarning() << "Error response for stop()";
            m_stopCalled = false;
            if (!getInited()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE stop";
        update();
    });
}

void AVTransport::next()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: next";

        if (!handleError(srv->next())) {
            qWarning() << "Error response for next()";
            if (!getInited()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE next";
        update();
    });
}

void AVTransport::previous()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: previous";

        if (!handleError(srv->previous())) {
            qWarning() << "Error response for previous()";
            if (!getInited()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }

        tsleep();
        m_updateMutex.unlock();

        //qDebug() << "--> UPDATE previous";
        update();
    });
}

void AVTransport::setPlayMode(int value)
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this, value](){
        qDebug() << "setPlayMode:" << value;

        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: setPlayMode";

        auto pm = static_cast<UPnPClient::AVTransport::PlayMode>(value);

        if (handleError(srv->setPlayMode(pm))) {
            m_updateMutex.unlock();
        } else {
            qWarning() << "Error response for setPlayMode(" << value << ")";
            m_updateMutex.unlock();
        }
    });
}

void AVTransport::seek(int value)
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    //qDebug() << "Seek:" << value;

    m_futureSeek = value < 0 ? 0 :
                   value > m_currentTrackDuration ? m_currentTrackDuration : value;
#ifdef SAILFISH
    m_seekTimer.start();
#else
    seekTimeout();
#endif
}

void AVTransport::seekTimeout()
{
    if (!getInited()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        qDebug() << "Seek timeout:" << m_futureSeek;

        m_updateMutex.lock();

        auto srv = s();

        if (!getInited() || !srv) {
            m_updateMutex.unlock();
            qWarning() << "AVTransport service is not inited";
            return;
        }

        qDebug() << "Calling: seek";

        if (handleError(srv->seek(UPnPClient::AVTransport::SEEK_REL_TIME,
                                  m_futureSeek))) {
            m_relativeTimePosition = m_futureSeek;
            m_oldTimePosition = m_futureSeek - 5;
            tsleep();
            m_updateMutex.unlock();

            //qDebug() << "--> UPDATE seekTimeout";
            update();
        } else {
            qWarning() << "Error response for seek(" << m_futureSeek << ")";
            m_updateMutex.unlock();
        }
    });
}

void AVTransport::fakeUpdateRelativeTimePosition()
{
    if (m_currentTrackDuration == 0 || m_relativeTimePosition < m_currentTrackDuration) {
        if (m_oldTimePosition < m_relativeTimePosition - 5) {
            asyncUpdatePositionInfo();
        } else {
            m_relativeTimePosition++;
            emit relativeTimePositionChanged();
        }
    } else {
        qDebug() << "--> aUPDATE fakeUpdateRelativeTimePosition";
        asyncUpdate(1000);
    }
}

bool AVTransport::updating()
{
    bool lock = m_updateMutex.tryLock();
    if (lock) {
        m_updateMutex.unlock();
        return false;
    }
    return true;
}

void AVTransport::update(int initDelay, int postDelay)
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    qDebug() << "Update start";

    if (!m_updateMutex.tryLock()) {
        qDebug() << "Update is locked";
        return;
    }

    tsleep(initDelay);

    updatePositionInfo();
    updateTransportInfo();
    updateMediaInfo();
    updateCurrentTransportActions();
    //updateTransportSettings();

    tsleep(postDelay);

    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        m_updateMutex.unlock();
        qWarning() << "AVTransport service is not inited";
        return;
    }

    if (getCurrentType() == T_Image && m_transportState == Playing) {
        qDebug() << "Calling stop because current track type is image";
        if (!handleError(srv->stop())) {
            qWarning() << "Error response for stop()";
            if (!isInitedOrIniting()) {
                m_updateMutex.unlock();
                qWarning() << "AVTransport service is not inited";
                return;
            }
        }
        tsleep();
    }

    qDebug() << "Update end";

    emit updated();

    needTimerCheck();

    m_updateMutex.unlock();
}

void AVTransport::needTimerCheck()
{
    auto app = static_cast<QGuiApplication*>(QGuiApplication::instance());
    //qDebug() << "applicationState:" << app->applicationState();

    if (m_transportState == Playing
#ifdef SAILFISH
           && app->applicationState() == Qt::ApplicationActive
#endif
       ) {
       emit needTimer(true);
    } else {
       emit needTimer(false);
    }
}

void AVTransport::asyncUpdate(int initDelay, int postDelay)
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this, initDelay, postDelay](){
        update(initDelay, postDelay);
    });
}

void AVTransport::asyncUpdatePositionInfo()
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        updatePositionInfo();
        updateCurrentTransportActions();
    });
}

void AVTransport::updatePositionInfo()
{
    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    UPnPClient::AVTransport::PositionInfo pi;

    if (!handleError(srv->getPositionInfo(pi))) {
        qWarning() << "Unable to get Position Info";
        pi.abscount = 0;
        pi.abstime = 0;
        pi.relcount = 0;
        pi.reltime = 0;
        pi.track = 0;
        pi.trackduration = 0;

    }

    qDebug() << "PositionInfo:";
    qDebug() << "  track:" << pi.track;
    qDebug() << "  trackduration:" << pi.trackduration;
    qDebug() << "  reltime:" << pi.reltime;
    qDebug() << "  abstime:" << pi.abstime;
    qDebug() << "  relcount:" << pi.relcount;
    qDebug() << "  abscount:" << pi.abscount;
    qDebug() << "  trackuri:" << QString::fromStdString(pi.trackuri);
    qDebug() << "  trackmeta id:" << QString::fromStdString(pi.trackmeta.m_id);

    if (m_currentTrack != pi.track) {
        m_currentTrack = pi.track;
        emit currentTrackChanged();
    }

    if (m_absoluteTimePosition != pi.abstime) {
        m_absoluteTimePosition = pi.abstime;
        emit absoluteTimePositionChanged();
    }

    if (m_currentTrackDuration != pi.trackduration) {
        m_currentTrackDuration = pi.trackduration;
        emit currentTrackDurationChanged();
    }

    if (m_relativeTimePosition != pi.reltime) {
        m_relativeTimePosition = pi.reltime;
        m_oldTimePosition = m_relativeTimePosition;
        emit relativeTimePositionChanged();
    }
}

void AVTransport::asyncUpdateTransportInfo()
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        updateTransportInfo();
    });
}

void AVTransport::updateTransportInfo()
{
    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    UPnPClient::AVTransport::TransportInfo ti;
    if (handleError(srv->getTransportInfo(ti))) {
        qDebug() << "TransportInfo:";
        qDebug() << "  tpstate:" << ti.tpstate;
        qDebug() << "  tpstatus:" << ti.tpstatus;

        if (m_transportState != ti.tpstate) {
            m_oldTransportState = m_transportState;
            m_transportState = ti.tpstate;
            emit transportStateChanged();
            emit transportActionsChanged();
            emit preControlableChanged();
        }

        if (m_transportStatus != ti.tpstatus) {
            m_transportStatus = ti.tpstatus;
            emit transportStatusChanged();
            emit preControlableChanged();
        }
    } else {
        qWarning() << "Unable to get Transport Info";

        if (m_transportState != Unknown) {
            m_oldTransportState = m_transportState;
            m_transportState = Unknown;
            emit transportStateChanged();
            emit transportActionsChanged();
            emit preControlableChanged();
        }

        if (m_transportStatus != TPS_Unknown) {
            m_transportStatus = TPS_Unknown;
            emit transportStatusChanged();
            emit preControlableChanged();
        }
    }
}

void AVTransport::updateTrackMeta(const UPnPClient::UPnPDirObject &trackmeta)
{
    m_id = QString::fromStdString(trackmeta.m_id);
    m_currentTitle = QString::fromStdString(trackmeta.m_title);
    m_currentClass = QString::fromStdString(trackmeta.getprop("upnp:class"));
    m_currentAuthor = QString::fromStdString(trackmeta.getprop("upnp:artist")).split(",").first();
    m_currentDescription  = QString::fromStdString(trackmeta.getprop("upnp:longDescription"));
    m_currentAlbum = QString::fromStdString(trackmeta.getprop("upnp:album"));
    emit currentMetaDataChanged();

    auto new_albumArtURI = QUrl(QString::fromStdString(trackmeta.getprop("upnp:albumArtURI")));
    if (m_currentAlbumArtURI != new_albumArtURI) {
        m_currentAlbumArtURI = new_albumArtURI;
        emit currentAlbumArtChanged();
    }

    // -- debug --

    qDebug() << "Current meta:";
    qDebug() << "  id:" << m_id;
    qDebug() << "  pid:" << QString::fromStdString(trackmeta.m_pid);
    qDebug() << "  title:" << QString::fromStdString(trackmeta.m_title);
    qDebug() << "  m_type:" << (trackmeta.m_type == UPnPClient::UPnPDirObject::item ? "Item" :
                                trackmeta.m_type == UPnPClient::UPnPDirObject::container ? "Container" :
                                                                                           "Unknown");
    qDebug() << "  m_iclass:" << (trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem ? "AudioItem" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem_musicTrack ? "AudioItem MusicTrack" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_audioItem_playlist ? "AudioItem Playlist" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_playlist ? "Playlist" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_videoItem ? "VideoItem" :
                                  trackmeta.m_iclass == UPnPClient::UPnPDirObject::ITC_unknown ? "Unknown" : "Unknown other");
    qDebug() << "  props:";
    for (auto const& x : trackmeta.m_props) {
        qDebug() << "   " << QString::fromStdString(x.first) << " : " << QString::fromStdString(x.second);
    }
    qDebug() << "  resources:";
    for (auto const& x : trackmeta.m_resources) {
        qDebug() << "   " << QString::fromStdString(x.m_uri);
    }

    // ---
}

void AVTransport::asyncUpdateMediaInfo()
{
    if (!isInitedOrIniting()) {
        qWarning() << "Service is not inited";
        return;
    }

    startTask([this](){
        updateMediaInfo();
    });
}

void AVTransport::updateMediaInfo()
{
    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    UPnPClient::AVTransport::MediaInfo mi;
    if (handleError(srv->getMediaInfo(mi))) {
        qDebug() << "MediaInfo:";
        qDebug() << "  nrtracks:" << mi.nrtracks;
        qDebug() << "  mduration:" << mi.mduration;
        qDebug() << "  cururi:" << QString::fromStdString(mi.cururi);
        qDebug() << "  nexturi:" << QString::fromStdString(mi.nexturi);
        qDebug() << "  pbstoragemed:" << QString::fromStdString(mi.pbstoragemed);
        qDebug() << "  rcstoragemed:" << QString::fromStdString(mi.rcstoragemed);
        qDebug() << "  ws:" << QString::fromStdString(mi.ws);

        if (m_numberOfTracks != mi.nrtracks) {
            m_numberOfTracks = mi.nrtracks;
            emit numberOfTracksChanged();
        }

        QString cururi = QString::fromStdString(mi.cururi);
        qDebug() << "old currentURI:" << m_currentURI;
        qDebug() << "new currentURI:" << cururi;

        if (m_currentURI != cururi) {
            m_currentURI = cururi;
            emit currentURIChanged();
            emit preControlableChanged();
        }

        QString nexturi = QString::fromStdString(mi.nexturi);
        qDebug() << "old nextURI:" << m_nextURI;
        qDebug() << "new nextURI:" << nexturi;

        if (nexturi == "NOT_IMPLEMENTED") {
            qWarning() << "nextURI is not supported";
            setNextURISupported(false);
        } else if (m_nextURI != nexturi) {
            setNextURISupported(true);
            if (m_nextURI.isEmpty()) {
                m_nextURI = nexturi;
                emit nextURIChanged();
                emit transportActionsChanged();
            } else {
                m_nextURI = nexturi;
                emit nextURIChanged();
            }
        }

        updateTrackMeta(mi.curmeta);

    } else {
        qWarning() << "Unable to get Media Info";

        if (m_numberOfTracks != 0) {
            m_numberOfTracks = 0;
            emit numberOfTracksChanged();
        }

        if (!m_currentURI.isEmpty() || !m_nextURI.isEmpty()) {
            m_currentURI.clear();
            m_nextURI.clear();
            emit currentURIChanged();
            emit nextURIChanged();
            emit preControlableChanged();
            emit transportActionsChanged();
        }

        m_id.clear();
        m_currentTitle.clear();
        m_currentClass.clear();
        m_currentAuthor.clear();
        m_currentDescription.clear();
        m_currentAlbum.clear();
        emit currentMetaDataChanged();

        if (!m_currentAlbumArtURI.isEmpty()) {
            m_currentAlbumArtURI.clear();
            emit currentAlbumArtChanged();
        }
    }
}

void AVTransport::asyncUpdateCurrentTransportActions()
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        updateCurrentTransportActions();
    });
}

void AVTransport::updateCurrentTransportActions()
{
    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    int ac = 0;
    if (handleError(srv->getCurrentTransportActions(ac))) {
        qDebug() << "CurrentTransportActions:";
        qDebug() << "  actions:" << ac;
        qDebug() << "  Next:" << ((ac & UPnPClient::AVTransport::TPA_Next) > 0);
        qDebug() << "  Pause:" << ((ac & UPnPClient::AVTransport::TPA_Pause) > 0);
        qDebug() << "  Play:" << ((ac & UPnPClient::AVTransport::TPA_Play) > 0);
        qDebug() << "  Previous:" << ((ac & UPnPClient::AVTransport::TPA_Previous) > 0);
        qDebug() << "  Seek:" << ((ac & UPnPClient::AVTransport::TPA_Seek) > 0);
        qDebug() << "  Stop:" << ((ac & UPnPClient::AVTransport::TPA_Stop) > 0);
    } else {
        qWarning() << "Unable to get Transport Actions";
    }

    if (m_currentTransportActions != ac) {
        m_currentTransportActions = ac;
        emit transportActionsChanged();
        emit preControlableChanged();
    }
}

void AVTransport::updateTransportSettings()
{
    auto srv = s();

    if (!isInitedOrIniting() || !srv) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    UPnPClient::AVTransport::TransportSettings ts;

    if (handleError(srv->getTransportSettings(ts))) {
        qDebug() << "TransportSettings:";
        qDebug() << "  playmode:" << ts.playmode;
        qDebug() << "  recqualitymode:" << QString::fromStdString(ts.recqualitymode);
    } else {
        qWarning() << "Unable to get Transport Settings";
    }

    if (m_playmode != ts.playmode) {
        m_playmode = ts.playmode;
        emit playModeChanged();
    }
}

void AVTransport::asyncUpdateTransportSettings()
{
    if (!isInitedOrIniting()) {
        qWarning() << "AVTransport service is not inited";
        return;
    }

    startTask([this](){
        updateTransportSettings();
    });
}

void AVTransport::blockUriChanged(int time)
{
    m_blockEmitUriChanged = true;

    qDebug() << "URIChanged blocked";

    startTask([this, time]() {
        tsleep(time);
        m_blockEmitUriChanged = false;
        qDebug() << "URIChanged unblocked";
    });
}
