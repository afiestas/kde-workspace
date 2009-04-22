/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Michael Zanetti <michael_zanetti@gmx.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWIN_SLIDEBACK_H
#define KWIN_SLIDEBACK_H

// Include with base class for effects.
#include <kwineffects.h>

namespace KWin
{

class SlideBackEffect
            : public Effect
    {
    public:
        SlideBackEffect();

        virtual void windowActivated( EffectWindow* c );

        virtual void prePaintWindow( EffectWindow *w, WindowPrePaintData &data, int time );
        virtual void paintWindow( EffectWindow* w, int mask, QRegion region, WindowPaintData& data );
        virtual void postPaintWindow( EffectWindow* w );

        virtual void prePaintScreen( ScreenPrePaintData &data, int time );
        virtual void postPaintScreen();

        virtual void windowDeleted( EffectWindow* w );
        virtual void windowAdded( EffectWindow* w );

        virtual void tabBoxClosed();

    private:

        WindowMotionManager motionManager;
        EffectWindowList usableOldStackingOrder;
        EffectWindowList oldStackingOrder;
        EffectWindowList coveringWindows;
        EffectWindowList elevatedList;
        QHash<EffectWindow *, QRect> destinationList;
        bool disabled;

        QRect getSlideDestination( const QRect &windowUnderGeometry, const QRect &windowOverGeometry);
        void updateStackingOrder();
        bool isWindowOnTop( EffectWindow *w );
        bool isWindowUsable( EffectWindow *w );
        bool stackingOrderChanged();
        bool intersects( EffectWindow *windowUnder, const QRect &windowOverGeometry );
        EffectWindowList usableWindows( const EffectWindowList &allWindows );
        EffectWindow *newTopWindow();

    };

} // namespace

#endif
