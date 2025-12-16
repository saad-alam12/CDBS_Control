/*
 * StringUtils.h
 *
 * Created on: 07/mag/2014
 * Author: Francesco Guatieri
 */

#ifndef STRINGUTILS_H_
#define STRINGUTILS_H_

#include <cmath>   // For NAN, INFINITY, fabs (used by jdtos, safestod)
#include <cstring> // For strcpy
#include <iomanip> // Potentially for stream manipulators if used by anytos/dtos more complexly
#include <sstream> // For std::stringstream, std::istringstream
#include <string>
#include <typeinfo> // For typeid
#include <vector>

// hash++ option -fexceptions // This is a compiler option

#include "Error.h" // For Utter, though not directly used by all functions here

// Using std namespace for convenience, as in original project structure
// This is generally fine within .h files if they are part of a closed project
// and not general-purpose libraries meant for wide distribution.
// CommonIncludes.h establishes `using namespace std;` so it might be implicitly
// available. To be safe, qualify types like std::string, std::vector etc. if
// CommonIncludes.h is not guaranteed. Assuming CommonIncludes.h is included
// before this by files using StringUtils. For standalone safety:
using std::cerr; // From <iostream> via CommonIncludes.h or Error.h
using std::endl; // From <iostream>
using std::string;
using std::stringstream;
using std::vector;

class FGStringStream {
public:
  stringstream Stream;

  FGStringStream() {};

  inline operator string() { return Stream.str(); };
  // This operator is unsafe as it returns a pointer to a temporary.
  // To fix properly, either remove it, or ensure FGStringStream object's
  // lifetime or have it manage an internal string buffer. For now, the warning
  // will persist, but it won't stop compilation.
  inline operator const char *() { return Stream.str().c_str(); }
};

template <class T>
inline FGStringStream &operator<<(FGStringStream &os, const T &v) {
  os.Stream << v;
  return os;
}

// This overload seems to cause ambiguity with the one above if not careful.
// Typically, one would pass by reference to modify. If by value, it implies a
// copy. Let's assume the intent was similar to standard stream operators. For
// safety, often only the by-reference version is provided for operator<<
// modification. If this is problematic, it might need review.
template <class T>
inline FGStringStream operator<<(FGStringStream os,
                                 const T &v) // Note: os is by value here
{
  os.Stream << v;
  return os;
}

#define FGSS FGStringStream() // This macro creates a temporary FGStringStream

inline bool isBlankspace(char c) { return c == ' ' || c == '\t' || c == '\n'; }

inline string Trim(string sss) {
  size_t Start; // Use size_t for indices
  size_t End;

  for (Start = 0; Start < sss.size() && isBlankspace(sss[Start]); ++Start)
    ;
  // Check if string is all blank space
  if (Start == sss.size())
    return "";

  for (End = sss.size() - 1; End > Start && isBlankspace(sss[End]); --End)
    ;
  // If Start is 0 and sss[0] is not blank, and End becomes < Start (e.g. single
  // char not blank) then End-Start+1 could be problematic. sss.size()-1 can be
  // -1 if size is 0. A safer end condition or check:
  if (End < Start && !isBlankspace(sss[Start]))
    End = Start; // for single non-blank char
  else if (End < Start)
    return ""; // Should not happen if start guard passed and string not empty

  return sss.substr(Start, End - Start + 1);
}

inline vector<string> StringSplit(const char *Str, char Sep) {
  if (!Str || !Str[0])
    return vector<string>(); // Added null check for Str

  vector<string> TempRes;
  string sss = "";
  const char *S = Str; // Keep original Str const

  while (*S) {
    if (*S == Sep) {
      TempRes.push_back(sss);
      sss = "";
    } else
      sss.push_back(*S);
    ++S;
  };
  TempRes.push_back(sss);
  return TempRes;
}

inline vector<string> StringSplit(const string &Str, char Sep) {
  return StringSplit(Str.c_str(), Sep);
}

