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

class cMenuDuplicateItem;

// --- cMenuDuplicates -------------------------------------------------------

class cMenuDuplicates : public cOsdMenu {
private:
  int recordingsState;
  int helpKeys;
  void SetHelpKeys(void);
  void Set(bool Refresh = false);
  eOSState Delete(void);
  eOSState Info(void);
protected:
  cRecording *GetRecording(cMenuDuplicateItem *Item);
public:
  cMenuDuplicates();
  ~cMenuDuplicates();
  virtual eOSState ProcessKey(eKeys Key);
  };

#endif
