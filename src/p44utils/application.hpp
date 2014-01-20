//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__application__
#define __p44utils__application__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  class MainLoop;

  class Application : public P44Obj
  {
    MainLoop &mainLoop;
  public:
    /// constructors
    Application(MainLoop &aMainLoop);
    Application();

    /// destructor
    virtual ~Application();

    /// main routine
    /// @param argc argument count as passed to C-level main() entry point
    /// @param argv argument pointer array as passed to C-level main() entry point
    virtual int main(int argc, char **argv);

    /// get shared instance (singleton)
    Application *sharedApplication();

    /// terminate app
    /// @param aExitCode the exit code to return to the parent
    void terminateApp(int aExitCode);

  protected:

    /// daemonize
    void daemonize();

    /// start running the app's main loop
    int run();

    /// scheduled to run when mainloop has started
    virtual void initialize();
  };


  /// Command line option descriptor
  /// @note a descriptor with both longOptionName==NULL and shortOptionChar=0 terminates a list of option descriptors
  typedef struct {
    char shortOptionChar; ///< the short option name (single character) or 0/NUL if none
    const char *longOptionName; ///< the long option name (string) or NULL if none
    bool withArgument; ///< true if option has an argument (separated by = or next argument)
    const char *optionDescription; ///< the description of the option, can have multiple lines separated by \n
    int optionIdentifier; ///< an optional identifier
  } CmdLineOptionDescriptor;

  typedef vector<string> ArgumentsVector;
  typedef map<string,string> OptionsMap;

  class CmdLineApp : public Application
  {
    typedef Application inherited;

    const CmdLineOptionDescriptor *optionDescriptors;

    string invocationName;
    string synopsis;
    OptionsMap options;
    ArgumentsVector arguments;

  public:

    /// constructors
    CmdLineApp(MainLoop &aMainLoop);
    CmdLineApp();

    /// destructor
    virtual ~CmdLineApp();

  protected:

    /// set command description constants (option definitions and synopsis)
    /// @param aSynopsis short usage description, used in showUsage(). %1$s will be replaced by invocationName
    /// @param aCmdLineOptionDescriptors pointer to array of descriptors for the options
    void setCommandDescriptors(const char *aSynopsis, const CmdLineOptionDescriptor *aOptionDescriptors);

    /// show usage, consisting of invocationName + synopsis + option descriptions
    void showUsage();

    /// parse command line.
    /// @param aArgc argument count as passed to C-level main() entry point
    /// @param aArgv argument pointer array as passed to C-level main() entry point
    /// @note setOptionDescriptors() must be called before using this method
    /// @note this method might call terminateApp() in case of command line syntax errors
    void parseCommandLine(int aArgc, char **aArgv);

    /// reset internal argument lists (to save memory when arguments are all processed)
    void resetCommandLine();

    /// process a command line option. Override this to implement processing command line options
    /// @param aOptionDescriptor the descriptor of the option
    /// @param aOptionValue the value of the option, empty string if option has no value
    /// @return true if option has been processed; false if option should be stored for later reference via getOption()
    /// @note will be called from parseCommandLine()
    /// @note base class will process "help" option by showing usage and terminating the app with EXIT_SUCCESS exit code
    virtual bool processOption(const CmdLineOptionDescriptor &aOptionDescriptor, const char *aOptionValue);

    /// process a non-option command line argument
    /// @param aArgument non-option argument
    /// @return true if argument has been processed; false if argument should be stored for later reference via getArgument()
    /// @note will be called from parseCommandLine()
    virtual bool processArgument(const char *aArgument) { return false; /* not processed, store */ };

    /// get app invocation name
    /// @return application invocation name (argv[0])
    /// @note parseCommandLine() must be called before using this method
    const char *getInvocationName();

    /// get option
    /// @param aOptionName the name of the option (longOptionName if exists, shortOptionChar if no longOptionName exists)
    /// @return NULL if option was not specified on the command line, empty string for options without argument, option's argument otherwise
    /// @note parseCommandLine() must be called before using this method
    const char *getOption(const char *aOptionName);

    /// @param aOptionName the name of the option (longOptionName if exists, shortOptionChar if no longOptionName exists)
    /// @param aInteger will be set with the integer value of the option, if any
    /// @return true if option was specified and had a valid integer argument, false otherwise (aInteger will be untouched then)
    /// @note parseCommandLine() must be called before using this method
    bool getIntOption(const char *aOptionName, int &aInteger);

    /// @param aOptionName the name of the option (longOptionName if exists, shortOptionChar if no longOptionName exists)
    /// @param aCString will be set to point to the option argument cstring, if any
    /// @return true if option was specified and had an option argument
    /// @note parseCommandLine() must be called before using this method
    bool getStringOption(const char *aOptionName, const char *&aCString);

    /// @param aOptionName the name of the option (longOptionName if exists, shortOptionChar if no longOptionName exists)
    /// @param aString will be set to point to the option argument cstring, if any
    /// @return true if option was specified and had an option argument
    /// @note parseCommandLine() must be called before using this method
    bool getStringOption(const char *aOptionName, string &aString);


    /// get number of stored options
    /// @return number of options present and not already processed by processOption() returning true
    /// @note parseCommandLine() must be called before using this method
    size_t numOptions();

    /// get non-option argument
    /// @param aArgumentIndex the index of the argument (0=first non-option argument, 1=second non-option argument, etc.)
    /// @return NULL if aArgumentIndex>=numArguments(), argument otherwise
    /// @note parseCommandLine() must be called before using this method
    const char *getArgument(size_t aArgumentIndex);

    /// get number of (non-processed) arguments
    /// @return number of arguments not already processed by processArgument() returning true
    /// @note parseCommandLine() must be called before using this method
    size_t numArguments();

  };


} // namespace p44


#endif /* defined(__p44utils__application__) */
