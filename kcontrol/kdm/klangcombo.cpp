/*
 * klangcombo.cpp - A combobox to select a language
 *
 * Copyright (c) 1998 Matthias Hoelzer (hoelzer@physik.uni-wuerzburg.de)
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


#include <qpainter.h>
#include <qpixmap.h>
#include <kiconloader.h>
#include <kapp.h>
#include <klocale.h>
#include <ksimpleconfig.h>
#include <kstddirs.h>

#include "klangcombo.h"
#include "klangcombo.moc"


KLanguageCombo::~KLanguageCombo ()
{
}


KLanguageCombo::KLanguageCombo (QWidget * parent, const char *name)
  : QComboBox(parent, name)
{
  loadLanguageList();
}


void KLanguageCombo::insertLanguage(const QString& lang)
{
  QPainter p;

  QString output = language(lang) + " ("+tag(lang)+")";

  int w = fontMetrics().width(output) + 24;
  QPixmap pm(w, 16);

  QPixmap flag(locate("locale", tag(lang) + "/flag.png"));
  pm.fill(colorGroup().background());
  p.begin(&pm);

  p.drawText(24,1,w-24,16,AlignLeft | AlignTop,output);
  if (!flag.isNull())
    p.drawPixmap(1,1,flag);
  p.end();
 
  insertItem(pm);
}


QString KLanguageCombo::getLanguage()
{
  return *tags.at(currentItem());
}


void KLanguageCombo::setLanguage(const QString& tag)
{
  int index = tags.findIndex(tag);

  if (index<0)
    index=0;
  setCurrentItem(index);
}


QString KLanguageCombo::tag(const QString& lang)
{
  QString tag(lang);

  int pos = tag.find(";");
  if (pos == -1)
    return "";
  else
    return tag.left(pos);
}


QString KLanguageCombo::language(const QString& lang)
{
  QString name(lang);

  int pos = name.find(";");
  if (pos == -1)
    return name;
  else
    return name.right(name.length()-pos-1);
}

#ifdef __GNUC__
#warning TODO: kdmconfig is massivly broken with KStandardDirs ;(
#endif

void KLanguageCombo::loadLanguageList()
{
  KSimpleConfig config(locate("config", "kcmlocalerc"));
  QString name;

  clear();
  languages.clear();
  tags.clear();

  config.setGroup("KCM Locale");
  tags = config.readListEntry("Languages");

  for ( QStringList::Iterator it = tags.begin(); it != tags.end(); ++it )
    {
      config.setGroup(*it);
      name = config.readEntry("Name");
      if (!name.isEmpty())
        languages.append(name);
      else
        languages.append(i18n("without name!"));
             
     insertLanguage(*it+";"+name);

     if (*it == "C")
       setCurrentItem(count()-1);
    }
}

