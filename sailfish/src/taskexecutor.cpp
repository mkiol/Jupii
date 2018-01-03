/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>

#include "taskexecutor.h"

TaskExecutor::Task::Task(std::function<void()> job) : m_job(job)
{
}

void TaskExecutor::Task::run()
{
    m_job();
}

TaskExecutor::TaskExecutor(QObject* parent, int threadCount) :
    m_pool(parent)
{
    m_pool.setMaxThreadCount(threadCount);
}

bool TaskExecutor::startTask(const std::function<void()> &job)
{
    if (m_pool.activeThreadCount() >= m_pool.maxThreadCount()) {
        qWarning() << "Too many task. Droping new task";
        return false;
    }

    auto task = new Task(job);
    task->setAutoDelete(true);
    m_pool.start(task);
    return true;
}

void TaskExecutor::waitForDone()
{
    m_pool.waitForDone();
}

bool TaskExecutor::taskActive()
{
    return m_pool.activeThreadCount() > 0;
}
