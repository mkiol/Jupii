/****************************************************************************
 *
 *  frameevent.cpp - Lipstick2vnc, a VNC server for Mer devices
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

#include <QEvent>

#include "frameevent.h"

const QEvent::Type FrameEvent::FrameEventType = (QEvent::Type)QEvent::registerEventType();

FrameEvent::FrameEvent(Buffer *b, uint32_t time, int tr)
    : QEvent(FrameEventType)
    , buffer(b)
    , timestamp(time)
    , transform(tr)
{
}
