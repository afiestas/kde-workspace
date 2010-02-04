/*
 *   Copyright 2010 Aaron Seigo <aseigo@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
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

#ifndef APPLET
#define APPLET

#include <QObject>
#include <QWeakPointer>

#include <KConfigGroup>

namespace Plasma
{
    class Applet;
} // namespace Plasma

class Applet : public QObject
{
    Q_OBJECT

public:
    Applet(QObject *parent = 0);
    ~Applet();

    QStringList configKeys() const;
    QStringList configGroups() const;

    void setCurrentConfigGroup(const QStringList &groupNames);
    QStringList currentConfigGroup() const;

    virtual Plasma::Applet *applet() const;

public Q_SLOTS:
    virtual QVariant readConfig(const QString &key, const QVariant &def = QString()) const;
    virtual void writeConfig(const QString &key, const QVariant &value);
    virtual void reloadConfig();

private:
    KConfigGroup m_configGroup;
    QStringList m_configGroupPath;
    bool m_configDirty;
};


#endif
