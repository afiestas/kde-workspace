/***************************************************************************
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef CLOCKAPPLET_H
#define CLOCKAPPLET_H

#include <QtCore/QTime>
#include <QtCore/QDate>

#include <plasma/applet.h>
#include <plasma/dataengine.h>
#include <plasma/dialog.h>
#include "ui_calendar.h"
#include <KDatePicker>

#include "plasmaclock_export.h"


class KDialog;

namespace Plasma
{
    class Svg;
}

class PLASMA_EXPORT ClockApplet : public Plasma::Applet
{
    Q_OBJECT
    public:
        ClockApplet(QObject *parent, const QVariantList &args);
        ~ClockApplet();

        void mousePressEvent(QGraphicsSceneMouseEvent *event);

    protected slots:
        void showCalendar(QGraphicsSceneMouseEvent *event);

    private:
        void updateToolTipContent();

        Plasma::Dialog *m_calendar;
        Ui::calendar m_calendarUi;
        QVBoxLayout *m_layout;
};


#endif
