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

#include "xkb_helper.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <kglobal.h>
#include <kstandarddirs.h>
#include <kprocess.h>
#include <kdebug.h>

#include "keyboard_config.h"

static const char* SETXKBMAP_EXEC = "setxkbmap";

static bool setxkbmapNotFound = false;
static QString setxkbmapExe;

static
QString getSetxkbmapExe()
{
	if( setxkbmapNotFound )
		return "";

	if( setxkbmapExe.isEmpty() ) {
		setxkbmapExe = KGlobal::dirs()->findExe(SETXKBMAP_EXEC);
		if( setxkbmapExe.isEmpty() ) {
			setxkbmapNotFound = true;
			kError() << "Can't find" << SETXKBMAP_EXEC << "- keyboard layouts won't be configured";
			return "";
		}
	}
	return setxkbmapExe;
}

static
void executeXmodmap(const QString& configFileName)
{
    if( QFile(configFileName).exists() ) {
    	QString xmodmap_exe = KGlobal::dirs()->findExe("xmodmap");
    	if( ! xmodmap_exe.isEmpty() ) {
    		KProcess xmodmapProcess;
    		xmodmapProcess << xmodmap_exe;
    		xmodmapProcess << configFileName;
    		kDebug() << "Executing" << xmodmapProcess.program().join(" ");
    		if( xmodmapProcess.execute() != 0 ) {
    			kError() << "Failed to execute " << xmodmapProcess.program();
    		}
    	}
    }
}

bool XkbHelper::initializeKeyboardLayouts()
{
	getSetxkbmapExe();
	if( ! setxkbmapNotFound ) {

		KeyboardConfig config;
		config.load();

		XkbHelper::initializeKeyboardLayouts(config);
	}
}

bool XkbHelper::initializeKeyboardLayouts(KeyboardConfig& config)
{
	QStringList setxkbmapCommandArguments;
	if( ! config.keyboardModel.isEmpty() ) {
		setxkbmapCommandArguments.append("-model");
		setxkbmapCommandArguments.append(config.keyboardModel);
	}
	if( config.configureLayouts ) {
		QStringList layouts;
		QStringList variants;
		foreach (LayoutConfig layoutConfig, config.layouts) {
			layouts.append(layoutConfig.layout);
			variants.append(layoutConfig.variant);
		}

		setxkbmapCommandArguments.append("-layout");
		setxkbmapCommandArguments.append(layouts.join(","));
		if( ! variants.join("").isEmpty() ) {
			setxkbmapCommandArguments.append("-variant");
			setxkbmapCommandArguments.append(variants.join(","));
		}
	}
	if( config.resetOldXkbOptions ) {
		setxkbmapCommandArguments.append("-option");
	}
	if( ! config.xkbOptions.isEmpty() ) {
		setxkbmapCommandArguments.append("-option");
		setxkbmapCommandArguments.append(config.xkbOptions.join(","));
	}

	if( ! setxkbmapCommandArguments.isEmpty() ) {
		KProcess setxkbmapProcess;
		setxkbmapProcess << getSetxkbmapExe() << setxkbmapCommandArguments;
		int res = setxkbmapProcess.execute();

		if( res == 0 ) {	// restore Xmodmap mapping reset by setxkbmap
			kDebug() << "Executed successfully" << setxkbmapProcess.program().join(" ");

			// TODO: is just home .Xmodmap enough or should system be involved too?
			//    QString configFileName = QDir("/etc/X11/xinit").filePath(".Xmodmap");
			//    executeXmodmap(configFileName);
			QString configFileName = QDir::home().filePath(".Xmodmap");
			executeXmodmap(configFileName);
		    return true;
		}
		else {
			kError() << "Failed to run" << setxkbmapProcess.program().join(" ") << "return code: " + res;
		}
	}
	return false;
}

//bool XkbHelper::initializeKeyboardHardware(KeyboardConfig& config)
//{
//	int numlockState = config.numlockState;
//	if( config.numlockState != KeyboardConfig::NO_CHANGE ) {
//		//TODO:
////		numlockx_change_numlock_state( numlockState == 0 );
//	}
//
//	if( config.keyboardRepeat ) {
//		//TODO:
////		set_repeatrate(config.repeatDelay, config.repeatRate);
//	}
//
//	//TODO:
////	clickVolume
//}
//
//extern "C"
//{
//	KDE_EXPORT void kcminit_keyboard()
//	{
//		//TODO: better check if xkb is supported?
//		getSetxkbmapExe();
//		if( ! setxkbmapNotFound ) {
//
//			KeyboardConfig config;
//			config.load();
//
//			KCMInit::initializeKeyboardHardware(config);
//			KCMInit::initializeKeyboardLayouts(config);
//		}
//	}
//}
