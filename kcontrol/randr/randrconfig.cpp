/*
 * Copyright (c) 2007 Gustavo Pichorim Boiko <gustavo.boiko@kdemail.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "collapsiblewidget.h"
#include "outputconfig.h"
#include "outputgraphicsitem.h"
#include "randrconfig.h"
#include "randrdisplay.h"
#include "randrscreen.h"
#include "randroutput.h"
#include "randr.h"

#ifdef HAS_RANDR_1_2

RandRConfig::RandRConfig(QWidget *parent, RandRDisplay *display)
: QWidget(parent), Ui::RandRConfigBase()
{
	m_display = display;
	Q_ASSERT(m_display);
	m_changed = false;

	if (!m_display->isValid())
		return;

	setupUi(this);

	// create the container for the settings widget
	QHBoxLayout *l = new QHBoxLayout(outputList);
	l->setSpacing(0);
	l->setContentsMargins(0,0,0,0);
	m_container = new SettingsContainer(outputList);
	l->addWidget(m_container);


	// create the scene
	m_scene = new QGraphicsScene(m_display->currentScreen()->rect());	
	screenView->setScene(m_scene);
}

RandRConfig::~RandRConfig()
{
}

void RandRConfig::load()
{
	kDebug() << "LOAD";
	if (!m_display->isValid())
		return;

	qDeleteAll(m_outputList);
	m_outputList.clear();

	QList<QGraphicsItem*> items = m_scene->items();
	foreach(QGraphicsItem *i, items)
		m_scene->removeItem(i);

	OutputMap outputs = m_display->currentScreen()->outputs();
	OutputMap::iterator it;

	// FIXME: adjust it to run on a multi screen system
	CollapsibleWidget *w;
	OutputGraphicsItem *o;
	for (it = outputs.begin(); it != outputs.end(); ++it)
	{
		o = new OutputGraphicsItem(*it);
		m_scene->addItem(o);

		OutputConfig *c = new OutputConfig(0, *it, o);
		w = m_container->insertWidget(c, (*it)->name());
		m_outputList.append(w);
		kDebug() << "Rect: " << (*it)->rect();
		connect(c, SIGNAL(updateView()), this, SLOT(slotUpdateView()));
	}

	slotUpdateView();

}

void RandRConfig::save()
{
	if (!m_display->isValid())
		return;

	apply();
}

void RandRConfig::defaults()
{
	update();
}

void RandRConfig::apply()
{
	if (m_changed) {
		m_display->applyProposed();

		update();
	}
}

void RandRConfig::update()
{
	// TODO: implement
}

void RandRConfig::slotUpdateView()
{
	QRect r(0,0,0,0);
	// updates the graphics view so that all outputs fit inside of it
	OutputMap outputs = m_display->currentScreen()->outputs();
	foreach(RandROutput *output, outputs)
	{
		r.setWidth(output->rect().width() + r.width());
		r.setHeight(output->rect().height() + r.height());
	}

	screenView->setSceneRect(r);
}
#include "randrconfig.moc"

#endif
