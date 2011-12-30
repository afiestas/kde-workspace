/*
 *   Copyright 2010 by Chani Armitage <chani@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library/Lesser General Public License
 *   version 2, or (at your option) any later version, as published by the
 *   Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library/Lesser General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ACTIVITYMANAGER_H
#define ACTIVITYMANAGER_H

#include <QGraphicsWidget>
#include <Plasma/Plasma>

namespace Plasma
{
class Corona;
class Containment;
}
class ActivityManagerPrivate;

class ActivityManager : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<QVariant> activityTypeActions READ activityTypeActions NOTIFY activityTypeActionsChanged)

public:
    //FIXME must learn about 'explicit'
    ActivityManager(Plasma::Location loc, QGraphicsItem *parent=0);
    ActivityManager(QGraphicsItem *parent=0);
    ~ActivityManager();

    /**
     * Changes the current containment
     * you've got to call this at least once so that it can access the corona
     * FIXME if you can use scene() as corona, get rid of this
     */
    void setContainment(Plasma::Containment *containment);

    /**
     * set location
     */
    void setLocation(Plasma::Location loc);

    /**
     * @return the location
     */
    Plasma::Location location();

    QList<QVariant> activityTypeActions();

    Q_INVOKABLE QPixmap pixmapForActivity(const QString &activityId);
    Q_INVOKABLE void cloneCurrentActivity();
    Q_INVOKABLE void createActivity(const QString &pluginName);
    Q_INVOKABLE void createActivityFromScript(const QString &script, const QString &name, const QString &icon, const QStringList &startupApps);
    Q_INVOKABLE void downloadActivityScripts();
    Q_INVOKABLE QString chooseIcon() const;

Q_SIGNALS:
    void locationChanged(Plasma::Location loc);
    void closeClicked();
    void addWidgetsRequested();
    void activityTypeActionsChanged();

private:
    Q_PRIVATE_SLOT(d, void containmentDestroyed())

    ActivityManagerPrivate * const d;

};

#endif
