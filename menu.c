/*
 * menu.c: Menu implementation for duplicates plugin.
 *
 * The menu implementation is based on recordings menu in VDR.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include "menu.h"
#include "visibility.h"
#include <vdr/menu.h>
#include <vdr/status.h>
#include <vdr/interface.h>
#include <vdr/svdrp.h>

static inline cOsdItem *SeparatorItem(const char *Label) {
  cOsdItem *Item = new cOsdItem(cString::sprintf("----- %s -----", Label));
  Item->SetSelectable(false);
  return Item;
}

// --- cDuplicatesReplayControl -------------------------------------------------------

class cDuplicatesReplayControl : public cReplayControl {
public:
  virtual eOSState ProcessKey(eKeys Key);
};

eOSState cDuplicatesReplayControl::ProcessKey(eKeys Key) {
  eOSState state = cReplayControl::ProcessKey(Key);
  if (state == osRecordings) {
     cControl::Shutdown();
     cRemote::CallPlugin("duplicates");
     return osContinue;
  }
  return state;
}

// --- cMenuDuplicate --------------------------------------------------------

class cMenuDuplicate : public cOsdMenu {
private:
  const cRecording *recording;
public:
  cMenuDuplicate(const cRecording *Recording);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuDuplicate::cMenuDuplicate(const cRecording *Recording)
:cOsdMenu(trVDR("Recording info")) {
  SetMenuCategory(mcRecording);
  recording = Recording;
  SetHelp(trVDR("Button$Play"));
}

void cMenuDuplicate::Display(void) {
  cOsdMenu::Display();
  DisplayMenu()->SetRecording(recording);
  if (recording->Info()->Description())
     cStatus::MsgOsdTextItem(recording->Info()->Description());
}

eOSState cMenuDuplicate::ProcessKey(eKeys Key)
{
  switch (int(Key)) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft);
                  return osContinue;
    case kInfo:   return osBack;
    default: break;
}

  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kPlay:
       case kRed:    cRemote::Put(Key, true);
       case kOk:     return osBack;
       default: break;
     }
  }
  return state;
}

// --- cMenuDuplicateItem ----------------------------------------------------

class cMenuDuplicateItem : public cOsdItem {
private:
  std::string fileName;
  cVisibility visibility;
public:
  cMenuDuplicateItem(cDuplicateRecording *DuplicateRecording);
  const char *FileName(void) { return fileName.c_str(); }
  cVisibility Visibility() { return visibility; }
};

cMenuDuplicateItem::cMenuDuplicateItem(cDuplicateRecording *DuplicateRecording) : visibility(DuplicateRecording->Visibility()) {
  fileName = DuplicateRecording->FileName();
  SetText(DuplicateRecording->Text());
}

// --- cMenuDuplicates -------------------------------------------------------

cMenuDuplicates::cMenuDuplicates()
:cOsdMenu(tr("Duplicate recordings"), 9, 7, 7)
{
  SetMenuCategory(mcRecording);
  helpKeys = -1;
  Set();
  Display();
  SetHelpKeys();
}

cMenuDuplicates::~cMenuDuplicates() {
  helpKeys = -1;
}

void cMenuDuplicates::SetHelpKeys(void) {
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  int NewHelpKeys = 0;
  if (ri) {
    NewHelpKeys = 1;
    if (ri->Visibility().Read() == HIDDEN)
      NewHelpKeys = 2;
  }
  if (NewHelpKeys != helpKeys) {
    switch (NewHelpKeys) {
      case 0: SetHelp(NULL); break;
      case 1:
      case 2: SetHelp(trVDR("Button$Play"), trVDR("Setup"), trVDR("Button$Delete"), NewHelpKeys == 1 ? tr("Hide") : tr("Unhide"));
      default: ;
    }
    helpKeys = NewHelpKeys;
  }
}

void cMenuDuplicates::Set(bool Refresh) {
  if (DuplicateRecordings.Lock(duplicateRecordingsStateKey)) {
    dsyslog("duplicates: %s menu.", Refresh ? "Refreshing" : "Creating");
    const char *CurrentRecording = NULL;
    int currentIndex = -1;
    if (Refresh)
      currentIndex = Current();
    else
      CurrentRecording = cReplayControl::LastReplayed();
    Clear();
    for (cDuplicateRecording *Duplicates = DuplicateRecordings.First(); Duplicates; Duplicates = DuplicateRecordings.Next(Duplicates)) {
      Add(SeparatorItem(Duplicates->Text()));
      for (cDuplicateRecording *Duplicate = Duplicates->Duplicates()->First(); Duplicate; Duplicate = Duplicates->Duplicates()->Next(Duplicate)) {
        cMenuDuplicateItem *Item = new cMenuDuplicateItem(Duplicate);
        Add(Item);
        if (CurrentRecording && strcmp(CurrentRecording, Item->FileName()) == 0)
          SetCurrent(Item);
      }
    }
    duplicateRecordingsStateKey.Remove();
    if (Count() == 0)
      Add(SeparatorItem(cString::sprintf(tr("%d duplicate recordings"), 0)));
    if (Refresh) {
      SetCurrentIndex(currentIndex);
      Display();
    }
  }
}

static bool HandleRemoteModifications(cTimer *NewTimer, cTimer *OldTimer = NULL) {
  cString ErrorMessage;
  if (!HandleRemoteTimerModifications(NewTimer, OldTimer, &ErrorMessage)) {
    Skins.Message(mtError, ErrorMessage);
    return false;
  }
  return true;
}

static bool TimerStillRecording(const char *FileName) {
  if (cRecordControl *rc = cRecordControls::GetRecordControl(FileName)) {
    // local timer
    if (Interface->Confirm(trVDR("Timer still recording - really delete?"))) {
      LOCK_TIMERS_WRITE;
      if (cTimer *Timer = rc->Timer()) {
        Timer->Skip();
        cRecordControls::Process(Timers, time(NULL));
        if (Timer->IsSingleEvent()) {
          Timers->Del(Timer);
          isyslog("deleted timer %s", *Timer->ToDescr());
        }
        Timers->SetModified();
      }
    } else
      return true; // user didn't confirm deletion
  } else {
    // remote timer
    cString TimerId = GetRecordingTimerId(FileName);
    if (*TimerId) {
      int Id;
      char *RemoteBuf = NULL;
      cString Remote;
      if (2 == sscanf(TimerId, "%d@%m[^ \n]", &Id, &RemoteBuf)) {
        Remote = RemoteBuf;
        free(RemoteBuf);
        if (Interface->Confirm(trVDR("Timer still recording - really delete?"))) {
          LOCK_TIMERS_WRITE;
          if (cTimer *Timer = Timers->GetById(Id, Remote)) {
            cTimer OldTimer = *Timer;
            Timer->Skip();
            Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
            if (Timer->IsSingleEvent()) {
              if (HandleRemoteModifications(NULL, Timer))
                Timers->Del(Timer);
              else
                return true; // error while deleting remote timer
            } else if (!HandleRemoteModifications(Timer, &OldTimer))
                return true; // error while modifying remote timer
          }
        } else
          return true; // user didn't confirm deletion
      }
    }
  }
  return false;
}

void cMenuDuplicates::SetCurrentIndex(int index) {
  if (index >= 0) {
    if (index >= Count())
      index = Count() - 1;
    cOsdItem *current = Get(index);
    while (current) {
      if (current->Selectable()) {
        SetCurrent(current);
        break;
      }
      current = Prev(current);
    }
  }
}

void cMenuDuplicates::Del(int index) {
  cOsdMenu::Del(index);
  // remove items that have less than 2 duplicates
  int d = 0;
  for (int i = Count() - 1; i >= 0; i--) {
    if (!SelectableItem(i)) {
      if (d < 2) {
        for (int j = 0; j <= d; j++) {
          cOsdMenu::Del(i);
        }
      }
      d = 0;
    } else
      d++;
  }
  SetCurrentIndex(index);
}

eOSState cMenuDuplicates::Delete(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  if (cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current())) {
    const char *FileName = ri->FileName();
    if (Interface->Confirm(trVDR("Delete recording?"))) {
      if (TimerStillRecording(FileName))
        return osContinue;
      {
        LOCK_RECORDINGS_READ
        if (const cRecording *Recording = Recordings->GetByName(FileName)) {
          FileName = Recording->FileName();
          if (RecordingsHandler.GetUsage(FileName)) {
            if (!Interface->Confirm(trVDR("Recording is being edited - really delete?")))
              return osContinue;
          }
        }
      }
      dsyslog("duplicates: Deleting recording %s.", FileName);
      RecordingsHandler.Del(FileName); // must do this w/o holding a lock, because the cleanup section in cDirCopier::Action() might request one!
      if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), FileName) == 0)
         cControl::Shutdown();
      cStateKey recordingsStateKey;
      cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey);
      Recordings->SetExplicitModify();
      cRecording *recording = Recordings->GetByName(FileName);
      if (!recording || recording->Delete()) {
        cReplayControl::ClearLastReplayed(FileName);
        Recordings->DelByName(FileName);
        cVideoDiskUsage::ForceCheck();
        Recordings->SetModified();
        recordingsStateKey.Remove();
        Del(Current());
        SetHelpKeys();
        Display();
      } else {
        recordingsStateKey.Remove();
        Skins.Message(mtError, trVDR("Error while deleting recording!"));
      }
    }
  }
  return osContinue;
}

eOSState cMenuDuplicates::Play(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(ri->FileName());
    if (recording) {
      cDuplicatesReplayControl::SetRecording(recording->FileName());
      cControl::Shutdown();
      cControl::Launch(new cDuplicatesReplayControl);
      return osEnd;
    }
  }
  return osContinue;
}

eOSState cMenuDuplicates::Setup(void) {
  if (HasSubMenu())
    return osContinue;
  cMenuSetupDuplicates *setupMenu = new cMenuSetupDuplicates(this);
  setupMenu->SetTitle(cString::sprintf("%s - %s", tr("Duplicate recordings"), trVDR("Setup")));
  return AddSubMenu(setupMenu);
}

eOSState cMenuDuplicates::Info(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(ri->FileName());
    if (recording && recording->Info()->Title())
      return AddSubMenu(new cMenuDuplicate(recording));
  }
  return osContinue;
}

eOSState cMenuDuplicates::ToggleHidden(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
    bool hidden = ri->Visibility().Read() == HIDDEN;
    if (Interface->Confirm(hidden ? tr("Unhide recording?") : tr("Hide recording?"))) {
      if (ri->Visibility().Write(hidden)) {
        if (dc.hidden) {
          ri->Visibility().Set(!hidden);
          SetHelpKeys();
        } else {
          Del(Current());
          SetHelpKeys();
          Display();
        }
      } else
        Skins.Message(mtError, tr("Error while setting visibility!"));
    }
  }
  return osContinue;
}

eOSState cMenuDuplicates::ProcessKey(eKeys Key) {
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
    switch (Key) {
      case kPlay:
      case kRed:    return Play();
      case kGreen:  return Setup();
      case kYellow: return Delete();
      case kOk:
      case kInfo:   return Info();
      case kBlue:   return ToggleHidden();
      case kNone:   Set(true);
                    break;
      default: break;
    }
  }
  if (!HasSubMenu()) {
    if (Key != kNone)
      SetHelpKeys();
  }
  return state;
}

// --- cMenuSetupDuplicates --------------------------------------------------

cMenuSetupDuplicates::cMenuSetupDuplicates(cMenuDuplicates *MenuDuplicates) {
  SetMenuCategory(mcSetup);
  menuDuplicates = MenuDuplicates;
  Add(new cMenuEditBoolItem(tr("Compare title"), &dc.title));
  Add(new cMenuEditBoolItem(tr("Show hidden"), &dc.hidden));
}

void cMenuSetupDuplicates::Store(void) {
  dc.Store();
  if (menuDuplicates != NULL) {
    menuDuplicates->SetCurrent(NULL);
    menuDuplicates->Set();
  }
}

void cMenuSetupDuplicates::SetTitle(const char *Title) {
  cMenuSetupPage::SetTitle(Title);
}
