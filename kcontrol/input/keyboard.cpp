/*
 * keyboard.cpp
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 *
 * Layout management, cleanups:
 * Copyright (c) 1999 Dirk A. Mueller <dmuell@gmx.net>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

//#include <iostream.h>
//#include <sys/types.h>
//#include <unistd.h>
//#include <sys/stat.h>
//#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qfileinfo.h>
#include <qcheckbox.h>
#include <qstring.h>
#include <qlayout.h>
#include <qwhatsthis.h>
#include <qvbuttongroup.h>
#include <qradiobutton.h>
#include <klocale.h>
#include <kconfig.h>
#include <knuminput.h>

#include "keyboard.h"
#include <X11/Xlib.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#endif

KeyboardConfig::KeyboardConfig (QWidget * parent, const char *name)
    : KCModule (parent, name)
{
  QString wtstr;
  QBoxLayout* lay = new QVBoxLayout(this, 10);

  repeatBox = new QCheckBox(i18n("Keyboard repeat"), this);
  lay->addWidget(repeatBox);
  connect(repeatBox, SIGNAL(clicked()), this, SLOT(changed()));

  wtstr = i18n("If you check this option, pressing and holding down a key"
     " emits the same character over and over again. For example,"
     " pressing and holding down the Tab key will have the same effect"
     " as that of pressing that key several times in succession:"
     " Tab characters continue to be emitted until you release the key.");
  QWhatsThis::add( repeatBox, wtstr );

  lay->addSpacing(10);
  click = new KIntNumInput(100, this);
  click->setLabel(i18n("Key click volume"));
  click->setRange(0, 100, 10);
  click->setSuffix("%");
  click->setSteps(5,25);
  connect(click, SIGNAL(valueChanged(int)), this, SLOT(changed()));

  lay->addWidget(click);

  wtstr = i18n("If supported, this option allows you to hear audible"
     " clicks from your computer's speakers when you press the keys"
     " on your keyboard. This might be useful if your keyboard"
     " does not have mechanical keys, or if the sound that the keys"
     " make is very soft.<p>"
     " You can change the loudness of the key click feedback"
     " by dragging the slider button or by clicking the up/down"
     " arrows on the spin-button. Setting the volume to 0 % turns off"
     " the key click.");
  QWhatsThis::add( click, wtstr );
  
  numlockGroup = new QVButtonGroup( i18n( "NumLock on KDE startup" ), this );
  ( void ) new QRadioButton( i18n( "Turn on" ), numlockGroup );
  ( void ) new QRadioButton( i18n( "Turn off" ), numlockGroup );
  ( void ) new QRadioButton( i18n( "Leave unchanged" ), numlockGroup );
  connect(numlockGroup, SIGNAL(released(int)), this, SLOT(changed()));
  lay->addWidget( numlockGroup );
#ifndef HAVE_XTEST
  numlockGroup->setDisabled( true );
#endif
  wtstr = i18n("If supported, this option allows you to setup"
     " the state of NumLock after KDE startup.<p> "
     " You can configure NumLock to be turned on or off,"
     " or configure KDE not to set NumLock state." );
  QWhatsThis::add( numlockGroup, wtstr );

  lay->addStretch(10);
  load();
}

int  KeyboardConfig::getClick()
{
    return click->value();
}

// set the slider and LCD values
void KeyboardConfig::setRepeat(int r)
{
    repeatBox->setChecked(r == AutoRepeatModeOn);
}

void KeyboardConfig::setClick(int v)
{
    click->setValue(v);
}

int KeyboardConfig::getNumLockState()
{
    QButton* selected = numlockGroup->selected();
    if( selected == NULL )
        return 2;
    int ret = numlockGroup->id( selected );
    if( ret == -1 )
        ret = 2;
    return ret;
}

void KeyboardConfig::setNumLockState( int s )
{
    numlockGroup->setButton( s );
}

void KeyboardConfig::load()
{
  KConfig *config = new KConfig("kcminputrc");

    XKeyboardState kbd;

    XGetKeyboardControl(kapp->getDisplay(), &kbd);

    config->setGroup("Keyboard");
    bool key = config->readBoolEntry("KeyboardRepeating", true);
    keyboardRepeat = (key ? AutoRepeatModeOn : AutoRepeatModeOff);
    clickVolume = config->readNumEntry("ClickVolume", kbd.key_click_percent);
    numlockState = config->readNumEntry( "NumLock", 2 );

    setClick(kbd.key_click_percent);
    setRepeat(kbd.global_auto_repeat);
    setNumLockState( numlockState );

  delete config;
}

void KeyboardConfig::save()
{
  KConfig *config = new KConfig("kcminputrc");

    XKeyboardControl kbd;

    clickVolume = getClick();
    keyboardRepeat = repeatBox->isChecked() ? AutoRepeatModeOn : AutoRepeatModeOff;
    numlockState = getNumLockState();

    kbd.key_click_percent = clickVolume;
    kbd.auto_repeat_mode = keyboardRepeat;
    XChangeKeyboardControl(kapp->getDisplay(),
                           KBKeyClickPercent | KBAutoRepeatMode,
                           &kbd);

    config->setGroup("Keyboard");
    config->writeEntry("ClickVolume",clickVolume);
    config->writeEntry("KeyboardRepeating", (keyboardRepeat == AutoRepeatModeOn));
    config->writeEntry("NumLock", numlockState );
    config->sync();

  delete config;
}

void KeyboardConfig::defaults()
{
    setClick(50);
    setRepeat(true);
    setNumLockState( 2 );
}

QString KeyboardConfig::quickHelp() const
{
  return i18n("<h1>Keyboard</h1> This module allows you to choose options"
     " for the way in which your keyboard works. The actual effect of"
     " setting these options depends upon the features provided by your"
     " keyboard hardware and the X server on which KDE is running.<p>"
     " For example, you may find that changing the key click volume"
     " has no effect because this feature is not available on your system.");
}

void KeyboardConfig::changed()
{
  emit KCModule::changed(true);
}

/*
 Originally comes from NumLockX http://dforce.sh.cvut.cz/~seli/en/numlockx
 Copyright (C) 2000 Lubos Lunak <l.lunak@email.cz>
*/

