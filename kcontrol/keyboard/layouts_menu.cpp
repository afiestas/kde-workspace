/*
 *  Copyright (C) 2010 Andriy Rysin (rysin@kde.org)
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

#include <kmenu.h>
#include <ktoolinvocation.h>
#include <klocalizedstring.h>

#include <QtGui/QAction>
#include <QtGui/QMenu>

#include "keyboard_config.h"
#include "x11_helper.h"
#include "xkb_helper.h"
#include "flags.h"
#include "layouts_menu.h"


LayoutsMenu::LayoutsMenu(const KeyboardConfig& keyboardConfig_, const Rules& rules_, Flags& flags_):
	keyboardConfig(keyboardConfig_),
	rules(rules_),
	flags(flags_),
	actionGroup(NULL)
{
}

LayoutsMenu::~LayoutsMenu()
{
	delete actionGroup;
}

const QIcon LayoutsMenu::getFlag(const QString& layout) const
{
	return keyboardConfig.showFlag ? flags.getIcon(layout) : QIcon();
}

// TODO: next 3 methods duplicated in keyboard_applet.cpp
void LayoutsMenu::actionTriggered(QAction* action)
{
	QString data = action->data().toString();
	if( data == "config" ) {
		QStringList args;
		args << "kcm_keyboard";
		KToolInvocation::kdeinitExec("kcmshell4", args);
	}
	else {
		LayoutUnit layoutUnit(LayoutUnit(action->data().toString()));
		if( X11Helper::getCurrentLayouts().layouts.contains(layoutUnit) ) {
			X11Helper::setLayout(layoutUnit);
		}
		else {
			QList<LayoutUnit> layouts(keyboardConfig.getDefaultLayouts());
			layouts.removeLast();
			layouts.append(layoutUnit);
			XkbHelper::initializeKeyboardLayouts(layouts);
			X11Helper::setLayout(layoutUnit);
		}
	}
}

QAction* LayoutsMenu::createAction(const LayoutUnit& layoutUnit) const
{
	QString shortText = Flags::getShortText(layoutUnit, keyboardConfig);
	QString longText = Flags::getLongText(layoutUnit, &rules);
	QString menuText = i18nc("short layout label - full layout name", "%1 - %2", shortText, longText);
	QAction* action = new QAction(getFlag(layoutUnit.layout), menuText, actionGroup);
	action->setData(layoutUnit.toString());
	return action;
}

QList<QAction*> LayoutsMenu::contextualActions()
{
	if( actionGroup ) {
		disconnect(actionGroup, SIGNAL(triggered(QAction*)), this, SLOT(actionTriggered(QAction*)));
		delete actionGroup;
	}
	actionGroup = new QActionGroup(this);

	X11Helper::getLayoutsList(); //UGLY: seems to be more reliable with extra call
	QList<LayoutUnit> currentLayouts = X11Helper::getLayoutsList();
	foreach(const LayoutUnit& layoutUnit, currentLayouts) {
		QAction* action = createAction(layoutUnit);
		actionGroup->addAction(action);
	}

	if( keyboardConfig.configureLayouts ) {
		QList<LayoutUnit> extraLayouts = keyboardConfig.layouts;
		foreach(const LayoutUnit& layoutUnit, currentLayouts) {
			extraLayouts.removeOne(layoutUnit);
		}
		if( extraLayouts.size() > 0 ) {
			QAction* separator = new QAction(actionGroup);
			separator->setSeparator(true);
			actionGroup->addAction(separator);

			foreach(const LayoutUnit& layoutUnit, extraLayouts) {
				QAction* action = createAction(layoutUnit);
				actionGroup->addAction(action);
			}
		}
	}

	QAction* separator = new QAction(actionGroup);
	separator->setSeparator(true);
	actionGroup->addAction(separator);
	QAction* configAction = new QAction(i18n("Configure..."), actionGroup);
	actionGroup->addAction(configAction);
	configAction->setData("config");
	connect(actionGroup, SIGNAL(triggered(QAction*)), this, SLOT(actionTriggered(QAction*)));
	return actionGroup->actions();
}