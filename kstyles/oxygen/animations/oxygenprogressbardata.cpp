//////////////////////////////////////////////////////////////////////////////
// oxygenprogressbar.cpp
// data container for progressbar animations
// -------------------
//
// Copyright (c) 2009 Hugo Pereira Da Costa <hugo@oxygen-icons.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "oxygenprogressbardata.h"
#include "oxygenprogressbardata.moc"

#include <QtGui/QProgressBar>
#include <cassert>

namespace Oxygen
{

    //______________________________________________
    ProgressBarData::ProgressBarData( QObject* parent, QWidget* target, int duration ):
        AnimationData( parent, target ),
        animation_( new Animation( duration, this ) ),
        ratio_(0),
        startValue_(0),
        endValue_(0)
    {
        setupAnimation( animation_, "ratio" );
        animation().data()->setCurveShape( QTimeLine::EaseInOutCurve );

        QProgressBar* progress = qobject_cast<QProgressBar*>( target );
        assert( progress );
        setStartValue( progress->value() );
        setEndValue( progress->value() );

        connect( target, SIGNAL( valueChanged( int ) ), SLOT( valueChanged( int ) ) );
    }

    //______________________________________________
    void ProgressBarData::valueChanged( int value )
    {

        // do nothing if not enabled
        if( !enabled() ) return;

        // do nothing if progress is invalid
        QProgressBar* progress = static_cast<QProgressBar*>( target().data() );
        if( !( progress && progress->maximum() != progress->minimum() ) ) return;

        // update start and end values
        bool isRunning( animation().data()->isRunning() );
        if( !isRunning ) setStartValue( endValue() );
        setEndValue( value );
        if( !isRunning ) animation().data()->start();

    }

}
