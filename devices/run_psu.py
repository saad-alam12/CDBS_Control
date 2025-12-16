import sys
import os
import time
import subprocess
import sysconfig
# --- Configuration ---
# Path to the directory where your .so module was built
# This should be your 'PythonWrapper/ADCBoardControlPython/build' directory
MODULE_BUILD_DIR = os.path.join(os.path.dirname(__file__), 'build')

# The expected name of your compiled module.
# CMake produces this based on the module name in PYBIND11_MODULE and Python version/platform.
# Your output showed: heinzinger_control.cpython-313-darwin.so
MODULE_FILENAME = (
     'heinzinger_control' + sysconfig.get_config_var('EXT_SUFFIX')
)
PYTHON_MODULE_NAME = 'heinzinger_control' # Name used in "import heinzinger_control"
CPP_CLASS_NAME_IN_PYTHON = 'HeinzingerPSU' # Name given in py::class_<...>(m, "HeinzingerPSU")

# --- Global variables for PSU instances ---
_psu_instances = {}  # Dictionary to store multiple PSU instances
_module_loaded = False

def setup_module_path_and_load():
    """Adds the build directory to Python's path and tries to load the module."""
    global _module_loaded

    if not os.path.isdir(MODULE_BUILD_DIR):
        print(f"ERROR: Build directory not found at {MODULE_BUILD_DIR}")
        _module_loaded = False
        return

    sys.path.insert(0, MODULE_BUILD_DIR)
    print(f"Added '{MODULE_BUILD_DIR}' to sys.path.")

    so_file_path = os.path.join(MODULE_BUILD_DIR, MODULE_FILENAME)
    if not os.path.exists(so_file_path):
        print(f"ERROR: Module file not found at {so_file_path}")
        print("Please ensure you've built the module correctly and it's in the build directory.")
        _module_loaded = False
        return
    else:
        print(f"Module file found at {so_file_path}")

    # Optional: Check shared library dependencies using otool -L (macOS specific)
    try:
        if sys.platform == 'darwin': # darwin is macOS
            result = subprocess.run(['otool', '-L', so_file_path], capture_output=True, text=True, check=True)
            print(f"\nShared library dependencies for {MODULE_FILENAME}:")
            print(result.stdout)
            if 'libusb-1.0.dylib' not in result.stdout:
                print("WARNING: libusb-1.0.dylib does not appear in otool -L output. Make sure it's correctly linked or available in rpath/lib path.")
            else:
                print("libusb-1.0.dylib seems to be linked.")
        elif sys.platform.startswith('linux'):
            # On Linux, you could use 'ldd <so_file_path>'
            print(f"On Linux, you can check dependencies with: ldd {so_file_path}")

    except FileNotFoundError:
        print(f"WARNING: 'otool' (or 'ldd') not found. Cannot check shared library dependencies.")
    except subprocess.CalledProcessError as e:
        print(f"WARNING: 'otool -L' (or 'ldd') failed for {so_file_path}: {e}")
        print("This might indicate linking issues or problems with the .so file itself.")
    except Exception as e:
        print(f"WARNING: Could not check shared library dependencies: {e}")

    # Try to import the module
    try:
        # The import name is just the module name, without .so or python tags
        module = __import__(PYTHON_MODULE_NAME)
        globals()[PYTHON_MODULE_NAME] = module # Make it available like a normal import
        print(f"\nSuccessfully imported '{PYTHON_MODULE_NAME}' module.")
        _module_loaded = True
    except ImportError as e:
        print(f"ERROR: Failed to import '{PYTHON_MODULE_NAME}' module.")
        print(f"Ensure '{MODULE_BUILD_DIR}' is in sys.path and contains '{MODULE_FILENAME}'.")
        print(f"Ensure all dependencies like libusb-1.0.dylib are installed and accessible.")
        print(f"  (On macOS, try 'brew install libusb' and ensure it's linked).")
        print(f"Import error details: {e}")
        _module_loaded = False
    except Exception as e:
        print(f"An unexpected error occurred during import: {e}")
        _module_loaded = False

