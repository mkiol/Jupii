/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QWidget>
#include <QPixmap>
#include <memory>

#include "devicemodel.h"
#include "filedownloader.h"
#include "playlistmodel.h"
#include "settingsdialog.h"
#include "avtransport.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_directory_busyChanged();
    void on_directory_initedChanged();
    void on_service_initedChanged();
    void on_av_currentMetaDataChanged();
    void on_av_positionChanged();
    void on_av_albumArtChanged();
    void on_av_stateChanged();
    void on_av_updated();
    void on_av_transportActionsChanged();
    void on_rc_volumeChanged();
    void on_rc_muteChanged();
    void on_deviceList_activated(const QModelIndex &index);
    void on_connectButton_clicked();
    void on_prevButton_clicked();
    void on_nextButton_clicked();
    void on_muteButton_toggled(bool checked);
    void on_playButton_clicked();
    void on_playmodeButton_clicked();
    void on_playlistModel_playModeChanged();
    void on_playlistModel_prevSupportedChanged();
    void on_playlistModel_nextSupportedChanged();
    void on_volumeSlider_sliderMoved(int position);
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void on_actionQt_triggered();
    void on_playlistView_activated(const QModelIndex &index);
    void on_actionRemoveItem_triggered();
    void on_actionSettings_triggered();
    void on_volumeSlider_actionTriggered(int action);
    void on_progressSlider_sliderMoved(int position);
    void on_progressSlider_actionTriggered(int action);
    void on_playlistView_customContextMenuRequested(const QPoint &pos);
    void on_actionPlayItem_triggered();
    void on_actionPauseItem_triggered();
    void on_deviceList_customContextMenuRequested(const QPoint &pos);
    void on_actionConnect_triggered();
    void on_actionFiles_triggered();
    void on_actionURL_triggered();
    void on_actionClear_triggered();
    void on_actionMic_triggered();
    void on_actionPulse_triggered();
    void on_recButton_toggled(bool checked);
    void on_cs_streamToRecordChanged(const QUrl &id, bool value);
    void on_cs_streamRecordableChanged(const QUrl &id, bool value);

private:
    Ui::MainWindow *ui;
    std::unique_ptr<DeviceModel> deviceModel;
    std::unique_ptr<FileDownloader> downloader;
    std::unique_ptr<SettingsDialog> settingsDialog;

    void enablePlaylist(bool enabled);
    void notify(const QString& message = "");
    void togglePlay();
    void play(int idx);
    QIcon getIconForType(AVTransport::Type type);
    QPixmap getImageForCurrent();
};

#endif // MAINWINDOW_H
