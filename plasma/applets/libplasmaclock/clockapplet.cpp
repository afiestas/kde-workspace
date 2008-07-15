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

#include "clockapplet.h"

#include <math.h>

#include <QtGui/QPainter>
#include <QtGui/QStyleOptionGraphicsItem>
#include <QtGui/QSpinBox>
#include <QtCore/QTimeLine>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtCore/QDate>

#include <KColorScheme>
#include <KConfigDialog>
#include <KConfigGroup>
#include <KDatePicker>
#include <KDebug>
#include <KDialog>
#include <KGlobalSettings>

#include <plasma/dataengine.h>
#include <plasma/dialog.h>
#include <plasma/theme.h>

#include "ui_timezonesConfig.h"

class ClockApplet::Private
{
public:
    Private()
        : calendar(0),
          timezone("Local")
    {}

    Ui::calendar calendarUi;
    Ui::timezonesConfig ui;
    Plasma::Dialog *calendar;
    QString timezone;
    QPoint clicked;
    QStringList m_timeZones;
};

ClockApplet::ClockApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
      d(new Private)
{
}

ClockApplet::~ClockApplet()
{
    delete d->calendar;
}

void ClockApplet::updateToolTipContent() {
    //QString timeString = KGlobal::locale()->formatTime(d->time, d->showSeconds);
    //TODO port to TOOLTIP manager
    /*Plasma::ToolTipData tipData;

    tipData.mainText = "";//d->time.toString(timeString);
    tipData.subText = "";//d->date.toString();
    //tipData.image = d->toolTipIcon;

    setToolTip(tipData);*/
}

void ClockApplet::createConfigurationInterface(KConfigDialog *parent)
{
    createClockConfigurationInterface(parent);

    QWidget *widget = new QWidget();
    d->ui.setupUi(widget);

    parent->addPage(widget, i18n("Time Zones"), icon());

    d->ui.localTimeZone->setChecked(isLocalTimezone());
    d->ui.timeZones->setSelected(currentTimezone(), true);
    d->ui.timeZones->setEnabled(!isLocalTimezone());

    parent->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));

#if 0
#ifdef CLOCK_APPLET_CONF
    ui.localTimeZone->setChecked(isLocalTimezone());
    ui.timeZones->setEnabled(!isLocalTimezone());
    foreach (const QString &str, m_timeZones) {
        ui.timeZones->setSelected(str, true);
    }
#endif
#endif
}

void ClockApplet::createClockConfigurationInterface(KConfigDialog *parent)
{

}

void ClockApplet::clockConfigAccepted()
{

}

void ClockApplet::configAccepted()
{
    KConfigGroup cg = config();

    d->m_timeZones = d->ui.timeZones->selection();
    cg.writeEntry("timeZones", d->m_timeZones);

    QString newTimezone = localTimezone();

    if (!d->ui.localTimeZone->isChecked() && !d->m_timeZones.isEmpty()) {
        newTimezone = d->m_timeZones.at(0);
    }

    changeEngineTimezone(currentTimezone(), newTimezone);

    setCurrentTimezone(newTimezone);
    cg.writeEntry("currentTimezone", newTimezone);

    clockConfigAccepted();

    constraintsEvent(Plasma::SizeConstraint);
    update();
    emit configNeedsSaving();
}

void ClockApplet::changeEngineTimezone(QString oldTimezone, QString newTimezone)
{
    Q_UNUSED(oldTimezone);
    Q_UNUSED(newTimezone);
}

void ClockApplet::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton) {
        d->clicked = scenePos().toPoint();
        event->setAccepted(true);
        return;
    }

    Applet::mousePressEvent(event);
}

void ClockApplet::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if ((d->clicked - scenePos().toPoint()).manhattanLength() <
        KGlobalSettings::dndEventDelay()) {
        showCalendar(event);
    }
}

void ClockApplet::showCalendar(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);

    if (d->calendar == 0) {
        d->calendar = new Plasma::Dialog();
        d->calendarUi.setupUi(d->calendar);
        d->calendar->setWindowFlags(Qt::Popup);
        d->calendar->adjustSize();
    }

    if (d->calendar->isVisible()) {
        d->calendar->hide();
    } else {
        kDebug();
        Plasma::DataEngine::Data data = dataEngine("time")->query(currentTimezone());
        d->calendarUi.kdatepicker->setDate(data["Date"].toDate());
        d->calendar->move(popupPosition(d->calendar->sizeHint()));
        d->calendar->show();
    }
}

void ClockApplet::setCurrentTimezone(const QString &tz)
{
    d->timezone = tz;
}

QString ClockApplet::currentTimezone() const
{
    return d->timezone;
}

bool ClockApplet::isLocalTimezone() const
{
    return d->timezone == localTimezone();
}

QString ClockApplet::localTimezone()
{
    return "Local";
}

#include "clockapplet.moc"