inline vector<string> StringSplit(const string &Str, const string &Sep) {
  if (!Str.size())
    return vector<string>();
  if (Sep.empty()) { // Handle empty separator: return vector of single
                     // characters
    vector<string> TempRes;
    for (char c : Str) {
      TempRes.push_back(string(1, c));
    }
    return TempRes;
  }

  size_t StringPos = 0;
  size_t NextPivot = 0;
  vector<string> TempRes;

  while ((NextPivot = Str.find(Sep, StringPos)) != string::npos) {
    TempRes.push_back(Str.substr(StringPos, NextPivot - StringPos));
    StringPos = NextPivot + Sep.size();
  };
  TempRes.push_back(Str.substr(StringPos));
  return TempRes;
}

inline string StringJoin(vector<string> V, char Separator) {
  string sss = "";
  for (unsigned int i = 0; i < V.size(); ++i)
    if (i)
      sss += Separator + V[i];
    else
      sss += V[i];
  return sss;
}

inline string StringReplace(string Object, string LookFor, string ReplaceWith) {
  if (LookFor.empty()) {
    // Original code printed to cerr but returned Object.
    // Depending on desired behavior, could return Object or throw error.
    // cerr << "Error! Cannot look for empty strings to replace.\n";
    return Object; // Or handle as error
  };
  string TempRes = ""; // Not needed if using string::replace or string::find
  size_t start_pos = 0;
  while ((start_pos = Object.find(LookFor, start_pos)) != std::string::npos) {
    Object.replace(start_pos, LookFor.length(), ReplaceWith);
    start_pos += ReplaceWith.length(); // Advance past the replacement
  }
  return Object;
  // Original loop based logic:
  // string TempRes = "";
  // for(unsigned int i = 0; i < Object.length(); ++i) {
  //  bool Match = (i <= Object.length() - LookFor.length()); // Ensure enough
  //  chars left if(Match) {
  //      for(unsigned int j = 0; j < LookFor.length(); ++j) {
  //          if(Object[i + j] != LookFor[j]) {
  //              Match = false;
  //              break;
  //          }
  //      }
  //  }
  //  if(Match) {
  //      TempRes += ReplaceWith;
  //      i += LookFor.size() - 1;
  //  } else {
  //      TempRes += Object[i];
  //  }
  // }
  // return TempRes;
}

inline string ExtractFileName(string fPath) {
  size_t i = fPath.find_last_of('/');
  if (i == string::npos)
    return fPath; // No slash, return whole path
  return fPath.substr(i + 1);
}

inline string ExtractFilePath(string fPath) {
  size_t p = fPath.find_last_of('/');
  if (p == string::npos)
    return ""; // No path component
  return fPath.substr(0, p + 1);
}

template <class T>
inline bool stoany(string s, T &Dest, bool AbortOnError = false) {
  std::istringstream Converter(s);
  if (!(Converter >> Dest)) {
    if (AbortOnError) {
      // Ensure Utter is available (from Error.h)
      // This creates a complex dependency if StringUtils is meant to be more
      // general. For now, assuming Error.h will be included where StringUtils.h
      // is used. Convert typeid(T).name() to string safely if needed.
      const char *typeName = typeid(T).name();
      return Utter("String \"" + s + "\" cannot be casted as " +
                       string(typeName ? typeName : "unknown_type") +
                       " as expected.",
                   false);
    } else {
      return false;
    }
  };
  return true;
}

template <class T> inline string anytos(T i) {
  stringstream sss;
  sss << i;
  return sss.str();
}

inline string itos(int i) {
  stringstream sss;
  sss << i;
  return sss.str();
}

inline string uitos(unsigned int i) {
  stringstream sss;
  sss << i;
  return sss.str();
}

inline string dtos(double d) {
  stringstream sss;
  // For consistent output like "1.0" vs "1", consider std::fixed and
  // std::setprecision
  sss << d;
  return sss.str();
}

inline double safestod(string s) {
  try {
    // Using std::stod from <string>
    size_t idx = 0;
    double val = std::stod(s, &idx);
    // Check if the entire string was consumed (ignoring trailing whitespace if
    // any)
    while (idx < s.length() && isBlankspace(s[idx])) {
      idx++;
    }
    if (idx < s.length()) { // Unconverted characters remain
      return NAN;           // Or handle as error
    }
    return val;
  } catch (...) { // Catches std::invalid_argument, std::out_of_range
    return NAN;
  }
}

