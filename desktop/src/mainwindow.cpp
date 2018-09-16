/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include <QApplication>
#include <QDebug>
#include <QListView>
#include <QFileDialog>
#include <QStringList>
#include <QMetaObject>
#include <QStandardPaths>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QPixmap>
#include <QLatin1String>
#include <QMessageBox>
#include <QInputDialog>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "directory.h"
#include "busyindicator.h"
#include "services.h"
#include "avtransport.h"
#include "renderingcontrol.h"
#include "utils.h"
#include "contentserver.h"
#include "settings.h"
#include "info.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QMetaObject::connectSlotsByName(this);
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationDisplayName());
    ui->actionAbout->setText(tr("About %1").arg(QApplication::applicationDisplayName()));
    settingsDialog = std::unique_ptr<SettingsDialog>(new SettingsDialog(this));

    enablePlaylist(false);

    auto directory = Directory::instance();
    auto services = Services::instance();
    auto rc = services->renderingControl;
    auto av = services->avTransport;

    deviceModel = std::unique_ptr<DeviceModel>(new DeviceModel(this));
    ui->deviceList->setModel(deviceModel.get());

    on_directory_initedChanged();
    on_directory_busyChanged();
    on_av_stateChanged();
    on_av_currentMetaDataChanged();
    on_av_positionChanged();
    on_av_albumArtChanged();
    on_av_transportActionsChanged();
    on_rc_muteChanged();
    on_rc_volumeChanged();

    connect(directory, &Directory::busyChanged,
            this, &MainWindow::on_directory_busyChanged);
    connect(directory, &Directory::initedChanged,
            this, &MainWindow::on_directory_initedChanged);
    connect(rc.get(), &RenderingControl::initedChanged,
            this, &MainWindow::on_service_initedChanged);
    connect(av.get(), &AVTransport::initedChanged,
            this, &MainWindow::on_service_initedChanged);
    connect(rc.get(), &RenderingControl::busyChanged,
            this, &MainWindow::on_service_initedChanged);
    connect(av.get(), &AVTransport::busyChanged,
            this, &MainWindow::on_service_initedChanged);
    connect(av.get(), &AVTransport::currentMetaDataChanged,
            this, &MainWindow::on_av_currentMetaDataChanged);
    connect(av.get(), &AVTransport::relativeTimePositionChanged,
            this, &MainWindow::on_av_positionChanged);
    connect(av.get(), &AVTransport::currentAlbumArtChanged,
            this, &MainWindow::on_av_albumArtChanged);
    connect(av.get(), &AVTransport::transportStateChanged,
            this, &MainWindow::on_av_stateChanged);
    connect(av.get(), &AVTransport::controlableChanged,
            this, &MainWindow::on_av_stateChanged);
    connect(av.get(), &AVTransport::currentURIChanged,
            this, &MainWindow::on_av_currentURIChanged);
    connect(av.get(), &AVTransport::nextURIChanged,
            this, &MainWindow::on_av_nextURIChanged);
    connect(av.get(), &AVTransport::trackEnded,
            this, &MainWindow::on_av_trackEnded);
    connect(av.get(), &AVTransport::updated,
            this, &MainWindow::on_av_updated);
    connect(av.get(), &AVTransport::transportActionsChanged,
            this, &MainWindow::on_av_transportActionsChanged);
    connect(rc.get(), &RenderingControl::volumeChanged,
            this, &MainWindow::on_rc_volumeChanged);
    connect(rc.get(), &RenderingControl::muteChanged,
            this, &MainWindow::on_rc_muteChanged);

    directory->discover();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::enablePlaylist(bool enabled)
{
    ui->playlistPlaceholder->setVisible(!enabled);
    ui->playlist->setVisible(enabled);
}

void MainWindow::notify(const QString& message)
{
    if (message.isEmpty())
        ui->statusBar->clearMessage();
    else
        ui->statusBar->showMessage(message);
}

void MainWindow::togglePlay()
{
    auto av = Services::instance()->avTransport;

    bool playing = av->getTransportState() == AVTransport::Playing;

    if (!playing) {
        av->setSpeed(1);
        av->play();
    } else {
        if (av->getPauseSupported())
            av->pause();
        else
            av->stop();
    }
}

void MainWindow::on_playmodeButton_clicked()
{
    if (playlistModel)
        playlistModel->togglePlayMode();
}

