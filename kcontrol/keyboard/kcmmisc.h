/*
 * keyboard.h
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __KCMMISC_H__
#define __KCMMISC_H__

#include <QtCore/QString>
#include <QtGui/QWidget>

class Ui_KeyboardConfigWidget;

class KCMiscKeyboardWidget : public QWidget
{
  Q_OBJECT
public:
  KCMiscKeyboardWidget(QWidget *parent);
  ~KCMiscKeyboardWidget();

  void save();
  void load();
  void defaults();

  QString quickHelp() const;

private slots:
  void changed();

  void delaySliderChanged (int value);
  void delaySpinboxChanged (int value);
  void rateSliderChanged (int value);
  void rateSpinboxChanged (double value);

Q_SIGNALS:
	void changed(bool state);

private:
  void setClick( int );
  void setRepeat( int flag, int delay, double rate);
  void setRepeatRate( int );
  void setNumLockState( int );

  int getClick();
  int getRepeatRate();
  int getNumLockState();

  int sliderMax;
  int clickVolume, keyboardRepeat;
  int numlockState; // 0 = on, 1 = off, 2 = don't change
  Ui_KeyboardConfigWidget& ui;
};

#endif

