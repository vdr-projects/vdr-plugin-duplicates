/*
 * recording.c: Duplicate recording handling.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include "config.h"
#include "recording.h"
#include <sys/time.h>
#include <sstream>

// --- cDuplicateRecording -------------------------------------------------------

cDuplicateRecording::cDuplicateRecording(void) : visibility(NULL) {
  duplicates = new cList<cDuplicateRecording>;
}

cDuplicateRecording::cDuplicateRecording(const cRecording *Recording) : visibility(Recording->FileName()) {
  checked = false;
  fileName = std::string(Recording->FileName());
  text = std::string(Recording->Title('\t', true));
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
  duplicates = NULL;
}

cDuplicateRecording::cDuplicateRecording(const cDuplicateRecording &DuplicateRecording) :
  checked(DuplicateRecording.checked),
  visibility(DuplicateRecording.visibility),
  fileName(DuplicateRecording.fileName),
  text(DuplicateRecording.text),
  title(DuplicateRecording.title),
  description(DuplicateRecording.description) {
  if (DuplicateRecording.duplicates != NULL) {
    duplicates = new cList<cDuplicateRecording>;
    for (const cDuplicateRecording *duplicate = DuplicateRecording.duplicates->First(); duplicate; duplicate = DuplicateRecording.duplicates->Next(duplicate)) {
      duplicates->Add(new cDuplicateRecording(*duplicate));
    }
  } else
    duplicates = NULL;
}

cDuplicateRecording::~cDuplicateRecording() {
  delete duplicates;
}

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
    return dc.hidden || visibility.Read() != HIDDEN && DuplicateRecording->visibility.Read() != HIDDEN;

  return false;
}

// --- cDuplicateRecordings ------------------------------------------------------

cDuplicateRecordings::cDuplicateRecordings(void) : cList("duplicates") {}

bool cDuplicateRecordings::RemoveDeleted(void) {
  LOCK_RECORDINGS_READ
  int removed = 0;
  for (cDuplicateRecording *duplicates = First(); duplicates; duplicates = Next(duplicates)) {
    for (cDuplicateRecording *duplicate = duplicates->Duplicates()->First(); duplicate; duplicate = duplicates->Duplicates()->Next(duplicate)) {
      const cRecording *recording = Recordings->GetByName(duplicate->FileName().c_str());
      if (!recording || !dc.hidden && duplicate->Visibility().Read() == HIDDEN) {
        duplicates->Duplicates()->Del(duplicate);
        removed++;
      }
    }
    if (duplicates->Duplicates()->Count() < 2)
      Del(duplicates);
  }
  dsyslog("duplicates: Removed %d deleted recordings.", removed);
  return removed > 0;
}

cDuplicateRecordings DuplicateRecordings;

// --- cDuplicateRecordingScannerThread ------------------------------------------

cDuplicateRecordingScannerThread::cDuplicateRecordingScannerThread() : cThread("duplicate recording scanner", true) {
  title = dc.title;
  hidden = dc.hidden;
}

cDuplicateRecordingScannerThread::~cDuplicateRecordingScannerThread(){
  Stop();
}

void cDuplicateRecordingScannerThread::Stop(void) {
  Cancel(3);
}

void cDuplicateRecordingScannerThread::Action(void) {
  while (Running()) {
    if (title != dc.title || hidden != dc.hidden) {
      recordingsStateKey.Reset();
      title = dc.title;
      hidden = dc.hidden;
    }
    if (cRecordings::GetRecordingsRead(recordingsStateKey)) {
      recordingsStateKey.Remove();
      Scan();
    }
    if (Running())
      cCondWait::SleepMs(500);
  }
}

void cDuplicateRecordingScannerThread::Scan(void) {
  dsyslog("duplicates: Scanning of duplicates started.");
  struct timeval startTime, stopTime;
  gettimeofday(&startTime, NULL);
  cStateKey duplicateRecordingsStateKey;
  DuplicateRecordings.Lock(duplicateRecordingsStateKey, true);
  duplicateRecordingsStateKey.Remove(DuplicateRecordings.RemoveDeleted());
  cDuplicateRecording *descriptionless = new cDuplicateRecording();
  cList<cDuplicateRecording> recordings;
  cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey); // write access is necessary for sorting!
  Recordings->Sort();
  for (const cRecording *recording = Recordings->First(); recording; recording = Recordings->Next(recording)) {
    cDuplicateRecording *Item = new cDuplicateRecording(recording);
    if (Item->HasDescription())
      recordings.Add(Item);
    else if (dc.hidden || Item->Visibility().Read() != HIDDEN)
      descriptionless->Duplicates()->Add(Item);
  }
  recordingsStateKey.Remove(false); // sorting doesn't count as a real modification
  cList<cDuplicateRecording> duplicates;
  for (cDuplicateRecording *recording = recordings.First(); recording; recording = recordings.Next(recording)) {
    if (!Running() || RecordingsStateChanged()) {
      delete descriptionless;
      return;
    }
    if (cIoThrottle::Engaged())
      cCondWait::SleepMs(100);
    if (!recording->Checked()) {
      recording->SetChecked();
      cDuplicateRecording *duplicate = new cDuplicateRecording();
      duplicate->Duplicates()->Add(new cDuplicateRecording(*recording));
      for (cDuplicateRecording *compare = recordings.First(); compare; compare = recordings.Next(compare)) {
        if (!compare->Checked()) {
          if (recording->IsDuplicate(compare)) {
            duplicate->Duplicates()->Add(new cDuplicateRecording(*compare));
            compare->SetChecked();
          }
        }
      }
      if (duplicate->Duplicates()->Count() > 1) {
        duplicate->SetText(std::string(cString::sprintf(tr("%d duplicate recordings"), duplicate->Duplicates()->Count())));
        duplicates.Add(duplicate);
      } else
        delete duplicate;
    }
  }
  if (descriptionless->Duplicates()->Count() > 0) {
    descriptionless->SetText(std::string(cString::sprintf(tr("%d recordings without description"), descriptionless->Duplicates()->Count())));
    duplicates.Add(descriptionless);
  } else
    delete descriptionless;
  if (RecordingsStateChanged())
    return;
  DuplicateRecordings.Lock(duplicateRecordingsStateKey, true);
  DuplicateRecordings.Clear();
  for (cDuplicateRecording *duplicate = duplicates.First(); duplicate; duplicate = duplicates.Next(duplicate)) {
    DuplicateRecordings.Add(new cDuplicateRecording(*duplicate));
  }
  duplicateRecordingsStateKey.Remove();
  gettimeofday(&stopTime, NULL);
  double seconds = (((long long)stopTime.tv_sec * 1000000 + stopTime.tv_usec) - ((long long)startTime.tv_sec * 1000000 + startTime.tv_usec)) / 1000000.0;
  dsyslog("duplicates: Scanning of duplicates took %.2f seconds.", seconds);
}

bool cDuplicateRecordingScannerThread::RecordingsStateChanged(void) {
  if (cRecordings::GetRecordingsRead(recordingsStateKey)) {
    recordingsStateKey.Reset();
    recordingsStateKey.Remove();
    dsyslog("duplicates: Recordings state changed while scanning.");
    cCondWait::SleepMs(500);
    return true;
  }
  return false;
}

cDuplicateRecordingScannerThread DuplicateRecordingScanner;

