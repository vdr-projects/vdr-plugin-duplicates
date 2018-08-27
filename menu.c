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
#include <sys/time.h>
#if VDRVERSNUM >= 20301
#include <vdr/svdrp.h>
#endif
#include <string>
#include <sstream>

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
#if VDRVERSNUM >= 10728
  SetMenuCategory(mcRecording);
#endif
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

// --- cDuplicateRecording -------------------------------------------------------

class cDuplicateRecording : public cListObject {
private:
  bool checked;
  cVisibility visibility;
  std::string fileName;
  std::string name;
  std::string title;
  std::string description;
public:
  cDuplicateRecording(const cRecording *Recording);
  cDuplicateRecording(const cDuplicateRecording &DuplicateRecording);
  bool HasDescription(void) const { return ! description.empty(); }
  bool IsDuplicate(cDuplicateRecording *DuplicateRecording);
  void SetChecked(bool chkd = true) { checked = chkd; }
  bool Checked() { return checked; }
  cVisibility Visibility() { return visibility; }
  std::string FileName(void) { return fileName; }
  std::string Name(void) { return name; }
};

cDuplicateRecording::cDuplicateRecording(const cRecording *Recording) : visibility(Recording->FileName()) {
  checked = false;
  fileName = std::string(Recording->FileName());
#if defined LIEMIKUUTIO && LIEMIKUUTIO < 131
  name = std::string(Recording->Title('\t', true, -1, false));
#else
  name = std::string(Recording->Title('\t', true));
#endif
  if (dc.title && Recording->Info()->Title())
     title = std::string(Recording->Info()->Title());
  else
     title = std::string();
  std::stringstream desc;
  if (Recording->Info()->ShortText())
     desc << std::string(Recording->Info()->ShortText());
  if (Recording->Info()->Description())
     desc << std::string(Recording->Info()->Description());
  description = desc.str();
  while(true) {
    size_t found = description.find("|");
    if (found == std::string::npos)
       break;
    description.replace(found, 1, "");
  }
  while(true) {
    size_t found = description.find(" ");
    if (found == std::string::npos)
       break;
    description.replace(found, 1, "");
  }
}

cDuplicateRecording::cDuplicateRecording(const cDuplicateRecording &DuplicateRecording) :
  checked(DuplicateRecording.checked),
  visibility(DuplicateRecording.visibility),
  fileName(DuplicateRecording.fileName),
  name(DuplicateRecording.name),
  title(DuplicateRecording.title),
  description(DuplicateRecording.description) {}

bool cDuplicateRecording::IsDuplicate(cDuplicateRecording *DuplicateRecording) {
  if (!HasDescription() || !DuplicateRecording->HasDescription())
    return false;

  size_t found;
  if (dc.title) {
    found = title.size() > DuplicateRecording->title.size() ?
              title.find(DuplicateRecording->title) : DuplicateRecording->title.find(title);
    if (found == std::string::npos)
      return false;
  }

  found = description.size() > DuplicateRecording->description.size() ?
            description.find(DuplicateRecording->description) : DuplicateRecording->description.find(description);
  if (found != std::string::npos)
    return true;

  return false;
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
  SetText(DuplicateRecording->Name().c_str());
}

// --- cMenuDuplicates -------------------------------------------------------