#ifdef HAVE_XTEST
int numlockx_get_numlock_state()
    {
    int i;
    int numlock_mask = 0;
    Window dummy1, dummy2;
    int dummy3, dummy4, dummy5, dummy6;
    unsigned int mask;
    XModifierKeymap* map = XGetModifierMapping( qt_xdisplay());
    KeyCode numlock_keycode = XKeysymToKeycode( qt_xdisplay(), XK_Num_Lock );
    if( numlock_keycode == NoSymbol )
        return 0;
    for( i = 0;
         i < 8;
         ++i )
        {
	if( map->modifiermap[ map->max_keypermod * i ] == numlock_keycode )
		numlock_mask = 1 << i;
	}
    XQueryPointer( qt_xdisplay(), DefaultRootWindow( qt_xdisplay()), &dummy1, &dummy2,
        &dummy3, &dummy4, &dummy5, &dummy6, &mask );
    XFreeModifiermap( map );
    return mask & numlock_mask;
    }

void numlockx_change_numlock()
    {
    XTestFakeKeyEvent( qt_xdisplay(), XKeysymToKeycode( qt_xdisplay(), XK_Num_Lock ), True,
        CurrentTime );
    XTestFakeKeyEvent( qt_xdisplay(), XKeysymToKeycode( qt_xdisplay(), XK_Num_Lock ), False,
        CurrentTime );
    }

void numlockx_set_on()
    {
    if( !numlockx_get_numlock_state())
        numlockx_change_numlock();
    }

void numlockx_set_off()
    {
    if( numlockx_get_numlock_state())
        numlockx_change_numlock();
    }

void numlockx_change_numlock_state( bool set_P )
    {
    if( set_P )
        numlockx_set_on();
    else
        numlockx_set_off();
    }
#else
void numlockx_change_numlock_state( bool ) {} // dummy
#endif

#include "keyboard.moc"
