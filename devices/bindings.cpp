#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // For automatic C++/Python STL conversions if needed elsewhere

#include "headers/Error.h" // Include Error.h again for direct access to 'extern int Verbosity'
#include "headers/Heinzinger.h" // This includes AnalogPSU.h -> FGUSBBulk.h -> Error.h (for Verbosity decl)

namespace py = pybind11;

// --- Getter and Setter for global C++ Verbosity ---
// These functions will be called from Python.
// 'Verbosity' is declared as 'extern int Verbosity;' in Error.h
// and defined in ProjectGlobals.cpp.
int get_cpp_global_verbosity() { return Verbosity; }

void set_cpp_global_verbosity(int v) { Verbosity = v; }

PYBIND11_MODULE(heinzinger_control, m) {
  m.doc() = "Python bindings for Heinzinger Power Supply Control";

  py::class_<HeinzingerVia16BitDAC>(m, "HeinzingerPSU")
      // New USB path-based constructor (preferred)
      .def(py::init<const std::string&, double, double, bool, double>(),
           py::arg("usb_path"),
           py::arg("max_voltage") = 30000.0,
           py::arg("max_current") = 2.0, 
           py::arg("verbose") = false,
           py::arg("max_input_voltage") = 10.0,
           "Initialize PSU using USB path identification (recommended)")
      // Legacy device_index constructor (for backward compatibility)
      .def(py::init<int, double, double, bool, double>(),
           py::arg("device_index") = 0,
           py::arg("max_voltage") = 50000.0,
           py::arg("max_current") = 0.0005, // 0.5 mA
           py::arg("verbose") = false,
           py::arg("max_input_voltage") = 10.0,
           "Initialize PSU using device index (deprecated - use USB path instead)")
      .def("switch_on", &HeinzingerVia16BitDAC::switch_on,
           "Switches the PSU relay on.")
      .def("switch_off", &HeinzingerVia16BitDAC::switch_off,
           "Switches the PSU relay off.")
      .def("set_voltage", &HeinzingerVia16BitDAC::set_voltage,
           py::arg("set_val"), "Sets the output voltage.")
      .def("set_current", &HeinzingerVia16BitDAC::set_current,
           py::arg("set_val"), "Sets the output current limit.")
      .def("read_voltage", &HeinzingerVia16BitDAC::read_voltage,
           "Reads the measured output voltage.")
      .def("read_current", &HeinzingerVia16BitDAC::read_current,
           "Reads the measured output current.")
      .def("set_max_volt", &HeinzingerVia16BitDAC::set_max_volt,
           "Sets the voltage to its maximum configured value.")
      .def("set_max_curr", &HeinzingerVia16BitDAC::set_max_curr,
           "Sets the current limit to its maximum configured value.")
      .def("is_relay_on", &HeinzingerVia16BitDAC::is_relay_on,
         "Return True if the PSU output relay is closed (output ON).")
      .def("readADC", &HeinzingerVia16BitDAC::readADC,
           "Reads and prints raw ADC values (for debugging).");

  // Expose the global C++ Verbosity variable to Python using getter and setter
  // functions
  m.def("get_cpp_verbosity_level", &get_cpp_global_verbosity,
        "Gets the C++ global Verbosity level.");
  m.def("set_cpp_verbosity_level", &set_cpp_global_verbosity, py::arg("level"),
        "Sets the C++ global Verbosity level.");
}