inline string mdtos(double d) {
  return StringReplace(StringReplace(dtos(d), "e", "*^"), "inf", "Infinity");
}
inline string himdtos(double d) {
  stringstream ss;
  ss.precision(18); // std::numeric_limits<double>::max_digits10 could be better
  ss << d;
  return StringReplace(StringReplace(ss.str(), "e", "*^"), "inf", "Infinity");
}

inline string jdtos(double x) {
  if (x != x)
    return "NaN"; // Check for NAN
  if (std::isinf(x))
    return string(x > 0 ? "" : "-") + "INFINITY"; // Check for INFINITY
  return dtos(x);
}
inline string jsondtos(double x) {
  if (x != x)
    return "null"; // NAN to null
  if (std::isinf(x))
    return "null"; // INFINITY to null
  return dtos(x);
}

inline uint64_t stoui64(const string &s) {
  uint64_t TempRes = 0; // Initialize
  stringstream ss(s);
  ss >> TempRes;
  // Add error checking: if (ss.fail() || !ss.eof()) { /* error */ }
  return TempRes;
}

inline void dtoa(double d, char *Dest) {
  // Ensure Dest is large enough. This is unsafe without a size parameter.
  // Prefer std::string or snprintf.
  stringstream sss;
  sss << d;
  // strcpy is unsafe. If sss.str() is too long, buffer overflow.
  // Consider: strncpy(Dest, sss.str().c_str(), size_of_dest_buffer - 1);
  // Dest[size_of_dest_buffer-1] = '\0'; For now, keeping original logic but
  // acknowledging risk.
  if (Dest)
    strcpy(Dest, sss.str().c_str());
}

inline unsigned char Hex2Char(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  // Original returned 0, which could be a valid hex value.
  // Better to indicate error if char is invalid.
  // For now, keeping original:
  return 0;
}

inline string Hex2String(string Data) {
  if (Data.length() % 2) {
    // cerr << "Error: unable to interpret HEX data. Uneven length of data.\n";
    return string(); // Return empty string on error
  };

  string TempRes;
  TempRes.reserve(Data.length() / 2); // Pre-allocate

  for (unsigned int i = 0; i < Data.length(); i += 2) {
    unsigned char c_val; // Renamed from c to avoid conflict
    c_val = Hex2Char(Data[i]) * 16 + Hex2Char(Data[i + 1]);
    TempRes.push_back(c_val);
  };
  return TempRes;
}

inline string UCase(string sss) {
  for (char &c : sss) { // Iterate by reference to modify
    if (c >= 'a' && c <= 'z')
      c = c + ('A' - 'a');
  }
  return sss;
}

inline string LCase(string sss) {
  for (char &c : sss) { // Iterate by reference
    if (c >= 'A' && c <= 'Z')
      c = c + ('a' - 'A');
  }
  return sss;
}

inline string StringPurge(string S, string Chars) {
  string TempRes;
  TempRes.reserve(S.size());
  for (char c_s : S) { // Use different variable name from outer S
    bool found = false;
    for (char c_chars : Chars) {
      if (c_s == c_chars) {
        found = true;
        break;
      }
    }
    if (!found) {
      TempRes.push_back(c_s);
    }
  }
  // Original: if(Chars.find(c) == string::npos) TempRes.push_back(c);
  // The original is more efficient. Reverting to that style.
  // TempRes.clear(); TempRes.reserve(S.size()); // Reset for original logic
  // for(char c : S) {
  //  if(Chars.find(c) == string::npos) TempRes.push_back(c);
  // }
  // The most efficient C++20 way is std::erase_if, or remove_if + erase
  // pre-C++20. For simplicity and to match original intent, a loop based on
  // string::find is fine.
  S.erase(std::remove_if(
              S.begin(), S.end(),
              [&](char c) { return Chars.find(c) != std::string::npos; }),
          S.end());
  return S; // Return the modified string S
}

inline string RightPad(const string &Origin, string::size_type Amount) {
  if (Origin.size() >= Amount)
    return Origin.substr(0, Amount);
  return Origin + string(Amount - Origin.size(), ' ');
}

inline string Ess(int i) {
  if (i == 1 || i == -1)
    return "";
  return "s";
} // Consider -1 case

#endif /* STRINGUTILS_H_ */