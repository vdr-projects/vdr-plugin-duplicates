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
#include <vdr/menu.h>
#include <vdr/status.h>
#include <vdr/interface.h>
#include <string>
#include <sstream>

static inline cOsdItem *SeparatorItem(const char *Label)
{
  cOsdItem *Item = new cOsdItem(cString::sprintf("----- %s -----", Label));
  Item->SetSelectable(false);
  return Item;
}

// --- cDuplicatesReplayControl -------------------------------------------------------

class cDuplicatesReplayControl : public cReplayControl {
public:
  virtual eOSState ProcessKey(eKeys Key);
  };

eOSState cDuplicatesReplayControl::ProcessKey(eKeys Key)
{
  eOSState state = cReplayControl::ProcessKey(Key);
  if (state == osRecordings)
  {
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
:cOsdMenu(trVDR("Recording info"))
{
#if VDRVERSNUM >= 10728
  SetMenuCategory(mcRecording);
#endif
  recording = Recording;
  SetHelp(trVDR("Button$Play"));
}

void cMenuDuplicate::Display(void)
{
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
  char *fileName;
public:
  cMenuDuplicateItem(const cRecording *Recording);
  ~cMenuDuplicateItem();
  const char *FileName(void) { return fileName; }
  };

cMenuDuplicateItem::cMenuDuplicateItem(const cRecording *Recording)
{
  fileName = strdup(Recording->FileName());
#if defined LIEMIKUUTIO && LIEMIKUUTIO < 131
  SetText(Recording->Title('\t', true, -1, false));
#else
  SetText(Recording->Title('\t', true));
#endif
}

cMenuDuplicateItem::~cMenuDuplicateItem()
{
  free(fileName);
}

// --- cDuplicateRecording -------------------------------------------------------

class cDuplicateRecording : public cListObject {
private:
  const cRecording *recording;
  bool checked;
protected:
  std::string title;
  std::string description;
public:
  cDuplicateRecording(const cRecording *Recording);
  ~cDuplicateRecording();
  const cRecording *Recording(void);
  bool HasDescription(void) const { return ! description.empty(); }
  bool IsDuplicate(const cDuplicateRecording *DuplicateRecording);
  void SetChecked(bool chkd = true) { checked = chkd; }
  bool Checked() { return checked; }
  };

cDuplicateRecording::cDuplicateRecording(const cRecording *Recording)
{
  recording = Recording;
  checked = false;
  if (dc.title && recording->Info()->Title())
     title = std::string(recording->Info()->Title());
  else
     title = std::string();
  std::stringstream desc;
  if (recording->Info()->ShortText())
     desc << std::string(recording->Info()->ShortText());
  if (recording->Info()->Description())
     desc << std::string(recording->Info()->Description());
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

cDuplicateRecording::~cDuplicateRecording()
{
  title.clear();
  description.clear();
}

const cRecording *cDuplicateRecording::Recording(void)
{
  return recording;
}

bool cDuplicateRecording::IsDuplicate(const cDuplicateRecording *DuplicateRecording)
{
  if (!HasDescription() || !DuplicateRecording->HasDescription())
     return false;

  size_t found;
  if (dc.title)
  {
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
  Recordings.StateChanged(recordingsState); // just to get the current state
  helpKeys = -1;
  Set();
  Display();
  SetHelpKeys();
}

cMenuDuplicates::~cMenuDuplicates()
{
  helpKeys = -1;
}

void cMenuDuplicates::SetHelpKeys(void)
{
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  int NewHelpKeys = 0;
  if (ri) {
      NewHelpKeys = 1;
      cRecording *recording = GetRecording(ri);
      if (recording && recording->Info()->Title())
         NewHelpKeys = 2;
     }
  if (NewHelpKeys != helpKeys) {
     switch (NewHelpKeys) {
       case 0: SetHelp(NULL); break;
       case 1:
       case 2: SetHelp(trVDR("Button$Play"), trVDR("Setup"), trVDR("Button$Delete"), NewHelpKeys == 2 ? trVDR("Button$Info") : NULL);
       default: ;
       }
     helpKeys = NewHelpKeys;
     }
}

void cMenuDuplicates::Set(bool Refresh)
{
  const char *CurrentRecording = NULL;
  int currentIndex = -1;
  if (Refresh)
     currentIndex = Current();
  else
     CurrentRecording = cReplayControl::LastReplayed();
  cList<cDuplicateRecording> *descriptionless = new cList<cDuplicateRecording>;
  cList<cDuplicateRecording> *recordings = new cList<cDuplicateRecording>;
  cThreadLock RecordingsLock(&Recordings);
  Clear();
  Recordings.Sort();
  for (cRecording *recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
      cDuplicateRecording *Item = new cDuplicateRecording(recording);
      if (Item->HasDescription())
         recordings->Add(Item);
      else
         descriptionless->Add(Item);
      }
  for (cDuplicateRecording *recording = recordings->First(); recording; recording = recordings->Next(recording)) {
      if (!recording->Checked()) {
         recording->SetChecked();
         cList<cDuplicateRecording> *duplicates = new cList<cDuplicateRecording>;
         duplicates->Add(new cDuplicateRecording(recording->Recording()));
         for (cDuplicateRecording *compare = recordings->First(); compare; compare = recordings->Next(compare)) {
       	     if (!compare->Checked() && recording->IsDuplicate(compare)) {
                duplicates->Add(new cDuplicateRecording(compare->Recording()));
                compare->SetChecked();
      	        }
             }
         if (duplicates->Count() > 1) {
            Add(SeparatorItem(cString::sprintf(tr("%d duplicate recordings"), duplicates->Count())));
            for (cDuplicateRecording *DuplicateRecording = duplicates->First(); DuplicateRecording; DuplicateRecording = duplicates->Next(DuplicateRecording)) {
                cMenuDuplicateItem *Item = new cMenuDuplicateItem(DuplicateRecording->Recording());
                if (*Item->Text())
                {
                   Add(Item);
                   if (CurrentRecording && strcmp(CurrentRecording, Item->FileName()) == 0)
                      SetCurrent(Item);
                   }
                else
                   delete Item;
                }
            }
            delete duplicates;
         }
      }
  if (descriptionless->Count() > 0)
     Add(SeparatorItem(cString::sprintf(tr("%d recordings without description"), descriptionless->Count())));
  for (cDuplicateRecording *DescriptionlessRecording = descriptionless->First(); DescriptionlessRecording; DescriptionlessRecording = descriptionless->Next(DescriptionlessRecording)) {
      cMenuDuplicateItem *Item = new cMenuDuplicateItem(DescriptionlessRecording->Recording());
      if (*Item->Text())
      {
         Add(Item);
         if (CurrentRecording && strcmp(CurrentRecording, Item->FileName()) == 0)
            SetCurrent(Item);
         }
      else
         delete Item;
      }
  delete descriptionless;
  delete recordings;
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
}

cRecording *cMenuDuplicates::GetRecording(cMenuDuplicateItem *Item)
{
  cRecording *recording = Recordings.GetByName(Item->FileName());
  if (!recording)
     Skins.Message(mtError, trVDR("Error while accessing recording!"));
  return recording;
}

eOSState cMenuDuplicates::Delete(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
     if (Interface->Confirm(trVDR("Delete recording?"))) {
        cRecordControl *rc = cRecordControls::GetRecordControl(ri->FileName());
        if (rc) {
           if (Interface->Confirm(trVDR("Timer still recording - really delete?"))) {
              cTimer *timer = rc->Timer();
              if (timer) {
                 timer->Skip();
                 cRecordControls::Process(time(NULL));
                 if (timer->IsSingleEvent()) {
                    isyslog("deleting timer %s", *timer->ToDescr());
                    Timers.Del(timer);
                    }
                 Timers.SetModified();
                 }
              }
           else
              return osContinue;
           }
        cRecording *recording = GetRecording(ri);
        if (recording) {
           if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), ri->FileName()) == 0)
              cControl::Shutdown();
           if (recording->Delete()) {
              cReplayControl::ClearLastReplayed(ri->FileName());
              Recordings.DelByName(ri->FileName());
              Set(true);
              SetHelpKeys();
              }
           else
              Skins.Message(mtError, trVDR("Error while deleting recording!"));
           }
        }
     }
  return osContinue;
}

