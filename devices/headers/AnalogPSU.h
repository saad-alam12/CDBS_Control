/*
 * AnalogPSU.h (Refactored for Pybind11, command checksums, logging AND IGNORING
 * 0xF00 for return value)
 */

#ifndef SOURCE_ANALOGPSU_H_
#define SOURCE_ANALOGPSU_H_

#include "CommonIncludes.h" // Includes iostream, string, vector, using namespace std
#include "Error.h"          // Specifically for Warn, Shout
#include "FGUSBBulk.h" // Includes FGBulk.h, Hex.h, Error.h, StringUtils.h, libusb, etc.
#include "Hex.h"   // Specifically for ToHex, ToBin used in logging
#include <cstring> // For memset
#include <stdint.h>

class FGAnalogPSUInterface {
public:
  static constexpr uint32_t ExpectedMagic = 0xA4A7051F;

#pragma pack(push, 1)
  class Status_t {
    // --- Status_t definition remains the same ---
  public:
    uint32_t MagicNo;
    uint16_t Checksum;
    uint16_t SequenceNo;
    int16_t Response;

    int16_t ADCA[4];
    uint16_t ADCB[4];

    uint16_t DACA;
    uint16_t DACB;
    uint8_t Relay;

    uint8_t SetMask;

    uint16_t ComputeChecksum() {
      uint16_t TempRes = 0xFFFF;
      uint16_t *p = (uint16_t *)this;
      for (unsigned int i = 0; i * 2 < sizeof(struct Status_t); ++i) {
        TempRes ^= *(p++);
      }
      return TempRes;
    }
  };
#pragma pack(pop)

  // --- Member variables remain the same ---
  FGUSBBulk Bridge;
  int16_t ADCA[4];
  uint16_t ADCB[4];
  uint16_t DACA_val;
  uint16_t DACB_val;
  uint8_t Relay_val;
  uint16_t SequenceNo_val;
  uint16_t Errors;
  bool Verbose = true;

  // --- Constructor, Open, Close, operator bool remain the same ---
  FGAnalogPSUInterface()
      : DACA_val(0), DACB_val(0), Relay_val(0), SequenceNo_val(0), Errors(0) {
    Open();
  }
  FGAnalogPSUInterface(const FGAnalogPSUInterface &) = delete;
  bool Open() {
    Close();
    bool success = Bridge.OpenDevice(0xA0A0, 0x000C, 0);
    if (Verbose && success)
      std::cout << "Refactored AnalogPSU: USB Device Opened." << std::endl;
    else if (Verbose && !success)
      std::cout << "Refactored AnalogPSU: Failed to open USB Device."
                << std::endl;
    return success;
  }
  bool Close() {
    if (!Bridge)
      return false;
    Bridge.CloseDevice();
    if (Verbose)
      std::cout << "Refactored AnalogPSU: USB Device Closed." << std::endl;
    return true;
  }
  operator bool() { return Bridge; }

  // --- Setters and Readout remain the same ---
  bool SetDACA(uint16_t A) {
    Status_t cmdStatus;
    memset(&cmdStatus, 0, sizeof(cmdStatus));
    cmdStatus.MagicNo = ExpectedMagic;
    cmdStatus.SetMask = 1;
    cmdStatus.DACA = A;
    return Query(cmdStatus);
  }
  bool SetDACB(uint16_t B) {
    Status_t cmdStatus;
    memset(&cmdStatus, 0, sizeof(cmdStatus));
    cmdStatus.MagicNo = ExpectedMagic;
    cmdStatus.SetMask = 2;
    cmdStatus.DACB = B;
    return Query(cmdStatus);
  }
  bool SetRelay(bool Power) {
    Status_t cmdStatus;
    memset(&cmdStatus, 0, sizeof(cmdStatus));
    cmdStatus.MagicNo = ExpectedMagic;
    cmdStatus.SetMask = 4;
    cmdStatus.Relay = Power ? 1 : 0;
    return Query(cmdStatus);
  }
  bool Readout() {
    Status_t cmdStatus;
    memset(&cmdStatus, 0, sizeof(cmdStatus));
    cmdStatus.MagicNo = ExpectedMagic;
    cmdStatus.SetMask = 0;
    return Query(cmdStatus);
  }