cMenuDuplicates::cMenuDuplicates()
#if defined LIEMIKUUTIO || VDRVERSNUM >= 10721
:cOsdMenu(tr("Duplicate recordings"), 9, 7, 7)
#else
:cOsdMenu(tr("Duplicate recordings"), 9, 7)
#endif
{
#if VDRVERSNUM >= 10728
  SetMenuCategory(mcRecording);
#endif
#if VDRVERSNUM < 20301
  Recordings.StateChanged(recordingsState); // just to get the current state
#endif
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
  struct timeval startTime, stopTime;
  gettimeofday(&startTime, NULL);
#ifdef DEBUG_VISIBILITY
  cVisibility::ClearCounters();
  int isDuplicateCount = 0, menuDuplicateItemCount = 0;
#endif
  const char *CurrentRecording = NULL;
  int currentIndex = -1;
  if (Refresh)
    currentIndex = Current();
  else
    CurrentRecording = cReplayControl::LastReplayed();
  cList<cDuplicateRecording> descriptionless;
  cList<cDuplicateRecording> recordings;
  Clear();
  {
#if VDRVERSNUM >= 20301
    cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey); // write access is necessary for sorting!
    Recordings->Sort();
    for (const cRecording *recording = Recordings->First(); recording; recording = Recordings->Next(recording)) {
#else
    cThreadLock RecordingsLock(&Recordings);
    Recordings.Sort();
    for (cRecording *recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
#endif
      cDuplicateRecording *Item = new cDuplicateRecording(recording);
      if (Item->HasDescription())
        recordings.Add(Item);
      else if (dc.hidden || Item->Visibility().Read() != HIDDEN)
        descriptionless.Add(Item);
    }
#if VDRVERSNUM >= 20301
    recordingsStateKey.Remove(false); // sorting doesn't count as a real modification
#endif
  }
  for (cDuplicateRecording *recording = recordings.First(); recording; recording = recordings.Next(recording)) {
    if (!recording->Checked()) {
      recording->SetChecked();
      cList<cDuplicateRecording> duplicates;
      duplicates.Add(new cDuplicateRecording(*recording));
      for (cDuplicateRecording *compare = recordings.First(); compare; compare = recordings.Next(compare)) {
        if (!compare->Checked()) {
#ifdef DEBUG_VISIBILITY
          isDuplicateCount++;
#endif
          if (recording->IsDuplicate(compare)) {
            duplicates.Add(new cDuplicateRecording(*compare));
            compare->SetChecked();
          }
        }
      }
      int count = duplicates.Count();
      if (!dc.hidden && count > 1) {
        for (cDuplicateRecording *DuplicateRecording = duplicates.First(); DuplicateRecording; DuplicateRecording = duplicates.Next(DuplicateRecording)) {
          if (DuplicateRecording->Visibility().Read() == HIDDEN)
            count--;
        }
      }
      if (count > 1) {
        Add(SeparatorItem(cString::sprintf(tr("%d duplicate recordings"), duplicates.Count())));
        for (cDuplicateRecording *DuplicateRecording = duplicates.First(); DuplicateRecording; DuplicateRecording = duplicates.Next(DuplicateRecording)) {
          if (dc.hidden || DuplicateRecording->Visibility().Read() != HIDDEN) {
            cMenuDuplicateItem *Item = new cMenuDuplicateItem(DuplicateRecording);
#ifdef DEBUG_VISIBILITY
            menuDuplicateItemCount++;
#endif
            if (*Item->Text()) {
              Add(Item);
              if (CurrentRecording && strcmp(CurrentRecording, Item->FileName()) == 0)
                SetCurrent(Item);
            } else
              delete Item;
          }
        }
      }
    }
  }
  if (descriptionless.Count() > 0)
    Add(SeparatorItem(cString::sprintf(tr("%d recordings without description"), descriptionless.Count())));
  for (cDuplicateRecording *DescriptionlessRecording = descriptionless.First(); DescriptionlessRecording; DescriptionlessRecording = descriptionless.Next(DescriptionlessRecording)) {
    cMenuDuplicateItem *Item = new cMenuDuplicateItem(DescriptionlessRecording);
#ifdef DEBUG_VISIBILITY
    menuDuplicateItemCount++;
#endif
    if (*Item->Text()) {
      Add(Item);
      if (CurrentRecording && strcmp(CurrentRecording, Item->FileName()) == 0)
        SetCurrent(Item);
    } else
      delete Item;
  }
  if (Count() == 0)
    Add(SeparatorItem(cString::sprintf(tr("%d duplicate recordings"), 0)));
  if (Refresh) {
    if (currentIndex >= 0) {
      if(currentIndex >= Count())
        currentIndex = Count() - 1;
      cOsdItem *current = Get(currentIndex);
      while (current) {
        if (current->Selectable()) {
          SetCurrent(current);
          break;
        }
        current = Prev(current);
      }
    }
    Display();
  }
  gettimeofday(&stopTime, NULL);
  double seconds = (((long long)stopTime.tv_sec * 1000000 + stopTime.tv_usec) - ((long long)startTime.tv_sec * 1000000 + startTime.tv_usec)) / 1000000.0;
#ifdef DEBUG_VISIBILITY
  dsyslog("duplicates: Displaying of duplicates took %.2f seconds, is duplicate count %d, get count %d, read count %d, access count %d, duplicate item count %d.",
    seconds, isDuplicateCount, cVisibility::getCount, cVisibility::readCount, cVisibility::accessCount, menuDuplicateItemCount);
#else
  dsyslog("duplicates: Displaying of duplicates took %.2f seconds.", seconds);
#endif
}

#if VDRVERSNUM >= 20301
static bool HandleRemoteModifications(cTimer *NewTimer, cTimer *OldTimer = NULL) {
  cString ErrorMessage;
  if (!HandleRemoteTimerModifications(NewTimer, OldTimer, &ErrorMessage)) {
    Skins.Message(mtError, ErrorMessage);
    return false;
  }
  return true;
}
#endif