void MainWindow::on_av_transportActionsChanged()
{
    auto av = Services::instance()->avTransport;

    //bool play = av->getPauseSupported();
    bool pause = av->getPauseSupported();
    bool stop = av->getStopSupported();
    bool next = av->getNextSupported();
    bool seek = av->getSeekSupported();
    bool pos = av->getRelativeTimePosition() > 5;
    bool playing = av->getTransportState() == AVTransport::Playing;
    bool pmro = playlistModel && playlistModel->getPlayMode() == PlayListModel::PM_RepeatOne;
    int count = playlistModel ? playlistModel->rowCount() : 0;
    int index = playlistModel ? playlistModel->getActiveItemIndex() : -1;

    ui->playButton->setEnabled(pause || stop);
    ui->nextButton->setEnabled(next && count > 0 && !pmro);
    ui->prevButton->setEnabled((seek && playing && pos) || (index > -1 && !pmro));
    ui->progressSlider->setEnabled(seek);
}

void MainWindow::on_rc_muteChanged()
{
    auto rc = Services::instance()->renderingControl;
    bool mute = rc->getMute();
    ui->muteButton->setChecked(mute);
    ui->volumeSlider->setEnabled(!mute);
}

void MainWindow::on_rc_volumeChanged()
{
    auto rc = Services::instance()->renderingControl;
    int pos = rc->getVolume();
    ui->volumeSlider->setValue(pos);
    ui->volumeLabel->setText(QString::number(pos));
}

void MainWindow::on_connectButton_clicked()
{
    auto directory = Directory::instance();
    deviceModel->clear();

    if (directory->getInited())
        directory->discover();
    else
        directory->init();
}

void MainWindow::on_directory_busyChanged()
{
    bool busy = Directory::instance()->getBusy();
    ui->connectButton->setEnabled(!busy);

    if (busy) {
        notify(tr("Finding devices..."));
    } else {
        int count = deviceModel->rowCount();
        notify(tr("Found %n device(s)", "", count));
    }
}

void MainWindow::on_directory_initedChanged()
{
    bool inited = Directory::instance()->getInited();
    ui->connectButton->setText(inited ? tr("Find devices") : tr("Connect"));
}

void MainWindow::on_deviceList_activated(const QModelIndex &index)
{
    auto item = dynamic_cast<DeviceItem*>(deviceModel->readRow(index.row()));
    auto av = Services::instance()->avTransport;

    if (av && av->getInited() && av->getDeviceId() == item->id()) {
        qWarning() << "The same device is already inited";
    } else if (item->supported()) {
        auto services = Services::instance();
        auto rc = services->renderingControl;
        auto av = services->avTransport;
        auto s = Settings::instance();
        QString id = item->id();

        rc->init(id);
        av->init(id);

        playlistModel = std::unique_ptr<PlayListModel>(new PlayListModel(this));
        connect(playlistModel.get(), &PlayListModel::playModeChanged,
                this, &MainWindow::on_playlistModel_playModeChanged);
        on_playlistModel_playModeChanged();
        ui->playlistView->setModel(playlistModel.get());
        if (s->getRememberPlaylist())
            playlistModel->load();

        deviceModel->setActiveIndex(index.row());
        enablePlaylist(true);
    } else {
        qWarning() << "Device not supported";
    }
}

void MainWindow::on_av_stateChanged()
{
    auto av = Services::instance()->avTransport;

    bool controlable = av->getControlable();
    bool playing = av->getTransportState() == AVTransport::Playing;
    bool busy = av->getBusy();
    bool image = av->getCurrentType() == AVTransport::T_Image;
    bool inited = av->getInited();

    ui->player->setEnabled(!busy && inited);
    ui->playButton->setText(playing ? tr("Pause") : tr("Play"));
    ui->playButton->setEnabled(controlable && !image);

    //ui->addUrlButton->setEnabled(playlistModel && inited);
    ui->addFilesButton->setEnabled(playlistModel && inited);
    ui->clearButton->setEnabled(playlistModel && inited);

    if (playlistModel)
        updatePlaylist(false);
}

void MainWindow::on_service_initedChanged()
{
    auto services = Services::instance();
    auto rc = services->renderingControl;
    auto av = services->avTransport;

    bool inited = rc->getInited() && av->getInited();
    bool busy = rc->getBusy() || av->getBusy();

    if (!inited && !busy) {
        enablePlaylist(false);
        notify();
        //deviceModel->updateModel();
        //auto directory = Directory::instance();
        //directory->discover();
    } else if (!inited && busy) {
        notify(tr("Connecting..."));
    } else if (inited && !busy) {
        notify(tr("Connected to %1").arg(av->getDeviceFriendlyName()));
    }
}

