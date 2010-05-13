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

#include <config-workspace.h>
#include <config-X11.h>

#include <kconfiggroup.h>
#include <kapplication.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kprocess.h>

#include <X11/Xlib.h>

#include <math.h>


/*
 Originally comes from NumLockX http://dforce.sh.cvut.cz/~seli/en/numlockx

 NumLockX

 Copyright (C) 2000-2001 Lubos Lunak        <l.lunak@kde.org>
 Copyright (C) 2001      Oswald Buddenhagen <ossi@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/

//#include <X11/Xlib.h>

#ifdef HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif

#ifdef HAVE_XKB
#define explicit myexplicit
#include <X11/XKBlib.h>
#undef explicit
#endif

#include <X11/keysym.h>

#if defined(HAVE_XTEST) || defined(HAVE_XKB)

/* the XKB stuff is based on code created by Oswald Buddenhagen <ossi@kde.org> */
#ifdef HAVE_XKB
int xkb_init()
    {
    int xkb_opcode, xkb_event, xkb_error;
    int xkb_lmaj = XkbMajorVersion;
    int xkb_lmin = XkbMinorVersion;
    return XkbLibraryVersion( &xkb_lmaj, &xkb_lmin )
			&& XkbQueryExtension( QX11Info::display(), &xkb_opcode, &xkb_event, &xkb_error,
			       &xkb_lmaj, &xkb_lmin );
    }

unsigned int xkb_mask_modifier( XkbDescPtr xkb, const char *name )
    {
    int i;
    if( !xkb || !xkb->names )
	return 0;
    for( i = 0;
         i < XkbNumVirtualMods;
	 i++ )
	{
	char* modStr = XGetAtomName( xkb->dpy, xkb->names->vmods[i] );
	if( modStr != NULL && strcmp(name, modStr) == 0 )
	    {
	    unsigned int mask;
	    XkbVirtualModsToReal( xkb, 1 << i, &mask );
	    return mask;
	    }
	}
    return 0;
    }

unsigned int xkb_numlock_mask()
    {
    XkbDescPtr xkb;
    if(( xkb = XkbGetKeyboard( QX11Info::display(), XkbAllComponentsMask, XkbUseCoreKbd )) != NULL )
	{
        unsigned int mask = xkb_mask_modifier( xkb, "NumLock" );
        XkbFreeKeyboard( xkb, 0, True );
        return mask;
        }
    return 0;
    }

int xkb_set_on()
    {
    unsigned int mask;
    if( !xkb_init())
        return 0;
    mask = xkb_numlock_mask();
    if( mask == 0 )
        return 0;
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, mask, mask);
    return 1;
    }

int xkb_set_off()
    {
    unsigned int mask;
    if( !xkb_init())
        return 0;
    mask = xkb_numlock_mask();
    if( mask == 0 )
        return 0;
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, mask, 0);
    return 1;
    }
#endif

#ifdef HAVE_XTEST
int xtest_get_numlock_state()
    {
    int i;
    int numlock_mask = 0;
    Window dummy1, dummy2;
    int dummy3, dummy4, dummy5, dummy6;
    unsigned int mask;
    KeyCode numlock_keycode = XKeysymToKeycode( QX11Info::display(), XK_Num_Lock );
    if( numlock_keycode == NoSymbol )
        return 0;
    XModifierKeymap* map = XGetModifierMapping( QX11Info::display() );
    for( i = 0;
         i < 8;
         ++i )
        {
	if( map->modifiermap[ map->max_keypermod * i ] == numlock_keycode )
		numlock_mask = 1 << i;
	}
    XQueryPointer( QX11Info::display(), DefaultRootWindow( QX11Info::display() ), &dummy1, &dummy2,
        &dummy3, &dummy4, &dummy5, &dummy6, &mask );
    XFreeModifiermap( map );
    return mask & numlock_mask;
    }

