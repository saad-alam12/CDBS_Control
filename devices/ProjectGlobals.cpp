#include "headers/Error.h"
#include <iostream>

// Definitions of global variables declared in Error.h

int Verbosity = 0;
std::ostream *ErrorStream = &std::cerr;
FGErrorCollector MainErrorCollector;