def initialize_psu(device_index=0, max_v=30000.0, max_c=25, verb=False, max_in_v=10.0):
    """Initializes connection to the PSU."""
    global _psu_instance
    if not _module_loaded:
        print("ERROR: Python module not loaded. Cannot initialize PSU.")
        return False
    if _psu_instance is not None:
        print("PSU already initialized.")
        return True
    try:
        # Access the module and class correctly
        psu_module = globals().get(PYTHON_MODULE_NAME)
        if not psu_module:
            print(f"ERROR: Module '{PYTHON_MODULE_NAME}' not found in globals after import.")
            return False
            
        PSUClass = getattr(psu_module, CPP_CLASS_NAME_IN_PYTHON)
        _psu_instance = PSUClass(device_index=device_index, max_voltage=max_v, max_current=max_c, verbose=verb, max_input_voltage=max_in_v)
        print("PSU C++ object instance created successfully.")
        # The C++ constructor already tries to open the device.
        # A short delay might be good practice after initialization if the device needs it.
        time.sleep(0.1) # Small delay
        
        # You could add a check here if your C++ class had an `is_open()` or similar method
        # if not _psu_instance.is_connected(): # Fictional method
        #     print("ERROR: PSU object created, but device connection failed (check C++ logs or is_connected status).")
        #     _psu_instance = None
        #     return False
        return True
    except AttributeError as e:
        print(f"ERROR: Class '{CPP_CLASS_NAME_IN_PYTHON}' not found in module '{PYTHON_MODULE_NAME}'.")
        print(f"Pybind11 binding error or mismatch? Details: {e}")
        _psu_instance = None
        return False
    except Exception as e:
        print(f"ERROR during PSU initialization: {e}")
        _psu_instance = None
        return False

def set_psu_voltage(voltage):
    """Sets PSU voltage. Returns True on success, False on error from C++."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        return False
    try:
        print(f"Python: Calling C++ set_voltage({voltage})")
        success = _psu_instance.set_voltage(float(voltage)) # Ensure float
        print(f"Python: C++ set_voltage returned: {success}")
        return success
    except Exception as e:
        print(f"ERROR setting voltage: {e}")
        return False

def read_psu_voltage():
    """Reads PSU voltage. Returns voltage value, or raises an exception on error."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        raise RuntimeError("PSU not initialized")
    try:
        print(f"Python: Calling C++ read_voltage()")
        voltage = _psu_instance.read_voltage()
        print(f"Python: C++ read_voltage returned: {voltage}")
        return voltage
    except Exception as e:
        print(f"ERROR reading voltage: {e}")
        raise # Re-raise the exception to be handled by the caller

def set_psu_current(current):
    """Sets PSU current limit. Returns True on success, False on error."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        return False
    try:
        print(f"Python: Calling C++ set_current({current})")
        success = _psu_instance.set_current(float(current)) # Ensure float
        print(f"Python: C++ set_current returned: {success}")
        return success
    except Exception as e:
        print(f"ERROR setting current: {e}")
        return False

def read_psu_current():
    """Reads PSU current. Returns current value, or raises an exception on error."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        raise RuntimeError("PSU not initialized")
    try:
        print(f"Python: Calling C++ read_current()")
        current = _psu_instance.read_current()
        print(f"Python: C++ read_current returned: {current}")
        return current
    except Exception as e:
        print(f"ERROR reading current: {e}")
        raise # Re-raise

