/*
 * visibility.c: Visibility classes for duplicates plugin.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include "visibility.h"

// --- cVisibility -----------------------------------------------------------

#ifdef DEBUG_VISIBILITY
int cVisibility::getCount = 0;
int cVisibility::readCount = 0;
int cVisibility::accessCount = 0;
#endif

cVisibility::cVisibility(const char *fileName) {
  hiddenFileName = AddDirectory(fileName, "duplicates.hidden");
  visibility = UNKNOWN;
}

cVisibility::cVisibility(const cVisibility &Visibility) :
  hiddenFileName(Visibility.hiddenFileName),
  visibility(Visibility.visibility) {}

eVisibility cVisibility::Get(void) {
#ifdef DEBUG_VISIBILITY
  getCount++;
#endif
  return visibility;
}

void cVisibility::Set(bool visible) {
  visibility = visible ? VISIBLE : HIDDEN;
}

eVisibility cVisibility::Read(void) {
#ifdef DEBUG_VISIBILITY
  readCount++;
#endif
  if (visibility == UNKNOWN) {
#ifdef DEBUG_VISIBILITY
    accessCount++;
#endif
    visibility = access(hiddenFileName, F_OK) == 0 ? HIDDEN : VISIBLE;
  }
  return visibility;
}

bool cVisibility::Write(bool visible) {
  if (visible) {
    if (remove(hiddenFileName) == 0) {
      visibility = VISIBLE;
      return true;
    }
  } else {
    FILE *f = fopen(hiddenFileName, "w");
    if (f) {
      fclose(f);
      visibility = HIDDEN;
      return true;
    }
  }
  return false;
}

#ifdef DEBUG_VISIBILITY
void cVisibility::ClearCounters(void) {
  getCount = 0;
  readCount = 0;
  accessCount = 0;
}
#endif
