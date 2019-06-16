/****************************************************************************
 *
 *  recorder.h - Lipstick2vnc, a VNC server for Mer devices
 *  Copyright (C) 2014 Jolla Ltd.
 *  Contact: Reto Zingg <reto.zingg@jolla.com>
 *
 *  This file is part of Lipstick2vnc.
 *
 *  Lipstick2vnc is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 ****************************************************************************/

#ifndef LIPSTICKRECORDER_RECORDER_H
#define LIPSTICKRECORDER_RECORDER_H

#include <QObject>
#include <QMutex>
#include <wayland-client.h>

#include "buffer.h"
#include "frameevent.h"

class QScreen;

struct wl_display;
struct wl_registry;
struct lipstick_recorder_manager;
struct lipstick_recorder;

class Buffer;
class BuffersHandler;

class Recorder : public QObject
{
    Q_OBJECT
public:
    Recorder(QObject *eventReceiver, int amountOfBuffers);
    ~Recorder();
    void recordFrame();
    void repaint();

    bool m_starving;

signals:
    void ready();

private slots:
    void start();

private:
    static void global(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
    static void globalRemove(void *data, wl_registry *registry, uint32_t id);
    static void frame(void *data, lipstick_recorder *recorder, wl_buffer *buffer, uint32_t time, int transform);
    static void failed(void *data, lipstick_recorder *recorder, int result, wl_buffer *buffer);
    static void cancel(void *data, lipstick_recorder *recorder, wl_buffer *buffer);
    static void setup(void *data, lipstick_recorder *recorder, int width, int height, int stride, int format);

    QObject *m_eventReceiver;

    wl_display *m_display;
    wl_registry *m_registry;
    wl_shm *m_shm;
    lipstick_recorder_manager *m_manager;
    lipstick_recorder *m_lipstickRecorder;
    QScreen *m_screen;
    QList<Buffer *> m_buffers;
    QMutex m_mutex;

    int m_amountOfBuffers;
};

#endif
