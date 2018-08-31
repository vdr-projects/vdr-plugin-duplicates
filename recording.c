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
  description(DuplicateRecording.description),
  duplicates(DuplicateRecording.duplicates) {}

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

cDuplicateRecordings DuplicateRecordings;

// --- cDuplicateRecordingScannerThread ------------------------------------------

cDuplicateRecordingScannerThread::cDuplicateRecordingScannerThread() : cThread("duplicate recording scanner", true) {
  title = dc.title;
  hidden = dc.hidden;  
}

cDuplicateRecordingScannerThread::~cDuplicateRecordingScannerThread(){
  Stop();
}

void cDuplicateRecordingScannerThread::Stop() {
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
      cCondWait::SleepMs(250);
  }
}

void cDuplicateRecordingScannerThread::Scan(void) {
  struct timeval startTime, stopTime;
  gettimeofday(&startTime, NULL);
  cStateKey duplicateRecordingsStateKey;
  DuplicateRecordings.Lock(duplicateRecordingsStateKey, true);
  cDuplicateRecording *descriptionless = new cDuplicateRecording();
  cList<cDuplicateRecording> recordings;
  DuplicateRecordings.Clear();
  {
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
  }
  for (cDuplicateRecording *recording = recordings.First(); recording; recording = recordings.Next(recording)) {
    if (!Running())
      return;
    if (cIoThrottle::Engaged())
      cCondWait::SleepMs(100);
    if (!recording->Checked()) {
      recording->SetChecked();
      cDuplicateRecording *duplicates = new cDuplicateRecording();
      duplicates->Duplicates()->Add(new cDuplicateRecording(*recording));
      for (cDuplicateRecording *compare = recordings.First(); compare; compare = recordings.Next(compare)) {
        if (!compare->Checked()) {
          if (recording->IsDuplicate(compare)) {
            duplicates->Duplicates()->Add(new cDuplicateRecording(*compare));
            compare->SetChecked();
          }
        }
      }
      if (duplicates->Duplicates()->Count() > 1) {
        duplicates->SetText(cString::sprintf(tr("%d duplicate recordings"), duplicates->Duplicates()->Count()));
        DuplicateRecordings.Add(duplicates);
      } else
        delete duplicates;
    }
  }
  if (descriptionless->Duplicates()->Count() > 0) {
    descriptionless->SetText(cString::sprintf(tr("%d recordings without description"), descriptionless->Duplicates()->Count()));
    DuplicateRecordings.Add(descriptionless);
  } else
    delete descriptionless;
  duplicateRecordingsStateKey.Remove();
  gettimeofday(&stopTime, NULL);
  double seconds = (((long long)stopTime.tv_sec * 1000000 + stopTime.tv_usec) - ((long long)startTime.tv_sec * 1000000 + startTime.tv_usec)) / 1000000.0;
  dsyslog("duplicates: Scanning of duplicates took %.2f seconds.", seconds);
}

cDuplicateRecordingScannerThread DuplicateRecordingScanner;

