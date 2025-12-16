/*
 * Error.h
 *
 * Created on: Sep 21, 2015
 * Author: Francesco Guatieri
 */

#ifndef FGLIBRARIES_ERROR_H_
#define FGLIBRARIES_ERROR_H_

#include <iostream> // For std::ostream, std::cerr (used by ErrorStream default)
#include <string>
#include <vector>

// DECLARATIONS of global variables. Definitions will be in ProjectGlobals.cpp
extern int Verbosity;
extern std::ostream *ErrorStream;

enum FGSeverity {
  FGErrorAnswer = 0,
  FGErrorInfo,
  FGErrorWarning,
  FGErrorError,
  FGErrorCritical,
  FGErrorReturn
};

class FGErrorCollector; // Forward declaration

typedef int (*FGErrorCollectorCallback)(FGErrorCollector &, std::string &,
                                        FGSeverity, int);

// DECLARATION of global variable. Definition in ProjectGlobals.cpp
extern FGErrorCollector MainErrorCollector;

// INLINE function definitions
inline std::string ErrorSeverityLevel(FGSeverity Severity) {
  switch (Severity) {
  case FGErrorAnswer:
    return "Result";
  case FGErrorInfo:
    return "Information";
  case FGErrorWarning:
    return "Warning";
  case FGErrorError:
    return "Error";
  case FGErrorCritical:
    return "Critical error";
  case FGErrorReturn:
    return "Return";
  };
  return "Uknown"; // Should be "Unknown"
}

// FGErrorCollector class definition (can stay in header if methods are inline
// or defined here)
class FGErrorCollector {
public:
  std::vector<std::pair<FGSeverity, std::string>> Log;
  FGErrorCollectorCallback Callback;

  FGErrorCollector()
      : Callback(nullptr) {
        }; // Make default constructor explicit if not using (nullptr) for CB
  FGErrorCollector(FGErrorCollectorCallback CB) : Callback(CB) {};

  // This method could also be in a .cpp if it were larger, but inline is fine
  // here.
  inline int Collect(std::string &Description,
                     FGSeverity Severity = FGErrorError, int ExitCode = 0) {
    if (Callback != nullptr) {
      Callback(*this, Description, Severity, ExitCode);
      return ExitCode;
    };

    // Ensure ErrorStream is non-null before dereferencing, or ensure it's
    // always valid. It's initialized to &std::cerr in ProjectGlobals.cpp.
    if (!ErrorStream)
      ErrorStream = &std::cerr; // Safety, though should be set.

    switch (Severity) {
    case FGErrorAnswer:
      *ErrorStream << "Result: ";
      break;
    case FGErrorInfo:
      *ErrorStream << "Information: ";
      break;
    case FGErrorWarning:
      *ErrorStream << "Warning: ";
      break;
    case FGErrorError:
      *ErrorStream << "Error: ";
      break;
    case FGErrorCritical:
      *ErrorStream << "Critical error: ";
      break;
    case FGErrorReturn:
      *ErrorStream << "Return: ";
      break;
    };

    *ErrorStream << Description << "\n";
    ErrorStream->flush(); // Good practice to flush after errors/warnings

    // Original code called exit() for FGErrorAnswer, FGErrorCritical,
    // FGErrorReturn. This is generally problematic for libraries. Consider
    // using exceptions instead. For now, keeping original behavior.
    if (Severity == FGErrorAnswer || Severity == FGErrorCritical ||
        Severity == FGErrorReturn)
      exit(ExitCode);
    return ExitCode;
  };
};

// INLINE function definitions using MainErrorCollector
inline int Answer(std::string What, int ExitCode = 0) {
  return MainErrorCollector.Collect(What, FGErrorAnswer, ExitCode);
}

inline int Return(std::string What, int ExitCode = 0) {
  return MainErrorCollector.Collect(What, FGErrorReturn, ExitCode);
}

inline int
Log(std::string What,
    int ExitCode = 0) // Changed name to avoid conflict if std::Log exists
{
  return MainErrorCollector.Collect(What, FGErrorInfo, ExitCode);
}

inline int Warn(std::string What, int ExitCode = 0) {
  return MainErrorCollector.Collect(What, FGErrorWarning, ExitCode);
}

inline int Shout(std::string What, int ExitCode = 0) {
  return MainErrorCollector.Collect(What, FGErrorError, ExitCode);
}

inline int Utter(std::string What, int ExitCode = 0) {
  return MainErrorCollector.Collect(What, FGErrorCritical, ExitCode);
}

#endif /* FGLIBRARIES_ERROR_H_ */