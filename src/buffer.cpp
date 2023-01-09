/****************************************************************************
 *
 *  buffer.cpp - Lipstick2vnc, a VNC server for Mer devices
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

#include "buffer.h"

Buffer *Buffer::create(wl_shm *shm, QScreen *screen)
{
    int width = screen->size().width();
    int height = screen->size().height();
    int stride = width * 4;
    int size = stride * height;

    char filename[] = "/tmp/lipstick-recorder-shm-XXXXXX";
    int fd = mkstemp(filename);
    if (fd < 0) {
        qWarning("creating a buffer file for %d B failed: %m\n", size);
        return Q_NULLPTR;
    }
    int flags = fcntl(fd, F_GETFD);
    if (flags != -1)
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

    if (ftruncate(fd, size) < 0) {
        qWarning("ftruncate failed: %s", strerror(errno));
        close(fd);
        return Q_NULLPTR;
    }

    uchar *data = (uchar *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unlink(filename);
    if (data == (uchar *)MAP_FAILED) {
        qWarning("mmap failed: %m\n");
        close(fd);
        return Q_NULLPTR;
    }

    Buffer *buf = new Buffer;

    wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    buf->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    //buf->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_RGB888);
    wl_buffer_set_user_data(buf->buffer, buf);
    wl_shm_pool_destroy(pool);
    buf->data = data;
    buf->image = QImage(data, width, height, stride, QImage::Format_RGBA8888);
    //buf->image = QImage(data, width, height, stride, QImage::Format_RGB32);
    buf->busy = false;
    close(fd);
    return buf;
}
