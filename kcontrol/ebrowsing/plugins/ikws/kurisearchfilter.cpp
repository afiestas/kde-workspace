/*  This file is part of the KDE project
    Copyright (C) 1999 Simon Hausmann <hausmann@kde.org>
    Copyright (C) 2000 Yves Arrouye <yves@realnames.com>
    Copyright (C) 2002, 2003 Dawit Alemayehu <adawit@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <unistd.h>

#include <kdebug.h>
#include <klocale.h>
#include <kinstance.h>
#include <kglobal.h>

#include "ikwsopts.h"
#include "kuriikwsfiltereng.h"
#include "kurisearchfilter.h"

typedef KGenericFactory<KURISearchFilter> KURISearchFilterFactory;
K_EXPORT_COMPONENT_FACTORY(libkurisearchfilter, KURISearchFilterFactory("kcmkurifilt"))

KURISearchFilter::KURISearchFilter(QObject *parent, const char *name,
                                   const QStringList &)
                 :KURIFilterPlugin(parent, name ? name : "kurisearchfilter", 1.0),
                  DCOPObject("KURISearchFilterIface")
{
}

KURISearchFilter::~KURISearchFilter()
{
}

void KURISearchFilter::configure()
{
  kdDebug() << "KURISearchFilter::configure: Config reload request..." << endl;
  KURISearchFilterEngine::self()->loadConfig();
}

bool KURISearchFilter::filterURI( KURIFilterData &data ) const
{
  if ( KURISearchFilterEngine::self()->verbose() )
    kdDebug() << "KURISearchFilter::filterURI: " << data.typedString() << endl;

  QString result = KURISearchFilterEngine::self()->webShortcutQuery( data.typedString() );

  if ( !result.isEmpty() )
  {
    setFilteredURI( data, KURL( result ) );
    setURIType( data, KURIFilterData::NET_PROTOCOL );
    return true;
  }

  return false;
}

KCModule *KURISearchFilter::configModule(QWidget *parent, const char *) const
{
  return new FilterOptions( KURISearchFilterFactory::instance(), parent, "kcmkurifilt");
}

QString KURISearchFilter::configName() const
{
  return i18n("Search F&ilters");
}

#include "kurisearchfilter.moc"
