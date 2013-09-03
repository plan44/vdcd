//
//  application.cpp
//  p44utils
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "application.hpp"

#include "mainloop.hpp"

#include <sys/stat.h> // for umask

using namespace p44;

#pragma mark - Application base class

static Application *sharedApplicationP = NULL;


Application *Application::sharedApplication()
{
  return sharedApplicationP;
}


Application::Application(MainLoop *aMainLoopP)
{
	mainLoopP = aMainLoopP;
  sharedApplicationP = this;
}


Application::Application()
{
	mainLoopP = MainLoop::currentMainLoop();
  sharedApplicationP = this;
}

Application::~Application()
{
}


int Application::main(int argc, char **argv)
{
	// NOP application
	return EXIT_SUCCESS;
}


void Application::initialize()
{
	// NOP
}


int Application::run()
{
	if (!mainLoopP)
		terminateApp(EXIT_SUCCESS); // NOP App
	// schedule the initialize() method as first mainloop method
	mainLoopP->executeOnce(boost::bind(&Application::initialize, this));
	// run the mainloop
	return mainLoopP->run();
}


void Application::terminateApp(int aExitCode)
{
  // TODO: add more elaborated exit handling
  exit(aExitCode);
}



void Application::daemonize()
{
  pid_t pid, sid;
	
  /* already a daemon */
  if ( getppid() == 1 ) return;
	
  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then we can exit the parent process. */
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
	
  /* At this point we are executing as the child process */
	
  /* Change the file mode mask */
  umask(0);
	
  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }
	
  /* Change the current working directory.  This prevents the current
	 directory from being locked; hence not being able to remove it. */
  if ((chdir("/")) < 0) {
    exit(EXIT_FAILURE);
  }
	
  /* Redirect standard files to /dev/null */
  freopen( "/dev/null", "r", stdin);
  freopen( "/dev/null", "w", stdout);
  freopen( "/dev/null", "w", stderr);
}


#pragma mark - CmdLineApp command line application


/// constructor
CmdLineApp::CmdLineApp(MainLoop *aMainLoopP) :
  inherited(aMainLoopP),
  optionDescriptors(NULL)
{
}


/// destructor
CmdLineApp::~CmdLineApp()
{
}



void CmdLineApp::setCommandDescriptors(const CmdLineOptionDescriptor *aOptionDescriptors, const char *aSynopsis)
{
  optionDescriptors = aOptionDescriptors;
  synopsis = aSynopsis ? aSynopsis : "Usage: %1$s";
}


#define MAX_INDENT 20