void xtest_change_numlock()
    {
    XTestFakeKeyEvent( QX11Info::display(), XKeysymToKeycode( QX11Info::display(), XK_Num_Lock ), True, CurrentTime );
    XTestFakeKeyEvent( QX11Info::display(), XKeysymToKeycode( QX11Info::display(), XK_Num_Lock ), False, CurrentTime );
    }

void xtest_set_on()
    {
    if( !xtest_get_numlock_state())
        xtest_change_numlock();
    }

void xtest_set_off()
    {
    if( xtest_get_numlock_state())
        xtest_change_numlock();
    }
#endif

void numlock_set_on()
    {
#ifdef HAVE_XKB
    if( xkb_set_on())
        return;
#endif
#ifdef HAVE_XTEST
    xtest_set_on();
#endif
    }

void numlock_set_off()
    {
#ifdef HAVE_XKB
    if( xkb_set_off())
        return;
#endif
#ifdef HAVE_XTEST
    xtest_set_off();
#endif
    }

void numlockx_change_numlock_state( bool set_P )
    {
    if( set_P )
        numlock_set_on();
    else
        numlock_set_off();
    }
#else
void numlockx_change_numlock_state( bool ) {} // dummy
#endif // defined(HAVE_XTEST) || defined(HAVE_XKB)


// This code is taken from xset utility from XFree 4.3 (http://www.xfree86.org/)


#if 0
//HAVE_XF86MISC
#include <X11/extensions/xf86misc.h>
void set_repeatrate(int delay, double rate)
{
  Display* dpy = QX11Info::display();
  XF86MiscKbdSettings values;

  XF86MiscGetKbdSettings(dpy, &values);
  values.delay = delay;
  values.rate = rate;
  XF86MiscSetKbdSettings(dpy, &values);
  return;
}
#else
void set_repeatrate(int delay, double rate)
{
#if HAVE_XKB
  Display* dpy = QX11Info::display();
  int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
  int xkbopcode, xkbevent, xkberror;

  if (XkbQueryExtension(dpy, &xkbopcode, &xkbevent, &xkberror, &xkbmajor,
				&xkbminor)) {
     XkbDescPtr xkb = XkbAllocKeyboard();
     if (xkb) {
        int res = XkbGetControls(dpy, XkbRepeatKeysMask, xkb);
        xkb->ctrls->repeat_delay = delay;
        xkb->ctrls->repeat_interval = (int)floor(1000/rate + 0.5);
        res = XkbSetControls(dpy, XkbRepeatKeysMask, xkb);
        return;
     }
  }
#endif
  // Fallback: use the xset utility.

  // Unfortunately xset does only support int parameters, so
  // really slow repeat rates cannot be supported this way.
  // (the FSG Accessibility standard requires support for repeat rates
  // of several seconds per character)
  int r;
  if (rate < 1)
     r = 1;
  else
     r = (int)floor(rate + 0.5);

  QString exe = KGlobal::dirs()->findExe("xset");
  if (exe.isEmpty())
    return;

  KProcess p;
  p << exe << "r" << "rate" << QString::number(delay) << QString::number(r);
  p.execute();
}
#endif

void init_keyboard_hardware()
{
    KConfigGroup config(KSharedConfig::openConfig( "kcminputrc" ), "Keyboard");

	XKeyboardState   kbd;
	XKeyboardControl kbdc;

	XGetKeyboardControl(QX11Info::display(), &kbd);
	bool key = config.readEntry("KeyboardRepeating", true);
	kbdc.key_click_percent = config.readEntry("ClickVolume", kbd.key_click_percent);
	kbdc.auto_repeat_mode = (key ? AutoRepeatModeOn : AutoRepeatModeOff);

	XChangeKeyboardControl(QX11Info::display(),
						   KBKeyClickPercent | KBAutoRepeatMode,
						   &kbdc);

	if( key ) {
		int delay_ = config.readEntry("RepeatDelay", 250);
		double rate_ = config.readEntry("RepeatRate", 30.);
		set_repeatrate(delay_, rate_);
	}


	int numlockState = config.readEntry( "NumLock", 2 );
	if( numlockState != 2 )
		numlockx_change_numlock_state( numlockState == 0 );
}
