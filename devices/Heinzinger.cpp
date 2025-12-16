// Contents of PythonWrapper/ADCBoardControlPython/Heinzinger.cpp

#include "headers/CommonIncludes.h" // For std::cerr, std::cout, std::endl from <iostream>
#include "headers/Error.h" // For Utter
// LinuxUtils.h is not directly used by Heinzinger class methods, but by the
// original main() #include "headers/LinuxUtils.h" FGUSBBulk.h and AnalogPSU.h
// are included via Heinzinger.h
#include "headers/Heinzinger.h" // Includes the class DECLARATION from Heinzinger.h

#include <climits> // For UINT16_MAX
#include <cmath> // For fabs, NAN, INFINITY if any string utils use them (though not directly here)
// #include <vector> // Included via CommonIncludes.h or other headers
// #include <fstream> // Included via CommonIncludes.h for the original main's
// ofstream

// This was defined in your original Heinzinger.cpp.
// It's used in constructor, set_voltage, set_current.
// For better design, this could be a static const member in Heinzinger.h
// or passed as a constructor argument if it can vary per instance/board.
#define BOARD_MAX_VOLT 11.3

// --- Method Definitions for HeinzingerVia16BitDAC ---

// New USB path-based constructor implementation
HeinzingerVia16BitDAC::HeinzingerVia16BitDAC(
    const std::string& usb_path,
    double max_voltage,
    double max_current_param,
    bool verbose_param,
    double max_input_voltage)
    : max_volt(max_voltage),                 // Initialize from parameter
      max_curr(max_current_param),           // Initialize from parameter
      verbose(verbose_param),                // Initialize from parameter
      max_analog_in_volt(max_input_voltage), // Initialize from parameter
      set_volt_cache(0.0), set_curr_cache(0.0), relay_cache(false),
      max_analog_in_volt_bin(0), _usbIndex(0) // Initialize _usbIndex to 0 for path-based
{
  Interface.Close(); // ensure nothing is open
  // Use new path-based device opening
  if (!Interface.Bridge.OpenDeviceByPath(0xA0A0, 0x000C, 0, usb_path)) {
    Utter("Unable to open USB device at path: " + usb_path);
  }
  if (!Interface) {
    Utter("Unable to open interface to analog PSU interface board.\n");
  }

  Interface.Verbose = this->verbose; // Set interface verbosity
  
  if (BOARD_MAX_VOLT < this->max_analog_in_volt) {
    Utter("The board has insufficient output voltage to control the PSU");
  }

  // Initialize the max_analog_in_volt_bin from max_analog_in_volt
  max_analog_in_volt_bin = static_cast<uint16_t>(
      UINT16_MAX * (this->max_analog_in_volt / BOARD_MAX_VOLT));
  if (verbose) {
    std::cout << "Max analog input voltage: " << max_analog_in_volt << " V" << std::endl;
    std::cout << "Max analog input voltage bin: " << max_analog_in_volt_bin << std::endl;
  }
}

// Legacy device_index-based constructor (for backward compatibility)
HeinzingerVia16BitDAC::HeinzingerVia16BitDAC(
    int device_index,
    double max_voltage,
    double max_current_param,
    bool verbose_param,
    double max_input_voltage)
    : max_volt(max_voltage),                 // Initialize from parameter
      max_curr(max_current_param),           // Initialize from parameter
      verbose(verbose_param),                // Initialize from parameter
      max_analog_in_volt(max_input_voltage), // Initialize from parameter
      set_volt_cache(0.0), set_curr_cache(0.0), relay_cache(false),
      max_analog_in_volt_bin(0), _usbIndex(device_index)
{
  Interface.Close(); // ensure nothing is open
  // Use legacy device_index method
  if (!Interface.Bridge.OpenDevice(0xA0A0, 0x000C, 0, device_index)) {
    Utter("Unable to open USB device #" + std::to_string(device_index));
  }
  if (!Interface) {
    Utter("Unable to open interface to analog PSU interface board.\n");
  }

  Interface.Verbose = this->verbose; // Use the initialized member 'verbose'

  if (BOARD_MAX_VOLT <
      this->max_analog_in_volt) { // Use member 'max_analog_in_volt'
    Utter("The board has insufficient output voltage to control the PSU");
    // Consider throwing an exception
  }

  // Calculate max_analog_in_volt_bin using member max_analog_in_volt
  this->max_analog_in_volt_bin = static_cast<uint16_t>(
      UINT16_MAX * (this->max_analog_in_volt / BOARD_MAX_VOLT));
}

