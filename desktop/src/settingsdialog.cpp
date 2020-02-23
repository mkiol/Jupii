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
    ui->allDevicesCheckBox->setCheckState(s->getShowAllDevices() ? Qt::Checked : Qt::Unchecked);
    ui->contentDirCheckBox->setCheckState(s->getContentDirSupported() ? Qt::Checked : Qt::Unchecked);

    bool screenEnabled = s->getScreenSupported();
    ui->screenFramerateComboBox->setEnabled(screenEnabled);
    int framerate = s->getScreenFramerate();
    int index = framerate < 15 ? 0 : framerate < 30 ? 1 : 2;
    ui->screenFramerateComboBox->setCurrentIndex(index);
    ui->cropCheckBox->setEnabled(screenEnabled);
    ui->cropCheckBox->setCheckState(s->getScreenCropTo169() > 0 ? Qt::Checked : Qt::Unchecked);
    ui->screenAudioCheckBox->setEnabled(screenEnabled);
    ui->screenAudioCheckBox->setCheckState(s->getScreenAudio() ? Qt::Checked : Qt::Unchecked);

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

void SettingsDialog::on_allDevicesCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setShowAllDevices(checked);
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

void SettingsDialog::on_screenFramerateComboBox_activated(int index)
{
    auto s = Settings::instance();
    int framerate;
    switch (index) {
    case 0:
        framerate = 5; break;
    case 1:
        framerate = 15; break;
    case 2:
        framerate = 30; break;
    default:
        framerate = 15;
    }
    s->setScreenFramerate(framerate);
}

void SettingsDialog::on_cropCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setScreenCropTo169(checked ? 1 : 0);
}

void SettingsDialog::on_screenAudioCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setScreenAudio(checked);
}

void SettingsDialog::on_contentDirCheckBox_toggled(bool checked)
{
    auto s = Settings::instance();
    s->setContentDirSupported(checked);
}