void MainWindow::on_av_currentMetaDataChanged()
{
    auto av = Services::instance()->avTransport;

    bool controlable = av->getControlable();
    bool image = av->getCurrentType() == AVTransport::T_Image;
    auto title = av->getCurrentTitle();
    auto author = av->getCurrentAuthor();

    ui->trackTitle->setText(title.isEmpty() ? tr("Unknown") : title);
    ui->artistName->setText(author.isEmpty() ? tr("Unknown") : author);
    ui->playButton->setEnabled(controlable && !image);
}

void MainWindow::on_av_positionChanged()
{
    if (!ui->progressSlider->isSliderDown()) {
        auto av = Services::instance()->avTransport;

        int pos = av->getRelativeTimePosition();
        int max = av->getCurrentTrackDuration();

        ui->progressSlider->setMaximum(max);
        ui->progressSlider->setMinimum(0);
        ui->progressSlider->setValue(pos);

        auto utils = Utils::instance();
        ui->progressLabel->setText(utils->secToStr(pos) + "/" + utils->secToStr(max));
    } else {
        qDebug() << "Slider is down, so ignoring position change event";
    }
}

void MainWindow::on_av_albumArtChanged()
{
    auto av = Services::instance()->avTransport;

    auto album = av->getCurrentAlbumArtURI();

    if (album.isEmpty()) {
        qWarning() << "Album art is empty";
        return;
    }

    downloader = std::unique_ptr<FileDownloader>(new FileDownloader(QUrl(album), this));
    connect(downloader.get(), &FileDownloader::downloaded, [this](int error){
        if (error == 0) {
            QImage img = QImage::fromData(downloader->downloadedData());
            img = img.scaled(ui->albumImage->minimumSize());
            QPixmap pix = QPixmap::fromImage(img);
            ui->albumImage->setPixmap(pix);
        } else {
            qWarning() << "Error occured during image download";
            ui->albumImage->setPixmap(QPixmap());
        }
    });
}

void MainWindow::on_addFilesButton_clicked()
{
    auto cserver = ContentServer::instance();

    auto filters = QString("Audio files (%1);;Video files (%2)")
            .arg(cserver->getExtensions(ContentServer::TypeMusic).join(" "),
                 cserver->getExtensions(ContentServer::TypeVideo).join(" "));

    if (Settings::instance()->getImageSupported())
        filters += QString(";;Images (%3)")
                .arg(cserver->getExtensions(ContentServer::TypeImage).join(" "));

    auto paths = QFileDialog::getOpenFileNames(this,
        tr("Select one or more files"),
        QStandardPaths::standardLocations(QStandardPaths::HomeLocation).last(),
        filters);

    if (paths.isEmpty())
        return;

    playlistModel->addItemPaths(paths);
}

void MainWindow::on_addUrlButton_clicked()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Add URL"),
                                         tr("URL:"), QLineEdit::Normal,
                                         QString(), &ok,
                                         Qt::WindowFlags(),
                                         Qt::ImhUrlCharactersOnly);
    if (ok && !text.isEmpty()) {
        qDebug() << "Url entered:" << text;
        QUrl url(text);
        if (url.isValid())
            playlistModel->addItemUrl(url);
        else
            qWarning() << "Url is invalid";
    }

    // "http://ice1.somafm.com/groovesalad-128-mp3"
}

void MainWindow::on_playButton_clicked()
{
    togglePlay();
}

void MainWindow::on_nextButton_clicked()
{
    if (playlistModel) {
        auto av = Services::instance()->avTransport;
        int count = playlistModel->rowCount();

        if (count > 0) {
            QString fid = playlistModel->firstId();
            QString aid = playlistModel->activeId();
            QString nid = playlistModel->nextActiveId();
            if (aid.isEmpty()) {
                playlistModel->setToBeActiveId(fid);
                av->setLocalContent(fid, nid);
            } else {
                playlistModel->setToBeActiveId(nid);
                av->setLocalContent(nid, "");
            }
        } else {
            qWarning() << "Playlist is empty so can't do next()";
        }
    }
}

