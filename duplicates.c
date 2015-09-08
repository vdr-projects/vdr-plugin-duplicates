/*
 * duplicates.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */


#include <vdr/plugin.h>
#include "config.h"
#include "menu.h"

static const char *VERSION        = "0.0.5";
static const char *DESCRIPTION    = trNOOP("Shows duplicate recordings");
static const char *MAINMENUENTRY  = trNOOP("Duplicate recordings");

class cPluginDuplicates : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginDuplicates(void);
  virtual ~cPluginDuplicates();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual bool Service(const char *Id, void *Data = NULL);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

cPluginDuplicates::cPluginDuplicates(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginDuplicates::~cPluginDuplicates()
{
  // Clean up after yourself!
}

const char *cPluginDuplicates::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginDuplicates::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginDuplicates::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  return true;
}

bool cPluginDuplicates::Start(void)
{
  // Start any background activities the plugin shall perform.
  return true;
}

void cPluginDuplicates::Stop(void)
{
  // Stop any background activities the plugin is performing.
}

void cPluginDuplicates::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginDuplicates::MainThreadHook(void)
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginDuplicates::Active(void)
{
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginDuplicates::WakeupTime(void)
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginDuplicates::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return new cMenuDuplicates();
}

cMenuSetupPage *cPluginDuplicates::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cMenuSetupDuplicates;
}

bool cPluginDuplicates::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return dc.SetupParse(Name, Value);;
}

bool cPluginDuplicates::Service(const char *Id, void *Data)
{
  // Handle custom service requests from other plugins
  return false;
}

const char **cPluginDuplicates::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  return NULL;
}

cString cPluginDuplicates::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  // Process SVDRP commands this plugin implements
  return NULL;
}

VDRPLUGINCREATOR(cPluginDuplicates); // Don't touch this!