eOSState cMenuDuplicates::Play(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
     cRecording *recording = GetRecording(ri);
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

eOSState cMenuDuplicates::Setup(void)
{
  if (HasSubMenu())
     return osContinue;
  cMenuSetupDuplicates *setupMenu = new cMenuSetupDuplicates(this);
  setupMenu->SetTitle(cString::sprintf("%s - %s", tr("Duplicate recordings"), trVDR("Setup")));
  return AddSubMenu(setupMenu);
}

eOSState cMenuDuplicates::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuDuplicateItem *ri = (cMenuDuplicateItem *)Get(Current());
  if (ri) {
     cRecording *recording = GetRecording(ri);
     if (recording && recording->Info()->Title())
        return AddSubMenu(new cMenuDuplicate(recording));
     }
  return osContinue;
}

eOSState cMenuDuplicates::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kPlay:
       case kRed:    return Play();
       case kGreen:  return Setup();
       case kYellow: return Delete();
       case kOk:
       case kInfo:
       case kBlue:   return Info();
       case kNone:   if (Recordings.StateChanged(recordingsState))
                        Set(true);
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

cMenuSetupDuplicates::cMenuSetupDuplicates(cMenuDuplicates *MenuDuplicates)
{
#if VDRVERSNUM >= 10728
  SetMenuCategory(mcSetup);
#endif
  menuDuplicates = MenuDuplicates;
  Add(new cMenuEditBoolItem(tr("Compare title"), &dc.title));
}

void cMenuSetupDuplicates::Store(void)
{
  dc.Store();
  if (menuDuplicates != NULL)
  {
     menuDuplicates->SetCurrent(NULL);
     menuDuplicates->Set();
  }
}

void cMenuSetupDuplicates::SetTitle(const char *Title)
{
  cMenuSetupPage::SetTitle(Title);
}
