/*
 * config.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <stdlib.h>
#include <strings.h>
#include <vdr/plugin.h>
#include "config.h"

cDuplicatesConfig::cDuplicatesConfig() {
  title = 1;
}

cDuplicatesConfig::~cDuplicatesConfig() { }


bool cDuplicatesConfig::SetupParse(const char *Name, const char *Value) {
  if      (!strcasecmp(Name, "title"))     title = atoi(Value);
  else
    return false;
  return true;
}

void cDuplicatesConfig::Store(void) {
  cPluginManager::GetPlugin(PLUGIN_NAME_I18N)->SetupStore("title", title);
}

cDuplicatesConfig dc;
