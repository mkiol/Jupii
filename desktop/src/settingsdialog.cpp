/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"
#include "utils.h"
#include "info.h"

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
#ifdef PULSE
    ui->pulseCheckBox->setCheckState(s->getPulseSupported() ? Qt::Checked : Qt::Unchecked);
    ui->pulseModeComboBox->setEnabled(s->getPulseSupported());
    ui->pulseModeComboBox->setCurrentIndex(s->getPulseMode());
#else
    ui->pulseCheckBox->setVisible(false);
    ui->pulseModeComboBox->setEnabled(false);
#endif
    ui->remoteContentModeComboBox->setCurrentIndex(s->getRemoteContentMode());

    // Interfaces
    auto infs = Utils::instance()->getNetworkIfs();
    ui->netiInfsComboBox->clear();
    ui->netiInfsComboBox->addItem(tr("auto"));
    ui->netiInfsComboBox->addItems(infs);
    auto prefInf = s->getPrefNetInf();
    if (prefInf.isEmpty())
        ui->netiInfsComboBox->setCurrentIndex(0); // auto
    else
        ui->netiInfsComboBox->setCurrentText(prefInf);

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

void SettingsDialog::on_pulseCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setPulseSupported(checked);
    ui->pulseModeComboBox->setEnabled(checked);
}

void SettingsDialog::on_netiInfsComboBox_activated(int index)
{
    auto s = Settings::instance();
    if (index == 0) {
        s->setPrefNetInf("");
    } else {
        auto inf = ui->netiInfsComboBox->currentText();
        qDebug() << "New prefered network interface:" << inf;
        s->setPrefNetInf(inf);
    }
}

void SettingsDialog::on_remoteContentModeComboBox_activated(int index)
{
    auto s = Settings::instance();
    s->setRemoteContentMode(index);
}

void SettingsDialog::on_pulseModeComboBox_activated(int index)
{
    auto s = Settings::instance();
    s->setPulseMode(index);
}
