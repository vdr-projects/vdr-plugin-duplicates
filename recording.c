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
#if defined LIEMIKUUTIO && LIEMIKUUTIO < 131
  text = std::string(Recording->Title('\t', true, -1, false));
#else
  text = std::string(Recording->Title('\t', true));
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

cDuplicateRecordings::cDuplicateRecordings(void) {}

void cDuplicateRecordings::Update(void) {
  struct timeval startTime, stopTime;
  gettimeofday(&startTime, NULL);
  cMutexLock MutexLock(&mutex);
#ifdef DEBUG_VISIBILITY
  cVisibility::ClearCounters();
  int isDuplicateCount = 0;
#endif
  cDuplicateRecording *descriptionless = new cDuplicateRecording();
  cList<cDuplicateRecording> recordings;
  Clear();
  {
#if VDRVERSNUM >= 20301
    cStateKey recordingsStateKey;
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
        descriptionless->Duplicates()->Add(Item);
    }
#if VDRVERSNUM >= 20301
    recordingsStateKey.Remove(false); // sorting doesn't count as a real modification
#endif
  }
  for (cDuplicateRecording *recording = recordings.First(); recording; recording = recordings.Next(recording)) {
    if (!recording->Checked()) {
      recording->SetChecked();
      cDuplicateRecording *duplicates = new cDuplicateRecording();
      duplicates->Duplicates()->Add(new cDuplicateRecording(*recording));
      for (cDuplicateRecording *compare = recordings.First(); compare; compare = recordings.Next(compare)) {
        if (!compare->Checked()) {
#ifdef DEBUG_VISIBILITY
          isDuplicateCount++;
#endif
          if (recording->IsDuplicate(compare)) {
            duplicates->Duplicates()->Add(new cDuplicateRecording(*compare));
            compare->SetChecked();
          }
        }
      }
      if (duplicates->Duplicates()->Count() > 1) {
        duplicates->SetText(cString::sprintf(tr("%d duplicate recordings"), duplicates->Duplicates()->Count()));
        Add(duplicates);
      } else
        delete duplicates;
    }
  }
  if (descriptionless->Duplicates()->Count() > 0) {
    descriptionless->SetText(cString::sprintf(tr("%d recordings without description"), descriptionless->Duplicates()->Count()));
    Add(descriptionless);
  } else
    delete descriptionless;
  gettimeofday(&stopTime, NULL);
  double seconds = (((long long)stopTime.tv_sec * 1000000 + stopTime.tv_usec) - ((long long)startTime.tv_sec * 1000000 + startTime.tv_usec)) / 1000000.0;
#ifdef DEBUG_VISIBILITY
  dsyslog("duplicates: Scanning of duplicates took %.2f seconds, is duplicate count %d, get count %d, read count %d, access count %d.",
    seconds, isDuplicateCount, cVisibility::getCount, cVisibility::readCount, cVisibility::accessCount);
#else
  dsyslog("duplicates: Scanning of duplicates took %.2f seconds.", seconds);
#endif
}

cDuplicateRecordings DuplicateRecordings;

