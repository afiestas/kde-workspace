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


#ifndef KEYBOARD_CONFIG_H_
#define KEYBOARD_CONFIG_H_

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>

struct LayoutConfig {
	QString layout;
	QString variant;

	LayoutConfig() {}
	LayoutConfig(const LayoutConfig& layoutConfig) {
		layout = layoutConfig.layout;
		variant = layoutConfig.variant;
	}
};

/**
 * This class provides configuration options for keyboard module
 */
class KeyboardConfig
{
public:
	static const int MAX_LABEL_LEN = 3;

//	enum NumLockSate { ON = 0, OFF = 1, NO_CHANGE = 2 };
	enum SwitchingPolicy {
		SWITCH_POLICY_GLOBAL = 0,
		SWITCH_POLICY_DESKTOP = 1,
		SWITCH_POLICY_APPLICATION = 2,
		SWITCH_POLICY_WINDOW = 3
	};

	// init hardware options
//	int clickVolume;
//	bool keyboardRepeat;
//	int repeatDelay;
//	int repeatRate;
//	NumLockSate numlockState;

	QString keyboardModel;
	bool resetOldXkbOptions;
	QStringList xkbOptions;

	// init layouts options
	bool configureLayouts;
	QList<LayoutConfig> layouts;		// one layout is "layout : variant" or "layout"

	// switch cotrol options
	SwitchingPolicy switchingPolicy;
//	bool stickySwitching;
//	int stickySwitchingDepth;

	// display options
	bool showFlag;
//	QMap<QString, QString> displayNames;	// layoutPair -> display name

	void setDefaults();
	void load();
	void save();
};

#endif /* KEYBOARD_CONFIG_H_ */
