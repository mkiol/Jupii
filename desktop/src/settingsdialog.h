/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    int exec();

private slots:
    void on_lastPlaylistCheckBox_toggled(bool checked);
    void on_imageCheckBox_toggled(bool checked);
    void on_allDevicesCheckBox_toggled(bool checked);
    void on_netiInfsComboBox_activated(int index);

    void on_remoteContentModeComboBox_activated(int index);

    void on_micCheckBox_toggled(bool checked);

private:
    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
