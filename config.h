/*
 * config.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef _DUPLICATES_CONFIG_H
#define _DUPLICATES_CONFIG_H

class cDuplicatesConfig {
  public:
    // variables
    int title;
    // member functions
    cDuplicatesConfig();
    ~cDuplicatesConfig();
    bool SetupParse(const char *Name, const char *Value);
    void Store(void);
};

extern cDuplicatesConfig dc;

#endif