def switch_psu_on():
    """Turns the PSU output ON. Returns True on success, False on error."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        return False
    try:
        print(f"Python: Calling C++ switch_on()")
        success = _psu_instance.switch_on()
        print(f"Python: C++ switch_on returned: {success}")
        return success
    except Exception as e:
        print(f"ERROR turning PSU on: {e}")
        return False

def switch_psu_off():
    """Turns the PSU output OFF. Returns True on success, False on error."""
    if _psu_instance is None:
        print("ERROR: PSU not initialized. Call initialize_psu() first.")
        return False
    try:
        print(f"Python: Calling C++ switch_off()")
        success = _psu_instance.switch_off()
        print(f"Python: C++ switch_off returned: {success}")
        return success
    except Exception as e:
        print(f"ERROR turning PSU off: {e}")
        return False

def cleanup_psu():
    """Cleans up PSU resources by deleting the Python object instance."""
    global _psu_instance
    if _psu_instance is not None:
        print("Cleaning up PSU instance (Python object deletion will trigger C++ destructor).")
        # The C++ destructor of HeinzingerVia16BitDAC (via FGAnalogPSUInterface -> FGUSBBulk)
        # should handle closing the USB device and releasing the interface.
        _psu_instance = None # Allow Python's garbage collector to reclaim it
    else:
        print("PSU instance already None or not initialized.")
    return True

def get_psu_instance(device_index=0, verb=False):
    """
    Ensure the PSU is ready and return the singleton instance.
    """
    if _psu_instance is None:
        setup_module_path_and_load()
        initialize_psu(device_index=device_index, verb=verb)
    return _psu_instance


# Main execution block
if __name__ == '__main__':
    setup_module_path_and_load()

    if not _module_loaded:
        print("\nExiting script as module could not be loaded.")
        sys.exit(1)

    print("\nAttempting to initialize PSU...")
    if _module_loaded:
        # Set C++ global Verbosity using the new setter function
        #heinzinger_control.set_cpp_verbosity_level(2) # Example: Set to 2 for detailed USB logs
        # Read it back to confirm (optional)
        #current_cpp_verbosity = heinzinger_control.get_cpp_verbosity_level()
        #print(f"Set C++ global Verbosity to: {current_cpp_verbosity}")
        print("\nAttempting to initialize PSU...")
    # Pass verbose=True to see AnalogPSU.h logs (this sets FGAnalogPSUInterface::Verbose member)
    if initialize_psu(device_index=0,verb=True): # globally turn off C++ Logs
        print("PSU initialized successfully from Python.")
        try:
            print("\n--- PSU Control Interface ---")
            if switch_psu_on(): # Attempt to switch on first
                 print("PSU output turned ON successfully via Python call.")
            else:
                print("WARNING: Failed to turn PSU ON via Python call. Check C++ logs. Continuing...")

            # Read initial state
            try:
                v_read = read_psu_voltage()
                c_read = read_psu_current()
                print(f"Initial Read: Voltage = {v_read:.2f} V, Current = {c_read:.4f} mA") # Assuming current is in mA from C++
            except Exception as e_read:
                print(f"Error reading initial state: {e_read}")


            while True:
                print("\nCommands: setv <val>, setc <val>, on, off, read, quit")
                user_input = input("Enter command: ").strip().lower()

                if user_input == 'quit':
                    print("Exiting control loop...")
                    break
                elif user_input.startswith("setv "):
                    try:
                        voltage = float(user_input.split()[1])
                        if set_psu_voltage(voltage):
                            print(f"Voltage set command sent for {voltage:.2f} V.")
                        else:
                            print(f"Failed to set voltage to {voltage:.2f} V.")
                    except (IndexError, ValueError):
                        print("Invalid format. Use: setv <number>")
                elif user_input.startswith("setc "):
                    try:
                        current = float(user_input.split()[1])
                        if set_psu_current(current):
                            print(f"Current limit set command sent for {current:.3f} mA.")
                        else:
                            print(f"Failed to set current limit to {current:.3f} mA.")
                    except (IndexError, ValueError):
                        print("Invalid format. Use: setc <number>")
                elif user_input == 'on':
                    if switch_psu_on():
                        print("PSU ON command successful.")
                    else:
                        print("Failed to turn PSU ON.")
                elif user_input == 'off':
                    if switch_psu_off():
                        print("PSU OFF command successful.")
                    else:
                        print("Failed to turn PSU OFF.")
                elif user_input == 'read':
                    try:
                        v_read = read_psu_voltage()
                        c_read = read_psu_current()
                        print(f"Read: Voltage = {v_read:.2f} V, Current = {c_read:.4f} mA")
                    except Exception as e_read:
                        print(f"Error during read: {e_read}")
                else:
                    print("Unknown command.")

                time.sleep(0.2) # Small delay between commands

        except KeyboardInterrupt:
            print("\nOperation interrupted by user.")
        except Exception as e_main:
            print(f"\nAn error occurred in the main loop: {e_main}")
        finally:
            print("\nShutting down PSU and cleaning up...")
            switch_psu_off() # Ensure PSU is off
            cleanup_psu()
            print("PSU control finished.")
    else:
        print("Failed to initialize PSU. Please check error messages above.")
        print("Make sure the physical PSU and USB interface board are connected and powered correctly.")
        print("If you saw 'Access denied (insufficient permissions)' from libusb in C++ logs, you might need to adjust udev rules (Linux) or run with more privileges (not recommended for general use).")