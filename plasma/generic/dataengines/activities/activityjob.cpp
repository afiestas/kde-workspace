/*
 * Copyright 2009 Chani Armitage <chani@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "activityjob.h"

#include <kactivitycontroller.h>

ActivityJob::ActivityJob(KActivityController *controller, const QString &id, const QString &operation, QMap<QString, QVariant> &parameters, QObject *parent) :
    ServiceJob(parent->objectName(), operation, parameters, parent),
    m_activityController(controller),
    m_id(id)
{
}

ActivityJob::~ActivityJob()
{
}

void ActivityJob::start()
{
    const QString operation = operationName();
    if (operation == "add") {
        //I wonder how well plasma will handle this...
        QString name = parameters()["Name"].toString();
        if (name.isEmpty()) {
            name = i18n("unnamed");
        }
        m_activityController->addActivity(name);
        setResult(true);
        return;
    }
    //m_id takes precendence over parameters
    QString id = (m_id.isEmpty() ? parameters()["Id"].toString() : m_id);
    if (operation == "setCurrent") {
        m_activityController->setCurrentActivity(id);
        setResult(true);
        return;
    }
    if (operation == "setName") {
        m_activityController->setActivityName(id, parameters()["Name"].toString());
        setResult(true);
        return;
    }
    if (operation == "setIcon") {
        m_activityController->setActivityIcon(id, parameters()["Icon"].toString());
        setResult(true);
        return;
    }
    if (operation == "remove") {
        m_activityController->removeActivity(id);
        setResult(true);
        return;
    }
    setResult(false);
}

#include "activityjob.moc"