void CmdLineApp::showUsage()
{
  // print synopsis
  fprintf(stderr, synopsis.c_str(), invocationName.c_str());
  // print options
  // - calculate indent
  size_t indent = 0;
  const CmdLineOptionDescriptor *optionDescP = optionDescriptors;
  bool anyShortOpts = false;
  while (optionDescP && (optionDescP->longOptionName!=NULL || optionDescP->shortOptionChar!='\x00')) {
    if (optionDescP->shortOptionChar) {
      anyShortOpts = true;
    }
    size_t n = 0;
    if (optionDescP->longOptionName) {
      n += strlen(optionDescP->longOptionName)+2; // "--XXXXX"
    }
    const char *desc = optionDescP->optionDescription;
    if (optionDescP->withArgument && desc) {
      const char *p = strchr(desc, ':');
      if (p) {
        n += 1 + (p-desc); // add room for argument description
      }
    }
    if (n>MAX_INDENT) n = MAX_INDENT;
    if (n>indent) indent = n; // new max
    optionDescP++;
  }
  if (anyShortOpts) indent += 4; // "-X, " prefix
  indent += 2 + 2; // two at beginning, two at end
  // - print options
  fprintf(stderr, "Options:\n");
  optionDescP = optionDescriptors;
  while (optionDescP && (optionDescP->longOptionName!=NULL || optionDescP->shortOptionChar!='\x00')) {
    size_t remaining = indent;
    fprintf(stderr, "  "); // start indent
    remaining -= 2;
    if (anyShortOpts) {
      // short names exist, print them for those options that have them
      if (optionDescP->shortOptionChar)
        fprintf(stderr, "-%c", optionDescP->shortOptionChar);
      else
        fprintf(stderr, "  ");
      remaining -= 2;
      if (optionDescP->longOptionName) {
        // long option follows, fill up
        if (optionDescP->shortOptionChar)
          fprintf(stderr, ", ");
        else
          fprintf(stderr, "  ");
        remaining -= 2;
      }
    }
    // long name
    if (optionDescP->longOptionName) {
      fprintf(stderr, "--%s", optionDescP->longOptionName);
      remaining -= strlen(optionDescP->longOptionName);
    }
    // argument
    const char *desc = optionDescP->optionDescription;
    if (optionDescP->withArgument) {
      if (desc) {
        const char *p = strchr(desc, ':');
        if (p) {
          size_t n = (p-desc);
          string argDesc(desc,n);
          fprintf(stderr, " %s", argDesc.c_str());
          remaining -= argDesc.length()+1;
          desc += n+1; // desc starts after colon
        }
      }
    }
    // complete first line indent
    while (remaining>0) fprintf(stderr, " ");
    // print option description, properly indented
    if (desc) {
      while (*desc) {
        if (*desc=='\n') {
          // next line
          fprintf(stderr, "\n");
          // indent
          remaining = indent;
          while (remaining>0) fprintf(stderr, " ");
        }
        else {
          fprintf(stderr, "%c", *desc);
        }
        desc++;
      }
    }
    // end of option, next line
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}



void CmdLineApp::parseCommandLine(int aArgc, char **aArgv)
{
  if (aArgc>0) {
    invocationName = aArgv[0];
    int rawArgIndex=1;
    while(rawArgIndex<aArgc) {
      const char *argP = aArgv[rawArgIndex];
      if (*argP=='-') {
        // option argument
        argP++;
        bool longOpt = false;
        string optName;
        string optArg;
        bool optArgFound = false;
        if (*argP=='-') {
          // long option
          longOpt = true;
          optName = argP+1;
          if (optName=="help") {
            showUsage();
            terminateApp(EXIT_SUCCESS);
          }
        }
        else {
          // short option
          optName = argP;
          if (optName=="h") {
            showUsage();
            terminateApp(EXIT_SUCCESS);
          }
          if (optName.length()>1 && optName[1]!='=') {
            // option argument follows directly after single char option
            optArgFound = true; // is non-empty by definition
            optArg = optName.substr(1,string::npos);
            optName.erase(1,string::npos);
          }
        }
        // search for option argument directly following option separated by equal sign
        string::size_type n = optName.find_first_of('=');
        if (n!=string::npos) {
          optArgFound = true; // explicit specification, counts as option argument even if empty string
          optArg = optName.substr(n+1,string::npos);
          optName.erase(n,string::npos);
        }
        // search for option descriptor
        const CmdLineOptionDescriptor *optionDescP = optionDescriptors;
        bool optionFound = false;
        while (optionDescP && (optionDescP->longOptionName!=NULL || optionDescP->shortOptionChar!='\x00')) {
          // not yet end of descriptor list
          if (
            (longOpt && optName==optionDescP->longOptionName) ||
            (!longOpt && optName[0]==optionDescP->shortOptionChar)
          ) {
            // option match found
            if (!optionDescP->withArgument) {
              // option without argument
              if (optArgFound) {
                fprintf(stderr, "Option '%s' does not expect an argument\n", optName.c_str());
                showUsage();
                terminateApp(EXIT_FAILURE);
              }
            }
            else {
              // option with argument
              if (!optArgFound) {
                // check for next arg as option arg
                if (rawArgIndex<aArgc-1) {
                  // there is a next argument, use it as option argument
                  optArgFound = true;
                  optArg = aArgv[rawArgIndex++];
                }
              }
              if (!optArgFound) {
                fprintf(stderr, "Option '%s' requires an argument\n", optName.c_str());
                showUsage();
                terminateApp(EXIT_FAILURE);
              }
            }
            // now have option processed by subclass
            if (!processOption(*optionDescP, optArg.c_str())) {
              // not processed, store instead
              if (optionDescP->longOptionName)
                optName = optionDescP->longOptionName;
              else
                optName[0] = optionDescP->shortOptionChar;
              // save in map
              options[optName] = optArg;
            }
            optionFound = true;
            break;
          }
          // next in list
          optionDescP++;
        }
        if (!optionFound) {
          fprintf(stderr, "Unknown Option '%s'\n", optName.c_str());
          showUsage();
          terminateApp(EXIT_FAILURE);
        }
      }
      else {
        // non-option argument
        // - have argument processed by subclass
        if (!processArgument(argP)) {
          // not processed, store instead
          arguments.push_back(argP);
        }
      }
      // next argument
      rawArgIndex++;
    }
  }
}


size_t CmdLineApp::numArguments()
{
  return arguments.size();
}


void CmdLineApp::resetCommandLine()
{
  invocationName.clear();
  synopsis.clear();
  options.clear();
  arguments.clear();
}


const char *CmdLineApp::getOption(const char *aOptionName)
{
  const char *opt = NULL;
  OptionsMap::iterator pos = options.find(aOptionName);
  if (pos!=options.end()) {
    opt = pos->second.c_str();
  }
  return opt;
}


const char *CmdLineApp::getArgument(size_t aArgumentIndex)
{
  if (aArgumentIndex>arguments.size()) return NULL;
  return arguments[aArgumentIndex].c_str();
}

