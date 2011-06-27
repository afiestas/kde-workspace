/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

Since the functionality provided in this class has been moved from
class Workspace, it is not clear who exactly has written the code.
The list below contains the copyright holders of the class Workspace.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SCREENEDGE_H
#define KWIN_SCREENEDGE_H
#include <QtCore/QObject>
#include <QtCore/QVector>
#include "kwinglobals.h"


namespace KWin {

//-----------------------------------------------------------------------------
// Electric Borders
//-----------------------------------------------------------------------------
// Electric Border Window management. Electric borders allow a user to change
// the virtual desktop or activate another features by moving the mouse pointer
// to the borders or corners. Technically this is done with input only windows.
//-----------------------------------------------------------------------------
class ScreenEdge : QObject {
    Q_OBJECT
public:
    ScreenEdge();
    ~ScreenEdge();
    void init();
    void check(const QPoint& pos, Time now);
    void restoreSize(ElectricBorder border);
    void reserve(ElectricBorder border);
    void unreserve(ElectricBorder border);
    void reserveActions(bool isToReserve);
    void reserveDesktopSwitching(bool isToReserve);
    void raiseWindows();
    void destroy();
    bool isEntered(XEvent * e);
    const QVector< Window* >& windows();
public Q_SLOTS:
    void update();
private:
    void switchDesktop(ElectricBorder border, const QPoint& pos);

    ElectricBorder m_currentScreenEdge;
    Window m_screenEdgeWindows[ELECTRIC_COUNT];
    int m_screenEdgeLeft;
    int m_screenEdgeRight;
    int m_screenEdgeTop;
    int m_screenEdgeBottom;
    Time m_screenEdgeTimeFirst;
    Time m_screenEdgeTimeLast;
    Time m_screenEdgeTimeLastTrigger;
    QPoint m_screenEdgePushPoint;
    int m_screenEdgeReserved[ELECTRIC_COUNT]; // Corners/edges used by something
};
}
#endif // KWIN_SCREENEDGE_H