void MainWindow::on_prevButton_clicked()
{
    if (playlistModel) {
        auto av = Services::instance()->avTransport;
        int count = playlistModel ? playlistModel->rowCount() : 0;
        bool seekable = av->getSeekSupported();

        if (count > 0) {
            QString pid = playlistModel->prevActiveId();
            QString aid = playlistModel->activeId();

            if (aid.isEmpty()) {
                if (seekable)
                    av->seek(0);
                return;
            }

            if (pid.isEmpty()) {
                if (seekable)
                    av->seek(0);
                return;
            }

            if (seekable && av->getRelativeTimePosition() > 5) {
                av->seek(0);
            } else {
                playlistModel->setToBeActiveId(pid);
                av->setLocalContent(pid, aid);
            }
        } else {
            if (seekable)
                av->seek(0);
        }
    }
}

void MainWindow::on_muteButton_toggled(bool checked)
{
    auto rc = Services::instance()->renderingControl;
    rc->setMute(checked);
}

void MainWindow::on_av_currentURIChanged()
{
    auto av = Services::instance()->avTransport;
    qDebug() << "on_av_currentURIChanged:" << av->getCurrentURI();

    if (playlistModel) {
        playlistModel->setActiveUrl(av->getCurrentURI());
        bool play = av->getTransportState() != AVTransport::Playing &&
                playlistModel->rowCount() == 1;
        updatePlaylist(play);
    }
}

void MainWindow::on_av_nextURIChanged()
{
    auto av = Services::instance()->avTransport;
    qDebug() << "on_av_nextURIChanged:" << av->getNextURI();

    if (playlistModel) {
        auto nextURI = av->getNextURI();
        auto currentURI = av->getCurrentURI();
        bool normalPlayMode = playlistModel->getPlayMode() == PlayListModel::PM_Normal;

        if (nextURI.isEmpty() && !currentURI.isEmpty() && normalPlayMode) {
            qDebug() << "AVT switches to nextURI without currentURIChanged";
            playlistModel->setActiveUrl(currentURI);

            bool play = av->getTransportState() != AVTransport::Playing &&
                    playlistModel->rowCount() == 1;
            updatePlaylist(play);
        }
    }
}

void MainWindow::on_av_trackEnded()
{
}

void MainWindow::on_av_updated()
{
    auto rc = Services::instance()->renderingControl;
    if (rc)
        rc->asyncUpdate();
}

void MainWindow::updatePlaylist(bool play)
{
    int count = playlistModel->rowCount();

    if (count > 0) {
        auto av = Services::instance()->avTransport;
        auto aid = playlistModel->activeId();
        if (aid.isEmpty()) {
            auto fid = playlistModel->firstId();
            if (play) {
                auto sid = playlistModel->secondId();
                playlistModel->setToBeActiveId(fid);
                av->setLocalContent(fid, sid);
            } else {
                if (!fid.isEmpty())
                    av->setLocalContent("", fid);
            }
        } else {
            auto nid = playlistModel->nextActiveId();
            av->setLocalContent("", nid);
        }
    }
}

void MainWindow::on_playlistModel_playModeChanged()
{
    if (playlistModel) {
        switch(playlistModel->getPlayMode()) {
        case PlayListModel::PM_Normal:
            ui->playmodeButton->setText(tr("Normal"));
            break;
        case PlayListModel::PM_RepeatAll:
            ui->playmodeButton->setText(tr("Repeat all"));
            break;
        case PlayListModel::PM_RepeatOne:
            ui->playmodeButton->setText(tr("Repeat one"));
            break;
        default:
            ui->playmodeButton->setText(tr("Normal"));
        }

        updatePlaylist(false);
    }
}

void MainWindow::on_volumeSlider_sliderMoved(int position)
{
    ui->volumeLabel->setText(QString::number(position));
}

