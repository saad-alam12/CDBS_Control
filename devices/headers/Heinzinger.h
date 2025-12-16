#ifndef HEINZINGER_H
#define HEINZINGER_H

#include "AnalogPSU.h" // For the FGAnalogPSUInterface member
#include <stdint.h>    // For uint16_t etc.
#include <string>      // For std::string in USB path constructor

// Declaration of the HeinzingerVia16BitDAC class
class HeinzingerVia16BitDAC {
private:
  FGAnalogPSUInterface
      Interface; // Definition of FGAnalogPSUInterface comes from AnalogPSU.h

  double max_analog_in_volt;
  uint16_t max_analog_in_volt_bin;

  // These seem to be for storing current state, but are not read from in the
  // provided .cpp You might want to ensure they are updated if they are meant
  // to reflect PSU state.
  double
      set_volt_cache; // Renamed from set_volt to avoid confusion with parameter
  double set_curr_cache; // Renamed from set_curr
  bool relay_cache;      // Renamed from relay

  double max_volt;
  double max_curr;

  bool verbose;
  int _usbIndex;   // store which identical device to open

  bool update(); // This is a private helper

public:
  // Constructor - USB path-based identification
  HeinzingerVia16BitDAC(const std::string& usb_path, double max_voltage = 30000.0, double max_current = 2.0,
                        bool verbose = false, double max_input_voltage = 10.0);
  
  // Legacy constructor for backward compatibility (deprecated)
  HeinzingerVia16BitDAC(int device_index, double max_voltage, double max_current,
                        bool verbose, double max_input_voltage);
  
  // Destructor - CRITICAL for USB resource cleanup
  ~HeinzingerVia16BitDAC() {
    Interface.Close();  // Properly release USB resources
  }

  // Public interface methods
  bool switch_on();
  bool switch_off();
  bool set_voltage(double set_val);
  bool set_current(double set_val);
  bool is_relay_on() const               // true => output enabled
  {
    return Interface.Relay_val != 0;   // Relay_val comes from the board
  }

  double read_voltage();
  double read_current();
  bool set_max_volt();
  bool set_max_curr();
  void readADC();
};

#endif // HEINZINGER_H