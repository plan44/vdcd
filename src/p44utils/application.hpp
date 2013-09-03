//
//  application.hpp
//  p44utils
//
//  Created by Lukas Zeller on 02.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44utils__application__
#define __p44utils__application__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  class MainLoop;

  class Application : public P44Obj
  {
    MainLoop *mainLoopP;
  public:
    /// constructor
    Application(MainLoop *aMainLoopP);

    /// default constructor
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
    bool withArgument; ///< true if option has an argument (separated by = or next argument)
    const char *longOptionName; ///< the long option name (string) or NULL if none
    char shortOptionChar; ///< the short option name (single character) or 0/NUL if none
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

    /// constructor
    CmdLineApp(MainLoop *aMainLoopP);

    /// destructor
    virtual ~CmdLineApp();

  protected:

    /// set command description constants (option definitions and synopsis)
    /// @param aCmdLineOptionDescriptors pointer to array of descriptors for the options
    /// @param aSynopsis short usage description, used in showUsage(). %1$s will be replaced by invocationName
    void setCommandDescriptors(const CmdLineOptionDescriptor *aOptionDescriptors, const char *aSynopsis = NULL);

    /// show usage, consisting of invocationName + synopsis + option descriptions
    void showUsage();

    /// parse command line.
    /// @param aArgc argument count as passed to C-level main() entry point
    /// @param aArgv argument pointer array as passed to C-level main() entry point
    /// @note setOptionDescriptors() must be called before using this method
    /// @note this method might call terminateApp() in case of command line syntax errors
    /// @note "-h" and "--help" options will always show usage and terminate the app with EXIT_SUCCESS exit code
    void parseCommandLine(int aArgc, char **aArgv);

    /// reset internal argument lists (to save memory when arguments are all processed)
    void resetCommandLine();

    /// process a command line option. Override this to implement processing command line options
    /// @param aOptionDescriptor the descriptor of the option
    /// @param aOptionValue the value of the option, empty string if option has no value
    /// @return true if option has been processed; false if option should be stored for later reference via getOption()
    /// @note will be called from parseCommandLine()
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
