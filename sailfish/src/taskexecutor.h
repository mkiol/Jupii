/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TASKEXECUTOR_H
#define TASKEXECUTOR_H

#include <QRunnable>
#include <QThreadPool>
#include <QObject>

#include <functional>

class TaskExecutor
{
public:
    class Task : public QRunnable
    {
    public:
        Task(std::function<void()> job);

    private:
        std::function<void()> m_job;
        void run();
    };

    TaskExecutor(QObject* parent = nullptr, int threadCount = 1);

    bool startTask(const std::function<void()> &job);
    void waitForDone();
    bool taskActive();

private:
    QThreadPool m_pool;
};

#endif // TASKEXECUTOR_H