// Private helper method implementation
bool HeinzingerVia16BitDAC::update() {
  if (!Interface.Readout()) {
    std::cerr << "Unable to perform analog PSU interface readout. Will reset "
                 "interface.\n";
    // Consider more robust reset logic or error propagation
    return false;
  } else {
    // Update cache members if you intend to use them to reflect current state
    // e.g., this->relay_cache = this->Interface.Relay_val; (assuming Relay_val
    // exists and is updated in Interface)
    //      this->set_volt_cache = this->read_voltage(); // but careful about
    //      re-entrancy or performance
    return true;
  }
}

// Public method implementations
bool HeinzingerVia16BitDAC::switch_on() {
  Interface.SetRelay(false);
  // Original code had: relay = update(); return relay;
  // 'relay' was a local variable in your original main's scope or uninitialized
  // member. Assuming 'update()' returning true means the operation was
  // successful. If you want to cache the relay state: if (update()) {
  //     this->relay_cache = Interface.Relay_val; // Assuming Interface updates
  //     a readable relay state return this->relay_cache;
  // }
  // return false;
  return update(); // Simpler: return true if update (which reads back) is
                   // successful
}

bool HeinzingerVia16BitDAC::switch_off() {
  Interface.SetRelay(true);
  // Similar logic to switch_on for caching and returning state
  return update();
}

bool HeinzingerVia16BitDAC::set_voltage(
    double set_val_param) { // Renamed parameter
  if (set_val_param > this->max_volt || set_val_param < 0) {
    std::cerr << "Set voltage value lies outside of device's specified range\n";
    return false;
  }

  // Using this-> to be explicit about members
  double set_percent_of_max = set_val_param / 0.98 / this->max_volt; 
  double required_analog_volt = this->max_analog_in_volt * set_percent_of_max;
  // Ensure required_analog_volt doesn't exceed max_analog_in_volt (could happen
  // if set_val_param is at the edge due to 0.98 factor)
  if (required_analog_volt > this->max_analog_in_volt) {
    required_analog_volt = this->max_analog_in_volt;
  }
  if (required_analog_volt < 0) {
    required_analog_volt = 0;
  }

  uint16_t required_register_value = static_cast<uint16_t>(
      UINT16_MAX * (required_analog_volt / BOARD_MAX_VOLT));
  if (required_register_value > this->max_analog_in_volt_bin &&
      this->max_analog_in_volt < BOARD_MAX_VOLT) {
    // This check is a bit complex. If max_analog_in_volt_bin represents the
    // true writable max for the analog voltage, use that. The original code
    // directly calculated required_register_value based on BOARD_MAX_VOLT. If
    // max_analog_in_volt_bin is the cap for register values due to PSU input
    // limits: required_register_value =
    // static_cast<uint16_t>(this->max_analog_in_volt_bin * set_percent_of_max);
    // This needs careful review of how max_analog_in_volt_bin is intended to be
    // used. Sticking to your original calculation logic for now:
  }

  Interface.SetDACA(required_register_value);
  // if (update()) {
  //     this->set_volt_cache = set_val_param; // Cache the requested set value
  //     return true;
  // }
  // return false;
  return update();
}

bool HeinzingerVia16BitDAC::set_current(
    double set_val_param) { // Renamed parameter
  if (set_val_param > this->max_curr || set_val_param < 0) {
    std::cerr << "Set current value lies outside of device's specified range\n";
    return false;
  }

  double set_percent_of_max = set_val_param / 0.98 / this->max_curr;
  double required_analog_volt = this->max_analog_in_volt * set_percent_of_max;
  if (required_analog_volt > this->max_analog_in_volt) {
    required_analog_volt = this->max_analog_in_volt;
  }
  if (required_analog_volt < 0) {
    required_analog_volt = 0;
  }
  uint16_t required_register_value = static_cast<uint16_t>(
      UINT16_MAX * (required_analog_volt / BOARD_MAX_VOLT));

  Interface.SetDACB(required_register_value);
  // if (update()) {
  //     this->set_curr_cache = set_val_param;
  //     return true;
  // }
  // return false;
  return update();
}

