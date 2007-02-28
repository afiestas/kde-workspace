/*
 *   Copyright (C) 2007 Matt Broadstone <mbroadst@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef DESKTOP_H
#define DESKTOP_H

#include <QWidget>

class QGraphicsView;
class QGraphicsScene;
class Desktop : public QWidget
{
    Q_OBJECT
public:
    Desktop(QWidget *parent = 0);

private:
    QGraphicsView *m_graphicsView;
    QGraphicsScene *m_graphicsScene;

};

#endif


