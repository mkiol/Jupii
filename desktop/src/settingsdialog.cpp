/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

int SettingsDialog::exec()
{
    auto s = Settings::instance();
    ui->lastPlaylistCheckBox->setCheckState(s->getRememberPlaylist() ? Qt::Checked : Qt::Unchecked);
    ui->allDevicesCheckBox->setCheckState(s->getShowAllDevices() ? Qt::Checked : Qt::Unchecked);
    ui->imageCheckBox->setCheckState(s->getImageSupported() ? Qt::Checked : Qt::Unchecked);
    return QDialog::exec();
}

void SettingsDialog::on_lastPlaylistCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setRememberPlaylist(checked);
}

void SettingsDialog::on_imageCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setImageSupported(checked);
}

void SettingsDialog::on_allDevicesCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setShowAllDevices(checked);
}
