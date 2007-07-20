/*
 *   Copyright (C) 2007 Aaron Seigo <aseigo@kde.org>
 *                 2007 Alexis Ménard <darktears31@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "phase.h"

#include <QGraphicsItem>

#include <KConfig>
#include <KConfigGroup>
#include <KService>
#include <KServiceTypeTrader>

#include "animator.h"

namespace Plasma
{


struct AnimationState
{
    QGraphicsItem* item;
    Phase::Animation animation;
    Phase::CurveShape curve;
    int interval;
    int currentInterval;
    int frames;
    int currentFrame;
};

struct ElementAnimationState
{
    QGraphicsItem* item;
    Phase::CurveShape curve;
    Phase::ElementAnimation animation;
    int interval;
    int currentInterval;
    int frames;
    int currentFrame;
    int id;
    QPixmap pixmap;
};

struct MovementState
{
    QGraphicsItem* item;
    Phase::CurveShape curve;
    Phase::Movement movement;
    int interval;
    int currentInterval;
    int frames;
    int currentFrame;
    QPoint destination;
};

class Phase::Private
{
    public:

        Private()
            : animator(0),
              animId(0),
              timerId(0)
        {
        }

        ~Private()
        {
            qDeleteAll(animatedItems);
            qDeleteAll(animatedElements);
            // Animator is a QObject
            // and we don't own the items
        }

        void performAnimation(qreal amount, const AnimationState* state)
        {
            switch (state->animation) {
                case Phase::Appear:
                    animator->appear(amount, state->item);
                    break;
                case Phase::Disappear:
                    animator->disappear(amount, state->item);
                    break;
                case Phase::Activate:
                    animator->activate(amount, state->item);
                    break;
                case Phase::FrameAppear:
                    animator->frameAppear(amount, state->item, QRegion()); //FIXME: what -is- the frame region?
                    break;
            }
        }
          void performMovement(qreal amount, const MovementState* state)
        {
            switch (state->movement) {
                case Phase::SlideIn:
                    animator->slideIn(amount, state->item, state->destination);
                    break;
                case Phase::SlideOut:
                    animator->slideOut(amount, state->item,state->destination);
                    break;
            }
        }
        Animator* animator;
        int animId;
        int timerId;
        QTime time;

        //TODO: eventually perhaps we should allow multiple animations simulataneously
        //      which would imply changing this to a QMap<QGraphicsItem*, QList<QTimeLine*> >
        //      and really making the code fun ;)
        QMap<QGraphicsItem*, AnimationState*> animatedItems;
        QMap<QGraphicsItem*, MovementState*> movingItems;
        QMap<Phase::AnimId, ElementAnimationState*> animatedElements;
};

class PhaseSingleton
{
    public:
        Phase self;
};

K_GLOBAL_STATIC( PhaseSingleton, privateSelf )

Phase* Phase::self()
{
    return &privateSelf->self;
}


Phase::Phase(QObject * parent)
    : QObject(parent),
      d(new Private)
{
    init();
}

Phase::~Phase()
{
    delete d;
}

void Phase::appletDestroyed(QObject* o)
{
    QGraphicsItem* item = dynamic_cast<QGraphicsItem*>(o);

    if (!item) {
        return;
    }

    QMap<QGraphicsItem*, AnimationState*>::iterator it = d->animatedItems.find(item);
    if (it != d->animatedItems.end()) {
        delete it.value();
        d->animatedItems.erase(it);
        return;
    }

    QMap<QGraphicsItem*, MovementState*>::iterator it2 = d->movingItems.find(item);
    if (it2 != d->movingItems.end()) {
        delete it2.value();
        d->movingItems.erase(it2);
    }
}

void Phase::animateItem(QGraphicsItem* item, Animation animation)
{
     //kDebug()<<k_funcinfo<<endl;
    // get rid of any existing animations on this item.
    //TODO: shoudl we allow multiple anims per item?
    QMap<QGraphicsItem*, AnimationState*>::iterator it = d->animatedItems.find(item);
    if (it != d->animatedItems.end()) {
        delete it.value();
        d->animatedItems.erase(it);
    }

    int frames = d->animator->framesPerSecond(animation);

    if (frames < 1) {
        // evidently this animator doesn't have an implementation
        // for this Animation
        return;
    }

    AnimationState* state = new AnimationState;
    state->item = item;
    state->animation = animation;
    state->curve = d->animator->curve(animation);
    //TODO: variance in times based on the value of animation
    state->frames = frames / 3;
    state->currentFrame = 0;
    state->interval = 333 / state->frames;
    state->interval = (state->interval / 40) * 40;
    state->currentInterval = state->interval;

    d->animatedItems[item] = state;
    d->performAnimation(0, state);

    if (!d->timerId) {
        d->timerId = startTimer(40);
        d->time.restart();
    }
}

void Phase::moveItem(QGraphicsItem* item, Movement movement, const QPoint &destination)
{
     //kDebug()<<k_funcinfo<<endl;
     QMap<QGraphicsItem*, MovementState*>::iterator it = d->movingItems.find(item);
     if (it != d->movingItems.end()) {
          delete it.value();
          d->movingItems.erase(it);
     }

     int frames = d->animator->framesPerSecond(movement);
     if (frames < 1) {
          // evidently this animator doesn't have an implementation
          // for this Animation
          return;
     }

     MovementState* state = new MovementState;
     state->destination=destination;
     state->item = item;
     state->movement = movement;
     state->curve = d->animator->curve(movement);
     //TODO: variance in times based on the value of animation
     state->frames = frames / 2;
     state->currentFrame = 0;
     state->interval = 500 / state->frames;
     state->interval = (state->interval / 40) * 40;
     state->currentInterval = state->interval;

     d->movingItems[item] = state;
     d->performMovement(0, state);

     if (!d->timerId) {
          d->timerId = startTimer(40);
          d->time.restart();
     }
}

void Phase::render(QGraphicsItem* item, QImage& image, RenderOp op)
{
    Q_UNUSED(item);
    switch (op) {
        case RenderBackground:
            d->animator->renderBackground(image);
            break;
    }
}

Phase::AnimId Phase::animateElement(QGraphicsItem *item, ElementAnimation animation)
{
    //kDebug() << "startElementAnimation(AnimId " << animation << ")" << endl;
    ElementAnimationState *state = new ElementAnimationState;
    state->item = item;
    state->curve = d->animator->curve(animation);
    state->animation = animation;
    //TODO: variance in times based on the value of animation
    state->frames = d->animator->framesPerSecond(animation) / 5;
    state->currentFrame = 0;
    state->interval = 200 / state->frames;
    state->interval = (state->interval / 40) * 40;
    state->currentInterval = state->interval;
    state->id = ++d->animId;

    //kDebug() << "animateElement " << animation << ", interval: " << state->interval << ", frames: " << state->frames << endl;
    bool needTimer = true;
    if (state->frames < 1) {
        state->frames = 1;
        state->currentFrame = 1;
        needTimer = false;
    }

    d->animatedElements[state->id] = state;
    state->item->update();

    //kDebug() << "startElementAnimation(AnimId " << animation << ") returning " << state->id << endl;
    if (needTimer && !d->timerId) {
        // start a 20fps timer;
        //TODO: should be started at the maximum frame rate needed only?
        d->timerId = startTimer(40);
        d->time.restart();
    }
    return state->id;
}

void Phase::stopElementAnimation(AnimId id)
{
    QMap<AnimId, ElementAnimationState*>::iterator it = d->animatedElements.find(id);
    if (it != d->animatedElements.end()) {
        delete it.value();
        d->animatedElements.erase(it);
    }
    //kDebug() << "stopElementAnimation(AnimId " << id << ") done" << endl;
}

void Phase::setAnimationPixmap(AnimId id, const QPixmap &pixmap)
{
    QMap<AnimId, ElementAnimationState*>::iterator it = d->animatedElements.find(id);

    if (it == d->animatedElements.end()) {
        kDebug() << "Phase::setAnimationPixmap(" << id << ") found no entry for it!" << endl;
        return;
    }

    it.value()->pixmap = pixmap;
}

QPixmap Phase::animationResult(AnimId id)
{
    QMap<AnimId, ElementAnimationState*>::const_iterator it = d->animatedElements.find(id);

    if (it == d->animatedElements.constEnd()) {
        //kDebug() << "Phase::animationResult(" << id << ") found no entry for it!" << endl;
        return QPixmap();
    }

    ElementAnimationState* state = it.value();
    qreal progress = state->frames;
    //kDebug() << "Phase::animationResult(" << id <<   " at " << progress  << endl;
    progress = state->currentFrame / progress;
    progress = qMin(1.0, qMax(0.0, progress));
    //kDebug() << "Phase::animationResult(" << id <<   " at " << progress  << endl;

    switch (state->animation) {
        case ElementAppear:
            return d->animator->elementAppear(progress, state->pixmap);
            break;
        case ElementDisappear:
            return d->animator->elementDisappear(progress, state->pixmap);
            break;
    }

    return state->pixmap;
}

void Phase::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    bool animationsRemain = false;
    int elapsed = 40;
    if (d->time.elapsed() > elapsed) {
        elapsed = d->time.elapsed();
    }
    d->time.restart();
    //kDebug() << "timeEvent, elapsed time: " << elapsed << endl;

    foreach (AnimationState* state, d->animatedItems) {
        if (state->currentInterval <= elapsed) {
            // we need to step forward!
            state->currentFrame += qMax(1, elapsed / state->interval);

            if (state->currentFrame < state->frames) {
                qreal progress = state->frames;
                progress = state->currentFrame / progress;
                progress = qMin(1.0, qMax(0.0, progress));
                d->performAnimation(progress, state);
                state->currentInterval = state->interval;
                //TODO: calculate a proper interval based on the curve
                state->interval *= 1 - progress;
                animationsRemain = true;
            } else {
                d->performAnimation(1, state);
                d->animatedItems.erase(d->animatedItems.find(state->item));
                delete state;
            }
        } else {
            state->currentInterval -= elapsed;
            animationsRemain = true;
        }
    }

    foreach (MovementState* state, d->movingItems) {
        if (state->currentInterval <= elapsed) {
            // we need to step forward!
            state->currentFrame += qMax(1, elapsed / state->interval);

            if (state->currentFrame < state->frames) {
                qreal progress = state->frames;
                progress = state->currentFrame / progress;
                progress = qMin(1.0, qMax(0.0, progress));
                //kDebug()<<progress<<endl;
                d->performMovement(progress, state);
                state->currentInterval = state->interval;
                animationsRemain = true;
            } else {
                d->performMovement(1, state);
                d->movingItems.erase(d->movingItems.find(state->item));
                emit movementComplete(state->item);
                delete state;
            }
        } else {
            state->currentInterval -= elapsed;
            animationsRemain = true;
        }
    }

    foreach (ElementAnimationState* state, d->animatedElements) {
        if (state->currentFrame == state->frames) {
            // since we keep element animations around until they are
            // removed, we will end up with finished animations in the queue;
            // just skip them
            //TODO: should we move them to a separate QMap?
            continue;
        }

        if (state->currentInterval <= elapsed) {
            // we need to step forward!
/*            kDebug() << "stepping forwards element anim " << state->id << " from " << state->currentFrame
                    << " by " << qMax(1, elapsed / state->interval) << " to "
                    << state->currentFrame + qMax(1, elapsed / state->interval) << endl;*/
            state->currentFrame += qMax(1, elapsed / state->interval);
            state->item->update();

            if (state->currentFrame < state->frames) {
                state->currentInterval = state->interval;
                //TODO: calculate a proper interval based on the curve
                qreal progress = state->frames;
                progress = state->currentFrame / progress;
                progress = qMin(1.0, qMax(0.0, progress));
                state->interval *= 1 - progress;
                animationsRemain = true;
            }
        } else {
            state->currentInterval -= elapsed;
            animationsRemain = true;
        }
    }

    if (!animationsRemain && d->timerId) {
        killTimer(d->timerId);
        d->timerId = 0;
    }
}

void Phase::init()
{
    KConfig c("plasmarc");
    KConfigGroup cg(&c, "Phase");
    QString pluginName = cg.readEntry("animator", "default");

    if (!pluginName.isEmpty()) {
        QString constraint = QString("[X-KDE-PluginInfo-Name] == '%1'").arg(pluginName);
        KService::List offers = KServiceTypeTrader::self()->query("Plasma/Animator", constraint);

        if (!offers.isEmpty()) {
             d->animator = KService::createInstance<Plasma::Animator>(offers.first(), 0, QStringList());
        }
    }

    if (!d->animator) {
        d->animator = new Animator(this);
    }
}

} // namespace Plasma

#include <phase.moc>
