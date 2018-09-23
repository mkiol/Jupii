/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ADDURLDIALOG_H
#define ADDURLDIALOG_H

#include <QDialog>

namespace Ui {
class AddUrlDialog;
}

class AddUrlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddUrlDialog(QWidget *parent = nullptr);
    QString getUrl();
    QString getName();
    ~AddUrlDialog();

private:
    Ui::AddUrlDialog *ui;
};

#endif // ADDURLDIALOG_H
