/*
 * visibility.h: Visibility classes for duplicates plugin.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef _DUPLICATES_VISIBILITY_H
#define _DUPLICATES_VISIBILITY_H

//#define DEBUG_VISIBILITY

#include <vdr/recording.h>

// --- eVisibility -----------------------------------------------------------

enum eVisibility {UNKNOWN, VISIBLE, HIDDEN};

// --- cVisibility -----------------------------------------------------------

class cVisibility {
private:
  cString hiddenFileName;
  eVisibility visibility;
public:
  cVisibility(const char *fileName);
  cVisibility(const cVisibility &Visibility);
  eVisibility Get(void);
  void Set(bool visible);
  eVisibility Read(void);
  bool Write(bool visible);
#ifdef DEBUG_VISIBILITY
  static int getCount;
  static int readCount;
  static int accessCount;
  static void ClearCounters(void);
#endif
};

#endif