static bool TimerStillRecording(const char *FileName) {
  if (cRecordControl *rc = cRecordControls::GetRecordControl(FileName)) {
    // local timer
    if (Interface->Confirm(trVDR("Timer still recording - really delete?"))) {
      LOCK_TIMERS_WRITE;
      if (cTimer *Timer = rc->Timer()) {
        Timer->Skip();
#if VDRVERSNUM >= 20301
        cRecordControls::Process(Timers, time(NULL));
#else
        cRecordControls::Process(time(NULL));
#endif
        if (Timer->IsSingleEvent()) {
#if VDRVERSNUM >= 20301
          Timers->Del(Timer);
#else
          Timers.Del(Timer);
#endif
          isyslog("deleted timer %s", *Timer->ToDescr());
        }
#if VDRVERSNUM >= 20301
        Timers->SetModified();
#else
        Timers.SetModified();
#endif
      }
    } else
      return true; // user didn't confirm deletion
  }
#if VDRVERSNUM >= 20301
  else {
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
#endif
  return false;
}

eOSState cMenuDuplicates::Delete(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
    if (Interface->Confirm(trVDR("Delete recording?"))) {
      if (TimerStillRecording(ri->FileName()))
        return osContinue;
#if VDRVERSNUM >= 20301
      cString FileName;
      {
        LOCK_RECORDINGS_READ
        if (const cRecording *Recording = Recordings->GetByName(ri->FileName())) {
          FileName = Recording->FileName();
          if (RecordingsHandler.GetUsage(FileName)) {
            if (!Interface->Confirm(trVDR("Recording is being edited - really delete?")))
              return osContinue;
          }
        }
      }
      RecordingsHandler.Del(FileName); // must do this w/o holding a lock, because the cleanup section in cDirCopier::Action() might request one!
#else
      cString FileName = ri->FileName();
      if (RecordingsHandler.GetUsage(FileName)) {
        if (Interface->Confirm(trVDR("Recording is being edited - really delete?"))) {
          RecordingsHandler.Del(FileName);
        } else
          return osContinue;
      }
#endif
      if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), FileName) == 0)
         cControl::Shutdown();
#if VDRVERSNUM >= 20301
      cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey);
      Recordings->SetExplicitModify();
      cRecording *recording = Recordings->GetByName(FileName);
#else
      cRecording *recording = Recordings.GetByName(FileName);
#endif
      if (!recording || recording->Delete()) {
#if VDRVERSNUM >= 20301
        cReplayControl::ClearLastReplayed(FileName);
        Recordings->DelByName(FileName);
#else
        cReplayControl::ClearLastReplayed(FileName);
        Recordings.DelByName(FileName);
        Recordings.StateChanged(recordingsState); // update state after deletion
#endif
        cVideoDiskUsage::ForceCheck();
#if VDRVERSNUM >= 20301
        Recordings->SetModified();
        recordingsStateKey.Remove();
#endif
        cOsdMenu::Del(Current());
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
        SetHelpKeys();
        Display();
      } else
        Skins.Message(mtError, trVDR("Error while deleting recording!"));
    }
  }
  return osContinue;
}

eOSState cMenuDuplicates::Play(void) {
  if (HasSubMenu() || Count() == 0)
    return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
#if VDRVERSNUM >= 20301
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(ri->FileName());
#else
    cRecording *recording = Recordings.GetByName(ri->FileName());
#endif
    if (recording) {
#if VDRVERSNUM >= 10728
      cDuplicatesReplayControl::SetRecording(recording->FileName());
#else
      cDuplicatesReplayControl::SetRecording(recording->FileName(), recording->Title());
#endif
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
#if VDRVERSNUM >= 20301
    LOCK_RECORDINGS_READ;
    const cRecording *recording = Recordings->GetByName(ri->FileName());
#else
    cRecording *recording = Recordings.GetByName(ri->FileName());
#endif
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
        if (dc.hidden)
          ri->Visibility().Set(!hidden);
        else
          Set(true);
        SetHelpKeys();
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
      default: break;
    }
  }
  if (!HasSubMenu()) {
#if VDRVERSNUM >= 20301
    if (cRecordings::GetRecordingsRead(recordingsStateKey)) {
      recordingsStateKey.Remove();
#else
    if (Recordings.StateChanged(recordingsState)) {
      Set(true);
#endif
    }
    if (Key != kNone)
      SetHelpKeys();
  }
  return state;
}

// --- cMenuSetupDuplicates --------------------------------------------------

cMenuSetupDuplicates::cMenuSetupDuplicates(cMenuDuplicates *MenuDuplicates) {
#if VDRVERSNUM >= 10728
  SetMenuCategory(mcSetup);
#endif
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