double HeinzingerVia16BitDAC::read_voltage() {
  if (!Interface.Readout()) { // Ensure data is fresh
    std::cerr << "Failed to readout interface for voltage reading."
              << std::endl;
    return -1.0; // Or some other error indicator, or throw exception
  }

  uint16_t readout_register_value =
      Interface.ADCB[2]; // Assuming ADCB is populated by Readout()
  // Constants from your original code for conversion:
  const double adc_conversion_factor = 3.2 * 3.3 * 1.12; //equals 11.376
  double readout_analog_volt =
      adc_conversion_factor * readout_register_value / UINT16_MAX;
  // The PSU's analog input for voltage monitoring is 0-10V, representing
  // 0-max_volt
  double converted_output_volt = this->max_volt * readout_analog_volt / 10; //previously divide by 10

  return converted_output_volt;
}

double HeinzingerVia16BitDAC::read_current() {
  if (!Interface.Readout()) { // Ensure data is fresh
    std::cerr << "Failed to readout interface for current reading."
              << std::endl;
    return -1.0; // Or some other error indicator, or throw exception
  }

  uint16_t readout_register_value =
      Interface.ADCB[3]; // Assuming ADCB is populated by Readout()
  const double adc_conversion_factor = 3.2 * 3.3 * 1.12;
  double readout_analog_volt =
      adc_conversion_factor * readout_register_value / UINT16_MAX;
  // The PSU's analog input for current monitoring is 0-10V, representing
  // 0-max_curr
  double converted_output_curr = this->max_curr * readout_analog_volt / 10; //previously divide by 10

  return converted_output_curr;
}

bool HeinzingerVia16BitDAC::set_max_volt() {
  // This sets the DACA to its max value. The resulting voltage depends on
  // how max_analog_in_volt relates to BOARD_MAX_VOLT and the PSU's response.
  // If max_analog_in_volt_bin is the calibrated max register value for desired
  // max_analog_in_volt: Interface.SetDACA(this->max_analog_in_volt_bin); Your
  // original code just used UINT16_MAX which sets the DAC to its max physical
  // output.
  Interface.SetDACA(UINT16_MAX);
  return update();
}

bool HeinzingerVia16BitDAC::set_max_curr() {
  Interface.SetDACB(UINT16_MAX);
  return update();
}

void HeinzingerVia16BitDAC::readADC() {
  if (!Interface.Readout()) {
    std::cerr << "Failed to readout interface for ADC reading." << std::endl;
    return;
  }
  // Interface.ADCB should be populated by Interface.Readout()
  for (const auto &a : Interface.ADCB) { // Added const
    std::cout << a << " ";
  }
  std::cout << std::endl;
}

// The main() function from your original Heinzinger.cpp is guarded here.
// It will not be compiled into the Python module if PYBIND11_MODULE_BUILD is
// defined. You could define PYBIND11_MODULE_BUILD in your CMakeLists.txt for
// the heinzinger_control target: target_compile_definitions(heinzinger_control
// PRIVATE PYBIND11_MODULE_BUILD) Or simply remove the main() function if you no
// longer need to build Heinzinger.cpp as a standalone executable.
#ifndef PYBIND11_MODULE_BUILD
#include <fstream>  // For ofstream in main
#include <unistd.h> // For sleep in main

int main() {
  HeinzingerVia16BitDAC dev(
      30000, 2, true, 10); // Example initialization matching header defaults

  // dev.set_current(2); // Redundant if constructor default is 2mA, but ok

  double set_volt_main, meas_volt, meas_curr;

  // std::ofstream file("./linearity_measurement.csv");
  // if (!file) {
  //  std::cerr << "Could not open file\n";
  //  return 1;
  // }
  // for (int u = 0; u <= 30000; u += 50) {
  //  std::cout << u << "\n";
  //  dev.set_voltage(u);
  //  sleep(5); // Consider std::this_thread::sleep_for for C++11
  //  meas_volt = dev.read_voltage();
  //  file << u << "," << meas_volt << "\n";
  // }
  // file.close();

  while (true) {
    std::cout << "Enter new set voltage: ";
    std::cin >> set_volt_main; // Using std::cin

    dev.set_voltage(set_volt_main);

    for (int i = 0; i < 20; i++) {
      meas_volt = dev.read_voltage();
      meas_curr = dev.read_current();
      std::cout << meas_volt << " V, " << meas_curr << " mA" << std::endl;
      sleep(1); // Consider std::this_thread::sleep_for
    }
  }
  return 0;
}
#endif // PYBIND11_MODULE_BUILD