/*
 * HexEncoding.h
 *
 * Created on: Aug 6, 2015
 * Author: Francesco Guatieri
 */

#ifndef SOURCE_HEXENCODING_H_
#define SOURCE_HEXENCODING_H_

#include <cstdint>  // For uint8_t, uint16_t, etc.
#include <iostream> // For std::cerr
#include <string>
#include <vector> // For ToBin examples if they were to take vectors

// Using std namespace for convenience, similar to StringUtils.h
using std::cerr;
using std::cout; // If ToBin uses cout for example, though current ones return
                 // string
using std::endl;
using std::string;

inline int FromHexDigit(char c) // Marked inline
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  cerr << "Error: unrecognized hex digit: \"" << c << "\"\n";
  return 0; // Or throw an error
}

// Template functions are implicitly inline and definitions must be in header
template <class T> T FromHex(const char *Data) {
  T TempRes = 0;
  if (!Data)
    return TempRes; // Basic null check

  for (unsigned int i = 0; i < sizeof(T) * 2 && Data[i] != '\0';
       ++i) // Added check for null terminator
    TempRes = (TempRes * 16) + FromHexDigit(Data[i]);
  // Original was: TempRes = (TempRes*16) + FromHexDigit(*(Data++)); which is
  // fine.

  return TempRes;
}

inline char ToHexDigit(int v) // Marked inline
{
  if (v < 0) {
    cerr << "Illegal (negative) value to be cast as hex digit\n";
    return '0';
  };
  if (v < 10)
    return (char)('0' + v); // Explicit cast
  if (v < 16)
    return (char)('A' + v - 10); // Explicit cast

  cerr << "Illegal value to be cast as hex digit\n";
  return '0';
}

inline string HexDecode4(string s) { // Marked inline
  string TempRes;
  TempRes.reserve(s.length() / 4); // Approximate reservation
  for (unsigned int i = 0; i + 3 < s.length(); i += 4) {
    // Original: TempRes += char(FromHexDigit(s[i+2])*16 +
    // FromHexDigit(s[i+3])); This seems to skip first two hex chars of every
    // four. Assuming it's intentional.
    TempRes += (char)(FromHexDigit(s[i + 2]) * 16 + FromHexDigit(s[i + 3]));
  };
  return TempRes;
}

inline string HexDecode(string s) { return HexDecode4(s); } // Marked inline

inline string HexDecode2(string s) { // Marked inline
  string TempRes;
  if (s.length() % 2 != 0) {
    // Handle odd length string if necessary, e.g., by ignoring last char or
    // erroring cerr << "Warning: Odd length hex string for HexDecode2" << endl;
  }
  TempRes.reserve(s.length() / 2);
  for (unsigned int i = 0; i + 1 < s.length(); i += 2)
    TempRes.push_back((char)(FromHexDigit(s[i]) * 16 + FromHexDigit(s[i + 1])));
  return TempRes;
}

// Template function
template <class T> string ToHex(T Val) {
  // Ensure T is a plain old data type for this byte-wise conversion to work
  // safely. static_assert(std::is_pod<T>::value || std::is_trivial<T>::value,
  // "ToHex requires POD/trivial type");
  uint8_t *p = (uint8_t *)&Val;
  string TempRes(sizeof(T) * 2, '0');      // Initialize with '0'
  char *c = &(TempRes[sizeof(T) * 2 - 1]); // Pointer to last char

  // This loop processes bytes from LSB to MSB if system is little-endian,
  // filling the string from right to left.
  for (unsigned int i = 0; i < sizeof(T); ++i) {
    uint8_t current_byte = p[i];               // Read byte
    *c-- = ToHexDigit(current_byte % 16);      // Lower nibble
    *c-- = ToHexDigit(current_byte / 16 % 16); // Upper nibble
  };
  return TempRes;
}

// These ToBin functions are fine as inline in a header.
inline string ToBin(uint64_t Val) {
  string TempRes(
      79,
      ' '); // 8 bytes * 8 bits + 7 spaces = 64 + 7 = 71. 8 * (8 bits + 1 space)
            // - 1 space = 8*9-1 = 71. 8 * 10 - 1 = 79 implies 9 chars per byte
            // + a space between bytes. Looks like "xxxxxxxx xxxxxxxx ..."
            // format, 8 bits, space, 8 bits... with one extra space for nibble
            // formatting Original: 8 bytes * (8 bits + 1 space_for_nibble +
            // 1_space_between_bytes) - 1_trailing_space = 8 * 10 -1 = 79
  for (int i = 0; i < 8; ++i) {                // Iterate through bytes
    uint8_t c = (Val >> ((7 - i) * 8)) & 0xFF; // Get i-th byte from MSB
    for (int j = 0; j < 8; ++j) { // Iterate through bits of that byte
      // TempRes position: i*(8_bits + 2_spaces) + j_th_bit +
      // (j>3?1:0)_nibble_space
      TempRes[i * 10 + j + (j > 3 ? 1 : 0)] =
          (char)('0' + ((c >> (7 - j)) & 0x01));
    }
  }
  return TempRes;
}

inline string ToBin(uint32_t Val) {
  string TempRes(39, ' '); // 4 * 10 - 1 = 39
  for (int i = 0; i < 4; ++i) {
    uint8_t c = (Val >> ((3 - i) * 8)) & 0xFF;
    for (int j = 0; j < 8; ++j)
      TempRes[i * 10 + j + (j > 3 ? 1 : 0)] =
          (char)('0' + ((c >> (7 - j)) & 0x01));
  }
  return TempRes;
}

inline string ToBin(uint16_t Val) {
  string TempRes(19, ' '); // 2 * 10 - 1 = 19
  for (int i = 0; i < 2; ++i) {
    uint8_t c = (Val >> ((1 - i) * 8)) & 0xFF;
    for (int j = 0; j < 8; ++j)
      TempRes[i * 10 + j + (j > 3 ? 1 : 0)] =
          (char)('0' + ((c >> (7 - j)) & 0x01));
  }
  return TempRes;
}

// Template function
template <class T>
string DestToHex(const T *Val, unsigned int Length = sizeof(T)) {
  if (!Val)
    return "";                             // Null check
  const uint8_t *p = (const uint8_t *)Val; // Const correct
  string TempRes(Length * 2, '0');
  char *c = &(TempRes[Length * 2 - 1]); // Pointer to last char

  for (unsigned int i = 0; i < Length; ++i) {
    uint8_t current_byte = p[i];
    *c-- = ToHexDigit(current_byte % 16);
    *c-- = ToHexDigit(current_byte / 16 % 16);
  };
  return TempRes;
}

#endif /* SOURCE_HEXENCODING_H_ */