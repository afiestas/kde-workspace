/*
 *   Copyright (C) 2006 Aaron Seigo <aseigo@kde.org>
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

#ifndef PLASMA_ENGINE_H
#define PLASMA_ENGINE_H

#include <QAtomic>
#include <QHash>
#include <QObject>
#include <QStringList>

namespace Plasma
{

class DataSource;
class DataVisualization;

class DataSource : public QObject
{
    Q_OBJECT

    public:
        typedef QHash<QString, DataSource*> Dict;
        typedef QHash<QString, QVariant> Data;

        DataSource(QObject* parent);
        virtual ~DataSource();

        QString name();

    signals:
        void data(const DataSource::Data&);

    private:
        class Private;
        Private* d;
};

class DataEngine : public QObject
{
    Q_OBJECT

    public:
        typedef QHash<QString, DataEngine*> Dict;

        DataEngine(QObject* parent);
        virtual ~DataEngine();

        virtual QStringList dataSources();
        void connect(const QString& source, DataVisualization* visualization);
        DataSource::Data query(const QString& source);

        void ref();
        void deref();

    protected:
        virtual void init();
        virtual void cleanup();
        virtual DataEngine* engine() = 0;
        virtual

    private:
        QAtomic ref;
        class Private;
        Private* d;
};

} // Plasma namespace

#define K_EXPORT_PLASMA_ENGINE(libname, classname)                       \
        K_EXPORT_COMPONENT_FACTORY(                                      \
                        plasmaengine_##libname,                          \
                        KGenericFactory<classname>("libplasmaengine_" #libname))

#endif // multiple inclusion guard


#endif
