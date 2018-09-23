/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "addurldialog.h"
#include "ui_addurldialog.h"

AddUrlDialog::AddUrlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddUrlDialog)
{
    ui->setupUi(this);
}

AddUrlDialog::~AddUrlDialog()
{
    delete ui;
}

QString AddUrlDialog::getUrl()
{
    return ui->urlEdit->text().trimmed();
}

QString AddUrlDialog::getName()
{
    return ui->nameEdit->text().trimmed();
}
