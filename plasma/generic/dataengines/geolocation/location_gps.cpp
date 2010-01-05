/*
 *   Copyright (C) 2009 Petri Damstén <damu@iki.fi>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "location_gps.h"

Gpsd::Gpsd(gps_data_t* gpsdata)
    : m_gpsdata(gpsdata)
    , m_abort(false)
{
}

Gpsd::~Gpsd()
{
    m_abort = true;
    m_condition.wakeOne();
    wait();
}

void Gpsd::update()
{
    if (!isRunning()) {
        start();
    } else {
        m_condition.wakeOne();
    }
}

void Gpsd::run()
{
#if GPSD_API_MAJOR_VERSION == 3 && defined( WATCH_ENABLE )
    gps_stream(m_gpsdata, WATCH_ENABLE, NULL);
#else
    gps_query(m_gpsdata, "w+x\n");
#endif

    while (!m_abort) {
        Plasma::DataEngine::Data d;

        if (gps_poll(m_gpsdata) != -1) {
            //kDebug() << "poll ok";
            if (m_gpsdata->online) {
                //kDebug() << "online";
                if (m_gpsdata->status != STATUS_NO_FIX) {
                    //kDebug() << "fix";
                    d["accuracy"] = 30;
                    d["latitude"] = QString::number(m_gpsdata->fix.latitude);
                    d["longitude"] = QString::number(m_gpsdata->fix.longitude);
                }
            }
        }

        emit dataReady(d);

        m_condition.wait(&m_mutex);
    }
}

Gps::Gps(QObject* parent, const QVariantList& args)
    : GeolocationProvider(parent, args),
      m_gpsd(0)
{
    gps_data_t* gpsdata = gps_open("localhost", DEFAULT_GPSD_PORT);
    if (gpsdata) {
        kDebug() << "gpsd found.";
        m_gpsd = new Gpsd(gpsdata);
        connect(m_gpsd, SIGNAL(dataReady(const Plasma::DataEngine::Data&)),
                this, SIGNAL(setData(const Plasma::DataEngine::Data&)));
    } else {
        kDebug() << "gpsd not found";
    }

    setIsAvailable(m_gpsd);
}

Gps::~Gps()
{
    delete m_gpsd;
}

void Gps::update()
{
    if (m_gpsd) {
        m_gpsd->update();
    }
}

K_EXPORT_PLASMA_GEOLOCATIONPROVIDER(gps, Gps)

#include "location_gps.moc"
