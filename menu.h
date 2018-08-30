/*
 * menu.h: Menu implementation for duplicates plugin.
 *
 * The menu implementation is based on recordings menu in VDR.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef _DUPLICATES_MENU_H
#define _DUPLICATES_MENU_H

#include <vdr/osdbase.h>
#include <vdr/recording.h>
#include <vdr/menuitems.h>
#include <vdr/videodir.h>
#include "config.h"
#include "recording.h"

class cMenuSetupDuplicates;

// --- cMenuDuplicates -------------------------------------------------------

class cMenuDuplicates : public cOsdMenu {
  friend class cMenuSetupDuplicates;
private:
  cStateKey duplicateRecordingsStateKey;
  int helpKeys;
  void SetHelpKeys(void);
  void Set(bool Refresh = false);
  void SetCurrentIndex(int index);
  void Del(int index);
  eOSState Play(void);
  eOSState Setup(void);
  eOSState Delete(void);
  eOSState Info(void);
  eOSState ToggleHidden(void);
public:
  cMenuDuplicates();
  ~cMenuDuplicates();
  virtual eOSState ProcessKey(eKeys Key);
};

// --- cMenuSetupDuplicates --------------------------------------------------

class cMenuSetupDuplicates : public cMenuSetupPage {
private:
  cMenuDuplicates *menuDuplicates;
protected:
  virtual void Store(void);
public:
  cMenuSetupDuplicates(cMenuDuplicates *menuDuplicates = NULL);
  void SetTitle(const char *Title);
};

#endif