  // --- Query method with MODIFIED return logic ---
  bool Query(Status_t CommandToSend) {
    if (!Bridge && !Open()) {
      Shout("Refactored AnalogPSU Query: Unable to open USB interface.", false);
      return false; // Communication failed
    }

    // Apply command checksum logic (same as before)
    CommandToSend.Checksum = 0;
    CommandToSend.Checksum = CommandToSend.ComputeChecksum();

    if (Verbose) {
      // Logging for Sending Command (same as before)
      std::cout << "--- REFACTORED Sending Command to Analog Board ---"
                << std::endl;
      std::cout << "  Raw Bytes (as sent): "
                << DestToHex((uint8_t *)&CommandToSend, sizeof(Status_t))
                << std::endl;
      // ... (print other fields: MagicNo, SetMask, Cmd values, Checksum) ...
      std::cout << "  MagicNo  : 0x" << std::hex << CommandToSend.MagicNo
                << std::dec << std::endl;
      std::cout << "  SetMask  : 0b"
                << ToBin(static_cast<uint16_t>(CommandToSend.SetMask)) << " (0x"
                << std::hex << (int)CommandToSend.SetMask << std::dec << ")"
                << std::endl;
      if (CommandToSend.SetMask & 1)
        std::cout << "  DACA Cmd : " << CommandToSend.DACA << std::endl;
      if (CommandToSend.SetMask & 2)
        std::cout << "  DACB Cmd : " << CommandToSend.DACB << std::endl;
      if (CommandToSend.SetMask & 4)
        std::cout << "  Relay Cmd: " << (int)CommandToSend.Relay << std::endl;
      std::cout << "  Checksum (calculated and sent): 0x" << std::hex
                << CommandToSend.Checksum << std::dec << std::endl;
      std::cout << "------------------------------------" << std::endl;
    }

    if (!Bridge.Bridge.Write(1, (uint8_t *)&CommandToSend, sizeof(Status_t))) {
      Shout("Refactored AnalogPSU Query: Unable to write to USB interface.",
            false);
      return false; // Communication failed
    }

    Status_t ResponseStatus;
    memset(&ResponseStatus, 0, sizeof(ResponseStatus));
    if (!Bridge.Bridge.Read(1, (uint8_t *)&ResponseStatus, sizeof(Status_t))) {
      Shout("Refactored AnalogPSU Query: Unable to read from USB interface.",
            false);
      return false; // Communication failed
    }

    if (Verbose) {
      // Logging for Received Response (same as before)
      std::cout << "--- REFACTORED Received Response from Analog Board ---"
                << std::endl;
      std::cout << "  Raw Bytes (received): "
                << DestToHex((uint8_t *)&ResponseStatus, sizeof(Status_t))
                << std::endl;
      // ... (print other fields: MagicNo, Checksum, SequenceNo, Response, Relay
      // etc.) ...
      std::cout << "  MagicNo    (recv): 0x" << std::hex
                << ResponseStatus.MagicNo << std::dec << std::endl;
      std::cout << "  Checksum   (recv): 0x" << std::hex
                << ResponseStatus.Checksum << std::dec << std::endl;
      std::cout << "  SequenceNo (recv): " << ResponseStatus.SequenceNo
                << std::endl;
      std::cout << "  Response   (recv): 0x" << std::hex
                << ResponseStatus.Response << std::dec << " (Error Word)"
                << std::endl;
      std::cout << "  Relay      (recv): " << (int)ResponseStatus.Relay
                << std::endl;
      std::cout << "---------------------------------------" << std::endl;
    }

    // Check USB packet validity (MagicNo, Packet Checksum)
    if (ResponseStatus.MagicNo != ExpectedMagic) {
      Shout("Refactored AnalogPSU Query: Magic number in response does not "
            "correspond.",
            false);
      return false; // Packet integrity failed
    }
    if (ResponseStatus.ComputeChecksum() != 0) {
      if (Verbose) {
        std::cout
            << "  Computed Checksum over received packet (should be 0): 0x"
            << std::hex << ResponseStatus.ComputeChecksum() << std::dec
            << std::endl;
      }
      Shout("Refactored AnalogPSU Query: Checksum in response does not "
            "correspond.",
            false);
      return false; // Packet integrity failed
    }

    // Communication and packet structure seem OK. Store results.
    this->Errors =
        ResponseStatus.Response; // Store the device's status/error code
    // ... (copy other fields like ADCA, ADCB, DACA_val, etc. as before) ...
    for (int i = 0; i < 4; ++i) {
      this->ADCA[i] = ResponseStatus.ADCA[i];
      this->ADCB[i] = ResponseStatus.ADCB[i];
    }
    this->DACA_val = ResponseStatus.DACA;
    this->DACB_val = ResponseStatus.DACB;
    this->Relay_val = ResponseStatus.Relay;
    this->SequenceNo_val = ResponseStatus.SequenceNo;

    // ---vvv--- MODIFIED LOGIC HERE ---vvv---
    // Check the device's reported error code, BUT only return 'false' for
    // critical errors. Based on colleague's advice, we treat 0xF00 as
    // non-critical for the return value.
    if (this->Errors != 0) {
      if (this->Errors == 0xF00) {
        // It's the specific code we decided to ignore for success/failure
        // reporting
        if (Verbose) {
          // Log it distinctly, perhaps using Warn or just cout
          // Using Warn implies it's still noteworthy
          Warn("Refactored AnalogPSU Query: Device reported status 0x" +
                   ToHex(this->Errors) +
                   " (Ignoring for success/fail return value).",
               false);
        }
        // *** Do NOT return false here - proceed to return true ***
      } else {
        // It's a *different* non-zero error code. Treat this as a failure.
        if (Verbose) {
          Shout("Refactored AnalogPSU Query: Device reported CRITICAL error "
                "word: 0x" +
                    ToHex(this->Errors),
                false);
        }
        return false; // Return false for other errors
      }
    }
    // Return true if Errors was 0 OR if it was specifically 0xF00 (and not
    // handled above)
    return true;
    // ---^^^--- END OF MODIFIED LOGIC ---^^^---

  }; // End of Query method

  void Dump(std::ostream &Str = std::cout) { /* ... as before ... */
    Str << "ADC A: ";
    for (auto &a : ADCA)
      Str << '\t' << a;
    Str << '\n';
    Str << "ADC B: ";
    for (auto &a : ADCB)
      Str << '\t' << a;
    Str << '\n';
    Str << "DAC A (readback): " << DACA_val << '\n';
    Str << "DAC B (readback): " << DACB_val << '\n';
    Str << "Relay (readback): " << int(Relay_val) << '\n';
    Str << "Sequence no (readback): " << SequenceNo_val << '\n';
    Str << "Last Device Error Word: 0x" << ToHex(Errors) << '\n';
  }
};

#endif /* SOURCE_ANALOGPSU_H_ */