void MainWindow::on_progressSlider_sliderMoved(int position)
{
    auto av = Services::instance()->avTransport;
    auto utils = Utils::instance();
    int max = av->getCurrentTrackDuration();
    ui->progressLabel->setText(utils->secToStr(position) + "/" + utils->secToStr(max));
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_actionAbout_triggered()
{
    QString name = QApplication::applicationDisplayName();
    QString version = QApplication::applicationVersion();

    QString translatedTextAboutCaption = tr(
        "<h3>%1</h3>"
        "<p>Version: %2</p>"
        "<p>This program let you stream audio, video and image files to UPnP/DLNA devices.</p>"
        ).arg(name, version);
    QString translatedTextAboutText = tr(
         "<p>Copyright &copy; %1 %2</p>"
         "<p>%3 is developed as an open source project under <a href=\"%4\">%5</a>."
         ).arg(Jupii::COPYRIGHT_YEAR,
               Jupii::AUTHOR,
               name,
               Jupii::LICENSE_URL,
               Jupii::LICENSE);

    QMessageBox* msgBox = new QMessageBox(this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(tr("About %1").arg(name));
    msgBox->setText(translatedTextAboutCaption);
    msgBox->setInformativeText(translatedTextAboutText);

    QPixmap pm(QLatin1String(":/images/jupii-64.png"));
    if (!pm.isNull())
        msgBox->setIconPixmap(pm);

    msgBox->exec();
}

void MainWindow::on_actionQt_triggered()
{
    QApplication::aboutQt();
}

void MainWindow::on_playlistView_activated(const QModelIndex &index)
{
    play(index.row());
}

void MainWindow::play(int idx)
{
    if (idx >= 0 && playlistModel) {
        auto item = dynamic_cast<PlaylistItem*>(playlistModel->readRow(idx));
        if (item) {
            auto av = Services::instance()->avTransport;
            bool playing = av->getTransportState() == AVTransport::Playing;
            if (item->active()) {
                if (!playing)
                    togglePlay();
            } else {
                playlistModel->setToBeActiveIndex(idx);
                av->setLocalContent(item->id(),"");
            }
        }
    }
}

void MainWindow::on_clearButton_clicked()
{
    if (playlistModel) {
        playlistModel->clear();
        updatePlaylist(false);
    }
}

void MainWindow::on_actionRemoveItem_triggered()
{
    if (playlistModel) {
        auto indexes = ui->playlistView->selectionModel()->selectedIndexes();
        for (const auto &idx : indexes)
            playlistModel->removeIndex(idx.row());
    }
}

void MainWindow::on_actionSettings_triggered()
{
    settingsDialog->exec();
}

void MainWindow::on_volumeSlider_actionTriggered(int action)
{
    Q_UNUSED(action)
    auto rc = Services::instance()->renderingControl;
    int pos = ui->volumeSlider->sliderPosition();
    ui->volumeLabel->setText(QString::number(pos));
    rc->setVolume(pos);
}
void MainWindow::on_progressSlider_actionTriggered(int action)
{
    Q_UNUSED(action)
    auto av = Services::instance()->avTransport;
    auto utils = Utils::instance();
    int pos = ui->progressSlider->sliderPosition();
    int max = av->getCurrentTrackDuration();
    ui->progressLabel->setText(utils->secToStr(pos) + "/" + utils->secToStr(max));
    av->seek(pos);
}

void MainWindow::on_playlistView_customContextMenuRequested(const QPoint &pos)
{
    if (playlistModel) {
        int idx = ui->playlistView->indexAt(pos).row();
        if (idx >= 0) {
            auto item = dynamic_cast<PlaylistItem*>(playlistModel->readRow(idx));
            if (item) {
                auto av = Services::instance()->avTransport;
                bool playing = av->getTransportState() == AVTransport::Playing;
                ui->actionPauseItem->setEnabled(av->getControlable());
                QList<QAction*> actions;
                if (item->active() && playing)
                    actions << ui->actionPauseItem;
                else
                    actions << ui->actionPlayItem;
                actions << ui->actionRemoveItem;
                QMenu::exec(actions, ui->playlistView->mapToGlobal(pos),
                            nullptr, ui->playlistView);
            }
        }
    }
}

void MainWindow::on_actionPlayItem_triggered()
{
    if (playlistModel) {
        auto indexes = ui->playlistView->selectionModel()->selectedIndexes();
        if (indexes.length() > 0) {
            play(indexes.first().row());
        } else {
            qWarning() << "No items selected";
        }
    }
}

void MainWindow::on_actionPauseItem_triggered()
{
    bool playing = Services::instance()->avTransport->
            getTransportState() == AVTransport::Playing;

    if (playing)
        togglePlay();
}

void MainWindow::on_deviceList_customContextMenuRequested(const QPoint &pos)
{
    if (deviceModel) {
        int idx = ui->deviceList->indexAt(pos).row();
        if (idx >= 0) {
            auto item = dynamic_cast<DeviceItem*>(deviceModel->readRow(idx));
            if (item) {
                ui->actionConnect->setEnabled(!item->active());
                QList<QAction*> actions;
                actions << ui->actionConnect;
                QMenu::exec(actions, ui->deviceList->mapToGlobal(pos),
                            nullptr, ui->deviceList);
            }
        }
    }
}

void MainWindow::on_actionConnect_triggered()
{
    if (deviceModel) {
        auto indexes = ui->deviceList->selectionModel()->selectedIndexes();
        if (indexes.length() > 0) {
            on_deviceList_activated(indexes.first());
        } else {
            qWarning() << "No items selected";
        }
    }
}
