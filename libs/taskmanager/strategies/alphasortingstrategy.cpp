/*****************************************************************

Copyright 2008 Christian Mollekopf <robertknight@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#include "alphasortingstrategy.h"

#include <QMap>
#include <QString>
#include <QtAlgorithms>
#include <QList>

#include <KDebug>

#include "taskitem.h"
#include "taskgroup.h"
#include "taskmanager.h"


namespace TaskManager
{

bool lessThan(const QString &s1, const QString &s2)
{
   // return s1.toLower() < s2.toLower();
    if (s1.localeAwareCompare(s2) < 0) {
        return true;
    }
    return false;
}

void AlphaSortingStrategy::sortItems(ItemList &items)
{
    kDebug();
    QMap<QString, AbstractGroupableItem*> map;

    foreach (AbstractGroupableItem *item, items) {
        if (item->isGroupItem()) {
            map.insertMulti((dynamic_cast<TaskGroup*>(item))->name(), item);
        } else {
            //map.insertMulti((dynamic_cast<TaskItem*>(item))->taskPointer()->visibleName(), item);
            map.insertMulti((dynamic_cast<TaskItem*>(item))->taskPointer()->classClass(), item); //sort by programname not windowname
        }
    }
    items.clear();

    QList <QString> keyList = map.keys();
    qSort(keyList.begin(), keyList.end(), lessThan);

    while (!map.empty()) {
        items.append(map.take(keyList.takeFirst()));
    }

    //For debug
   /* foreach (AbstractGroupableItem *item, items) {
        if (item->isGroupItem()) {
            kDebug() << (dynamic_cast<TaskGroup*>(item))->name();
        } else {
            kDebug() << (dynamic_cast<TaskItem*>(item))->taskPointer()->visibleName();
        }
    }*/

}


GroupManager::TaskSortingStrategy AlphaSortingStrategy::type() const
{
    return GroupManager::AlphaSorting;
}

} //namespace

#include "alphasortingstrategy.moc"

