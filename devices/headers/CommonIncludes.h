/*
 * CommonIncludes.h
 *
 * Created on: 18/mar/2014
 * Author: Francesco Guatieri
 */

// hash++ option -g
// hash++ option -rdynamic
// hash++ option -lpthread

#ifndef COMMONINCLUDES_H_
#define COMMONINCLUDES_H_

#include <stdio.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <signal.h> // For signal, SIGSEGV

#ifndef NOPSTREAMS
// This absolute path is problematic for portability.
// CMake should add the parent directory of 'pstreams' to include paths,
// then this could be #include "pstreams/pstream.h"
// The current CMakeLists.txt adds ADCBoardControlPython/pstreams, so #include
// "pstream.h" should work if pstream.h is directly in
// ADCBoardControlPython/pstreams/ If structure is
// ADCBoardControlPython/pstreams/PStreams/pstream.h, then #include
// "PStreams/pstream.h"
#include "pstream.h" // Assuming pstreams/ is in include path and pstream.h is there
    // Or adjust to "PStreams/pstream.h" if it's in a subfolder named PStreams
#else
#include "PStreams/pstream.h" // This implies a PStreams subdirectory. Match your actual structure.
#endif

// This using directive is broad. It's often better to qualify (std::string,
// std::cout) or use 'using std::string;' etc. for specific types in headers.
// However, to match original project style:
using namespace std;

#define Pi (3.1415926535897932384626433832795028841971693993751)
#define NepherE (2.7182818284590452353602874713526624977572470937000)
#define TwoPi (2 * Pi)

#ifndef SEGHANDLER
#define SEGHANDLER

// Standard values if these are not provided by libraries.
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef SIGSEGV
#define SIGSEGV 11 // This might differ per OS; signal.h usually defines it.
#endif

#include <execinfo.h> // For backtrace, backtrace_symbols_fd (GNU extension mostly)

// Marked INLINE
inline void SegFaultHandler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1); // Exiting from a signal handler directly can be risky.
           // std::quick_exit or _Exit might be safer if available and
           // appropriate.
}

// Marked INLINE
inline bool InstallSegFaultHandler() {
  // signal() returns SIG_ERR on error. Check it.
  if (signal(SIGSEGV, SegFaultHandler) == SIG_ERR) {
    // perror("Signal SIGSEGV registration failed"); // From <stdio.h> or
    // <cstdio>
    return false;
  }
  return true;
}

// DECLARED extern. Definition in ProjectGlobals.cpp
extern bool isSegFaultHandlerInstalled;

#endif // SEGHANDLER

#endif /* COMMONINCLUDES_H_ */