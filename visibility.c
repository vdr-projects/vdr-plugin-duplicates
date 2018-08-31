/*
 * visibility.c: Visibility classes for duplicates plugin.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include "visibility.h"

// --- cVisibility -----------------------------------------------------------

cVisibility::cVisibility(const char *fileName) {
  hiddenFileName = AddDirectory(fileName, "duplicates.hidden");
  visibility = UNKNOWN;
}

cVisibility::cVisibility(const cVisibility &Visibility) :
  hiddenFileName(Visibility.hiddenFileName),
  visibility(Visibility.visibility) {}

eVisibility cVisibility::Get(void) {
  return visibility;
}

void cVisibility::Set(bool visible) {
  visibility = visible ? VISIBLE : HIDDEN;
}

eVisibility cVisibility::Read(void) {
  if (visibility == UNKNOWN) {
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

