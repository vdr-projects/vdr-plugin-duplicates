/*
 * recording.h: Duplicate recording handling.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef _DUPLICATES_RECORDING_H
#define _DUPLICATES_RECORDING_H

#include "visibility.h"
#include <vdr/recording.h>
#include <string>

// --- cDuplicateRecording -------------------------------------------------------

class cDuplicateRecording : public cListObject {
private:
  bool checked;
  cVisibility visibility;
  std::string fileName;
  std::string text;
  std::string title;
  std::string description;
  cList<cDuplicateRecording> *duplicates;
public:
  cDuplicateRecording(void);
  cDuplicateRecording(const cRecording *Recording);
  cDuplicateRecording(const cDuplicateRecording &DuplicateRecording);
  ~cDuplicateRecording();
  bool HasDescription(void) const;
  bool IsDuplicate(cDuplicateRecording *DuplicateRecording);
  void SetChecked(bool chkd = true) { checked = chkd; }
  bool Checked() { return checked; }
  cVisibility Visibility() { return visibility; }
  std::string FileName(void) { return fileName; }
  void SetText(std::string t) { text = t; }
  std::string Text(void) { return text; }
  cList<cDuplicateRecording> *Duplicates(void) { return duplicates; }
};

// --- cDuplicateRecordings ------------------------------------------------------

class cDuplicateRecordings : public cList<cDuplicateRecording> {
public:
  cDuplicateRecordings(void);
  void Remove(std::string fileName);
};

extern cDuplicateRecordings DuplicateRecordings;

// --- cDuplicateRecordingScannerThread ------------------------------------------

class cDuplicateRecordingScannerThread : public cThread {
private:
  cStateKey recordingsStateKey;
  int title;
  int hidden;
  void Scan(void);
  bool RecordingsStateChanged(void);
protected:
  virtual void Action(void);
public:
  cDuplicateRecordingScannerThread();
  ~cDuplicateRecordingScannerThread();
  void Stop(void);
};

extern cDuplicateRecordingScannerThread DuplicateRecordingScanner;

#endif
