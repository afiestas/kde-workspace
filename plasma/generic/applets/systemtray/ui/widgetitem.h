/***********************************************************************************************************************
 * KDE System Tray (Plasmoid)
 *
 * Copyright (C) 2012 ROSA  <support@rosalab.ru>
 * License: GPLv2+
 * Authors: Dmitry Ashkadov <dmitry.ashkadov@rosalab.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .
 **********************************************************************************************************************/


#ifndef __SYSTEMTRAY__WIDGETITEM_H
#define __SYSTEMTRAY__WIDGETITEM_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Includes
#include <QtGui/QGraphicsWidget>
#include <QtDeclarative/QDeclarativeItem>

namespace SystemTray
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// class WidgetItem
/** @class WidgetItem
 * Represents declarative item containing an specified graphics widget.
 */
class WidgetItem: public QDeclarativeItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *widget READ widget WRITE setWidget NOTIFY changedWidget) ///< widget to embed
public:
    explicit WidgetItem(QDeclarativeItem *parent = 0);
    virtual ~WidgetItem();

public:
    QObject *widget() const;
    void setWidget(QObject *obj);

signals:
    void changedWidget();

private:
    void unbind();
    QWeakPointer<QGraphicsWidget> m_widget;
};

} //namespace SystemTray

#endif // __SYSTEMTRAY__WIDGETITEM_H
