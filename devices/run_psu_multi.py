import sys
import os
import time
import subprocess
import sysconfig

# --- Configuration ---
# Path to the directory where your .so module was built
MODULE_BUILD_DIR = os.path.join(os.path.dirname(__file__), 'build')

# The expected name of your compiled module.
MODULE_FILENAME = (
     'heinzinger_control' + sysconfig.get_config_var('EXT_SUFFIX')
)
PYTHON_MODULE_NAME = 'heinzinger_control' 
CPP_CLASS_NAME_IN_PYTHON = 'HeinzingerPSU'

# --- USB Path Configuration ---
# USB paths for reliable device identification (keep PSUs in same physical ports!)
USB_PATH_HEINZINGER = "@00110000"  # Heinzinger PSU (30kV, no relay)
USB_PATH_FUG = "@00120000"         # FUG PSU (50kV, with relay)

# --- Global variables for PSU instances ---
_psu_instances = {}  # Dictionary to store multiple PSU instances by device_index
_module_loaded = False

def setup_module_path_and_load():
    """
    Sets up and loads the special code needed to control PSU hardware.
    
    This program controls PSUs (Power Supply Units) using code written in C++
    for better performance. Before we can use that C++ code from Python, we need
    to load it like loading a library book from a specific shelf.
    
    This function does three main things:
    1. Tells Python where to find the C++ code file (in the 'build' folder)
    2. Checks that the code file actually exists there
    3. Loads the C++ code so Python can use it
    
    Returns:
        bool: True if everything loaded successfully, False if something went wrong
    
    What This Function Does:
        - Adds the 'build' folder to Python's search path
        - Looks for the compiled C++ module file (ends in .so on Linux/Mac)
        - Imports the C++ code so other functions can use it
        - Prints helpful messages about what's happening
        - Sets a flag so we know the module is ready to use
    
    Common Problems:
        - "Build directory not found" = The C++ code hasn't been compiled yet
        - "Module file not found" = Compilation failed or file was moved
        - "Failed to import" = The compiled code has missing dependencies
    
    Notes:
        - Safe to call multiple times (won't reload if already loaded)
        - Must be called before using any PSU control functions
        - The C++ code file is created by compiling with cmake and make
    """
    global _module_loaded

    if _module_loaded:
        return True

    if not os.path.isdir(MODULE_BUILD_DIR):
        print(f"ERROR: Build directory not found at {MODULE_BUILD_DIR}")
        _module_loaded = False
        return False

    sys.path.insert(0, MODULE_BUILD_DIR)
    print(f"Added '{MODULE_BUILD_DIR}' to sys.path.")

    so_file_path = os.path.join(MODULE_BUILD_DIR, MODULE_FILENAME)
    if not os.path.exists(so_file_path):
        print(f"ERROR: Module file not found at {so_file_path}")
        print("Please ensure you've built the module correctly and it's in the build directory.")
        _module_loaded = False
        return False
    else:
        print(f"Module file found at {so_file_path}")

    # Try to import the module
    try:
        module = __import__(PYTHON_MODULE_NAME)
        globals()[PYTHON_MODULE_NAME] = module 
        print(f"Successfully imported '{PYTHON_MODULE_NAME}' module.")
        _module_loaded = True
        return True 
    except ImportError as e:
        print(f"ERROR: Failed to import '{PYTHON_MODULE_NAME}' module.")
        print(f"Import error details: {e}")
        _module_loaded = False
        return False
    except Exception as e:
        print(f"An unexpected error occurred during import: {e}")
        _module_loaded = False
        return False

def initialize_psu_by_path(usb_path, max_v=30000.0, max_c=25, verb=False, max_in_v=10.0):
    """
    Initializes connection to a Power Supply Unit (PSU) using USB path identification.
    
    This function creates a new PSU instance using USB path-based identification
    instead of device enumeration order, providing more reliable device selection.
    
    Args:
        usb_path (str): USB path identifier (e.g., "@00110000" for Heinzinger, "@00120000" for FUG)
        max_v (float): Maximum voltage limit in volts (default: 30000.0 = 30kV)
        max_c (float): Maximum current limit in milliamps (default: 25mA)  
        verb (bool): Enable verbose logging for debugging (default: False)
        max_in_v (float): Maximum input voltage in volts (default: 10.0V)
    
    Returns:
        bool: True if PSU initialization successful, False otherwise
    """
    global _psu_instances
    
    if not _module_loaded:
        print("ERROR: Python module not loaded. Cannot initialize PSU.")
        return False
    
    # Check if PSU for this USB path is already initialized
    if usb_path in _psu_instances:
        print(f"PSU at USB path {usb_path} already initialized.")
        return True
    
    try:
        # Get the C++ class from the imported module
        cpp_module = globals()[PYTHON_MODULE_NAME]
        cpp_class = getattr(cpp_module, CPP_CLASS_NAME_IN_PYTHON)
        
        # Create PSU instance using USB path
        psu_instance = cpp_class(usb_path, max_v, max_c, verb, max_in_v)
        
        # Store the instance
        _psu_instances[usb_path] = psu_instance
        print(f"PSU at USB path {usb_path} initialized successfully.")
        time.sleep(0.1)  # Small delay after initialization
        return True
        
    except AttributeError as e:
        print(f"ERROR: C++ class '{CPP_CLASS_NAME_IN_PYTHON}' not found in module '{PYTHON_MODULE_NAME}': {e}")
        return False
    except Exception as e:
        print(f"ERROR: Failed to initialize PSU at USB path {usb_path}: {e}")
        return False

def initialize_psu(device_index=0, max_v=30000.0, max_c=25, verb=False, max_in_v=10.0):
    """
    Initializes connection to a Power Supply Unit (PSU) with specific device index.
    
    This function creates a new PSU instance using the C++ class bound via pybind11,
    configures it with the specified parameters, and stores it in the global instance
    dictionary for later use. Each PSU is identified by its device_index to support
    multiple concurrent PSU operations.
    
    Args:
        device_index (int): Unique identifier for the PSU device (default: 0)
                           Used to differentiate between multiple PSUs
        max_v (float): Maximum voltage limit in millivolts (default: 30000.0 = 30V)
                      Safety limit to prevent exceeding PSU voltage capabilities
        max_c (int): Maximum current limit in amperes (default: 25A)
                    Safety limit to prevent exceeding PSU current capabilities
        verb (bool): Enable verbose logging for debugging (default: False)
                    When True, provides detailed operation logs
        max_in_v (float): Maximum input voltage in volts (default: 10.0V)
                         Input voltage safety threshold
    
    Returns:
        bool: True if PSU initialization successful, False otherwise
    
    Side Effects:
        - Stores PSU instance in global _psu_instances dictionary
        - Prints status messages to stdout
        - Adds 100ms delay after successful initialization
    
    Raises:
        AttributeError: If C++ class binding is not found
        Exception: For any other initialization errors
    
    Notes:
        - Prevents duplicate initialization of same device_index
        - Requires _module_loaded to be True (call setup_module_path_and_load() first)
        - Uses dynamic class loading via globals() and getattr()
        - Compatible with pybind11 C++ bindings
    """
    global _psu_instances
    
    if not _module_loaded:
        print("ERROR: Python module not loaded. Cannot initialize PSU.")
        return False
    
    # Check if PSU for this device index is already initialized
    if device_index in _psu_instances:
        print(f"PSU on device {device_index} already initialized.")
        return True
    
    try:
        # Access the module and class correctly
        psu_module = globals().get(PYTHON_MODULE_NAME)
        if not psu_module:
            print(f"ERROR: Module '{PYTHON_MODULE_NAME}' not found in globals after import.")
            return False
            
        PSUClass = getattr(psu_module, CPP_CLASS_NAME_IN_PYTHON)
        psu_instance = PSUClass(
            device_index=device_index, 
            max_voltage=max_v, 
            max_current=max_c, 
            verbose=verb, 
            max_input_voltage=max_in_v
        )
        
        # Store the instance
        _psu_instances[device_index] = psu_instance
        print(f"PSU C++ object instance created successfully for device {device_index}.")
        
        # Small delay after initialization
        time.sleep(0.1)
        return True
        
    except AttributeError as e:
        print(f"ERROR: Class '{CPP_CLASS_NAME_IN_PYTHON}' not found in module '{PYTHON_MODULE_NAME}'.")
        print(f"Pybind11 binding error or mismatch? Details: {e}")
        return False
    except Exception as e:
        print(f"ERROR during PSU initialization for device {device_index}: {e}")
        return False

def get_psu_instance(device_index=0, max_v=30000.0, max_c=25, verb=False, max_in_v=10.0):
    """
    Gets you a "handle" to control a specific PSU, setting it up if needed.
    
    Think of this like getting a remote control for a specific TV. If the remote
    is already set up for that TV, it just gives you the remote. If not, it sets
    up the remote first, then gives it to you.
    
    This is the easiest way to start controlling a PSU - it handles all the
    setup work automatically.
    
    Args:
        device_index (int): Which PSU you want (0 for first PSU, 1 for second, etc.)
        max_v (float): Safety limit for voltage in millivolts (30000.0 = 30 volts max)
        max_c (int): Safety limit for current in amperes (25 = 25 amps max)
        verb (bool): Show detailed messages for troubleshooting (default: False)
        max_in_v (float): Input voltage safety limit in volts (default: 10.0V)
    
    Returns:
        object: A "PSU controller" object you can use to control the PSU
                Returns None if something went wrong during setup
    
    Example:
        # Get controller for the first PSU with safe 5V, 2A limits
        psu = get_psu_instance(0, max_v=5000, max_c=2)
        if psu:
            print("PSU controller ready!")
        else:
            print("Failed to set up PSU controller")
    
    What Happens Automatically:
        - Loads the C++ control code if not already loaded
        - Sets up the PSU with your safety limits if not already done
        - Gives you back the same controller if you call this again
    
    Notes:
        - Always check if the returned value is None before using it
        - Safe to call multiple times with the same device_index
        - Each PSU (device_index) gets its own separate controller
    """
    if device_index not in _psu_instances:
        setup_module_path_and_load()
        if not initialize_psu(device_index=device_index, max_v=max_v, max_c=max_c, verb=verb, max_in_v=max_in_v):
            return None
    
    return _psu_instances.get(device_index)

def get_psu_instance_by_path(usb_path, max_v=30000.0, max_c=25, verb=False, max_in_v=10.0):
    """
    Gets a PSU controller using reliable USB path identification.
    
    This is the new, improved way to get PSU controllers that doesn't depend on
    which device the computer finds first. Each PSU is identified by its specific
    USB port location, making it much more reliable than device enumeration order.
    
    Args:
        usb_path (str): USB path identifier - use USB_PATH_HEINZINGER or USB_PATH_FUG constants
        max_v (float): Safety limit for voltage in volts (30000.0 = 30kV max)
        max_c (float): Safety limit for current in milliamps (25 = 25mA max)  
        verb (bool): Show detailed messages for troubleshooting (default: False)
        max_in_v (float): Input voltage safety limit in volts (default: 10.0V)
    
    Returns:
        object: A PSU controller object, or None if setup failed
        
    Example:
        # Get Heinzinger PSU controller  
        heinz = get_psu_instance_by_path(USB_PATH_HEINZINGER, max_v=30000, max_c=2.0)
        
        # Get FUG PSU controller
        fug = get_psu_instance_by_path(USB_PATH_FUG, max_v=50000, max_c=0.5)
    """
    global _psu_instances
    
    # Try to initialize PSU if not already done
    if not initialize_psu_by_path(usb_path, max_v, max_c, verb, max_in_v):
        return None
    
    return _psu_instances.get(usb_path)

def set_psu_voltage(device_index, voltage):
    """
    Sets how much electrical "pressure" (voltage) a PSU should output.
    
    Voltage is like water pressure in a pipe - higher voltage means more electrical
    "pressure" that can push current through circuits. This function tells the PSU
    "please output exactly this much voltage."
    
    The change happens immediately - as soon as you call this function, the PSU
    starts outputting the new voltage level.
    
    Args:
        device_index (int): Which PSU to control (0 for first PSU, 1 for second, etc.)
        voltage (float): The voltage you want in millivolts
                        (e.g., 3300 = 3.3 volts, 5000 = 5.0 volts)
    
    Returns:
        bool: True if the voltage was set successfully, False if something went wrong
    
    Side Effects:
        - PSU immediately changes its output voltage
        - Connected devices will see the new voltage right away
        - Prints status messages to help with troubleshooting
    
    Safety Warning:
        - Make sure the voltage is safe for connected devices!
        - Too much voltage can damage or destroy connected circuits
        - Always double-check your voltage calculations
    
    Example:
        # Set PSU #0 to output 3.3 volts (3300 millivolts)
        success = set_psu_voltage(0, 3300)
        if success:
            print("Voltage set to 3.3V")
        else:
            print("Failed to set voltage - check PSU connection")
    
    Common Voltage Values:
        - 3300 = 3.3V (common for microcontrollers)
        - 5000 = 5.0V (common for logic circuits)
        - 12000 = 12V (common for motors, fans)
    
    Notes:
        - PSU must be set up first with initialize_psu() or get_psu_instance()
        - Voltage must be within the safety limits set during PSU initialization
        - Use millivolts to avoid decimal point errors
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        return False
    try:
        print(f"Python: Calling C++ set_voltage({voltage}) for device {device_index}")
        success = psu_instance.set_voltage(float(voltage))
        print(f"Python: C++ set_voltage returned: {success}")
        return success
    except Exception as e:
        print(f"ERROR setting voltage for device {device_index}: {e}")
        return False

def read_psu_voltage(device_index):
    """
    Asks a PSU "what voltage are you actually outputting right now?"
    
    This function measures the real voltage coming out of the PSU at this moment.
    It's like using a multimeter to check the actual voltage - you might have
    asked for 5.0V, but the PSU might be outputting 4.98V due to load or accuracy.
    
    This is useful for:
    - Verifying the PSU is working correctly
    - Checking if voltage drops under load
    - Monitoring power supply stability
    - Troubleshooting circuit problems
    
    Args:
        device_index (int): Which PSU to read from (0 for first PSU, 1 for second, etc.)
    
    Returns:
        float: The actual voltage being output right now (in millivolts)
               (e.g., 3287.5 means 3.2875 volts actual output)
    
    Example:
        set_psu_voltage(0, 5000)  # Ask for 5.0V
        actual = read_psu_voltage(0)  # See what we actually get
        print(f"Requested 5.0V, actually getting {actual/1000:.3f}V")
        
        # Check if voltage is close enough to what we wanted
        if abs(actual - 5000) < 100:  # Within 0.1V
            print("Voltage is accurate")
        else:
            print("Voltage is off - check connections or PSU")
    
    Possible Problems:
        - RuntimeError: PSU not set up yet (call initialize_psu first)
        - Communication errors: PSU disconnected or hardware problem
        - May return 0 if PSU is turned off
    
    Notes:
        - PSU must be set up first with initialize_psu() or get_psu_instance()
        - This is a "read-only" operation - doesn't change any settings
        - Takes a moment to communicate with PSU hardware
        - Value might fluctuate slightly due to electrical noise
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        raise RuntimeError(f"PSU on device {device_index} not initialized")
    try:
        voltage = psu_instance.read_voltage()
        return voltage
    except Exception as e:
        print(f"ERROR reading voltage for device {device_index}: {e}")
        raise

def set_psu_current(device_index, current):
    """
    Sets the maximum current limit for a specific PSU device.
    
    A PSU (Power Supply Unit) can supply both voltage and current. This function
    sets the maximum amount of electrical current (measured in amperes/amps) that
    the PSU is allowed to output. This acts as a safety limit - if the connected
    load tries to draw more current than this limit, the PSU will reduce voltage
    to keep current at or below this limit.
    
    Think of it like a water faucet with a flow limiter - you're setting the
    maximum flow rate, not the actual flow (which depends on what's connected).
    
    Args:
        device_index (int): Which PSU you want to control (like PSU #0, PSU #1, etc.)
        current (float): Maximum current in amperes (e.g., 2.5 means 2.5 amps max)
                        Automatically converted to ensure compatibility
    
    Returns:
        bool: True if the current limit was set successfully, False if something went wrong
    
    Side Effects:
        - Changes the PSU's internal current limit setting immediately
        - May cause voltage to drop if connected load tries to draw too much current
        - Prints status messages to help with debugging
    
    Example:
        set_psu_current(0, 2.0)  # Set PSU #0 to maximum 2 amps
        set_psu_current(1, 0.5)  # Set PSU #1 to maximum 0.5 amps
    
    Notes:
        - The PSU must be initialized first using initialize_psu()
        - Current should be within the PSU's capabilities (set during initialization)
        - This is a safety feature - actual current depends on what you connect to the PSU
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        return False
    try:
        success = psu_instance.set_current(float(current))
        return success
    except Exception as e:
        print(f"ERROR setting current for device {device_index}: {e}")
        return False

def read_psu_current(device_index):
    """
    Reads how much electrical current a specific PSU is currently supplying.
    
    This function asks the PSU "how much current are you supplying right now?"
    and returns the answer. Current is measured in amperes (amps) and represents
    the actual electrical current flowing from the PSU to whatever is connected to it.
    
    Think of it like checking the speedometer in a car - it tells you the current
    speed, not the speed limit. Similarly, this tells you the actual current flow,
    not the maximum current limit.
    
    Args:
        device_index (int): Which PSU you want to read from (like PSU #0, PSU #1, etc.)
    
    Returns:
        float: Current electrical current in amperes (e.g., 1.5 means 1.5 amps flowing)
    
    Raises:
        RuntimeError: If the PSU hasn't been set up yet (need to call initialize_psu first)
        Exception: If there's a communication problem with the PSU hardware
    
    Example:
        current = read_psu_current(0)  # Read current from PSU #0
        print(f"PSU is supplying {current} amps")
    
    Notes:
        - The PSU must be initialized first using initialize_psu()
        - Reading current doesn't change any PSU settings
        - The value shows actual current flow, which depends on what's connected
        - May be 0 if PSU is off or nothing is connected that draws current
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        raise RuntimeError(f"PSU on device {device_index} not initialized")
    try:
        current = psu_instance.read_current()
        return current
    except Exception as e:
        print(f"ERROR reading current for device {device_index}: {e}")
        raise

def switch_psu_on(device_index):
    """
    Turns ON the electrical output of a specific PSU device.
    
    A PSU (Power Supply Unit) is like an electrical switch that can be turned on or off.
    When OFF, it produces no electricity even if voltage/current are set. When ON,
    it actively supplies electricity at the configured voltage and current levels.
    
    This function is like flipping a light switch to the "ON" position - it enables
    the PSU to start supplying power to whatever is connected to its output terminals.
    
    Args:
        device_index (int): Which PSU you want to turn on (like PSU #0, PSU #1, etc.)
    
    Returns:
        bool: True if the PSU was turned on successfully, False if something went wrong
    
    Side Effects:
        - PSU begins supplying electricity at its configured voltage/current settings
        - Connected devices/circuits will receive power and may start operating
        - PSU status indicators may change (LEDs, displays, etc.)
    
    Safety Warning:
        - Make sure all connections are secure before turning on the PSU
        - Verify voltage and current settings are appropriate for connected devices
        - Be aware that connected devices will immediately receive power
    
    Example:
        # First set up the PSU with safe settings
        initialize_psu(0, max_v=5000, max_c=1.0)  # 5V, 1A max
        set_psu_voltage(0, 3300)  # Set to 3.3V
        set_psu_current(0, 0.5)   # Limit to 0.5A
        switch_psu_on(0)          # Now turn it on - power flows!
    
    Notes:
        - PSU must be initialized first using initialize_psu()
        - Configure voltage and current BEFORE turning on for safety
        - Use switch_psu_off() to turn off the power output
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        return False
    try:
        success = psu_instance.switch_on()
        return success
    except Exception as e:
        print(f"ERROR turning PSU on for device {device_index}: {e}")
        return False

def switch_psu_off(device_index):
    """
    Turns OFF the electrical output of a specific PSU device.
    
    This function stops the PSU from supplying electricity, like turning off a light
    switch. Even though the PSU is still connected and configured, no power flows
    to connected devices until you turn it back on with switch_psu_on().
    
    Think of it as the "emergency stop" or "safe shutdown" for the power supply.
    The PSU remembers its voltage and current settings, but stops outputting power.
    
    Args:
        device_index (int): Which PSU you want to turn off (like PSU #0, PSU #1, etc.)
    
    Returns:
        bool: True if the PSU was turned off successfully, False if something went wrong
    
    Side Effects:
        - PSU stops supplying electricity immediately
        - Connected devices/circuits lose power and may shut down
        - PSU status indicators may change (LEDs, displays, etc.)
        - Voltage and current settings are preserved for next time
    
    Safety Benefits:
        - Safe way to cut power without unplugging cables
        - Protects connected devices during connection changes
        - Allows safe adjustment of settings while power is off
    
    Example:
        switch_psu_off(0)  # Turn off PSU #0 - power stops flowing
        # Now safe to change connections or settings
        set_psu_voltage(0, 5000)  # Change voltage while safely off
        switch_psu_on(0)   # Turn back on with new settings
    
    Notes:
        - PSU must be initialized first using initialize_psu()
        - This is the safe way to stop power output
        - PSU settings (voltage, current limits) are not changed, just output disabled
        - Always turn off before making connection changes
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        return False
    try:
        success = psu_instance.switch_off()
        return success
    except Exception as e:
        print(f"ERROR turning PSU off for device {device_index}: {e}")
        return False

def is_relay_on(device_index):
    """
    Checks whether a specific PSU's internal relay/switch is currently ON or OFF.
    
    A relay is like an internal electrical switch inside the PSU. When the relay is ON,
    the PSU can supply power to its output terminals. When the relay is OFF, no power
    flows even if the PSU is configured and "switched on" - it's like having a second
    safety switch inside the device.
    
    Think of it like checking whether a circuit breaker is in the ON or OFF position.
    This function asks the PSU "is your internal power switch currently on?"
    
    Args:
        device_index (int): Which PSU you want to check (like PSU #0, PSU #1, etc.)
    
    Returns:
        bool: True if the internal relay is ON (power can flow)
              False if the internal relay is OFF (no power output) or if there's an error
    
    Example:
        if is_relay_on(0):
            print("PSU #0 relay is ON - power can flow")
        else:
            print("PSU #0 relay is OFF - no power output")
    
    Common Use Cases:
        - Safety check before connecting sensitive equipment
        - Troubleshooting why a PSU isn't supplying power
        - Verifying PSU state before making measurements
        - Confirming proper shutdown after switch_psu_off()
    
    Notes:
        - PSU must be initialized first using initialize_psu()
        - This is a read-only check - it doesn't change the relay state
        - Returns False if PSU communication fails (safer assumption)
        - Relay state may be different from the main on/off switch state
    """
    psu_instance = _psu_instances.get(device_index)
    if psu_instance is None:
        print(f"ERROR: PSU on device {device_index} not initialized.")
        return False
    try:
        return psu_instance.is_relay_on()
    except Exception as e:
        print(f"ERROR checking relay state for device {device_index}: {e}")
        return False

def cleanup_psu(device_index=None):
    """
    Properly shuts down and releases PSU resources to prevent memory leaks.
    
    When you're done using PSU devices, it's important to "clean up" - this means
    telling the computer to release the memory and resources that were being used
    to control the PSUs. It's like closing a program properly instead of just
    unplugging the computer.
    
    This function can clean up either one specific PSU or all PSUs at once.
    
    Args:
        device_index (int, optional): Which PSU to clean up (like PSU #0, PSU #1, etc.)
                                    If not provided (None), cleans up ALL PSUs
    
    Returns:
        bool: Always returns True (cleanup is generally safe to attempt)
    
    Side Effects:
        - Removes PSU instances from internal memory
        - Frees up computer resources used for PSU control
        - PSU hardware may remain in its last configured state
        - After cleanup, you'll need to initialize again to use the PSU
    
    Example:
        # Clean up just PSU #0
        cleanup_psu(0)
        
        # Clean up all PSUs
        cleanup_psu()  # or cleanup_psu(None)
    
    When to Use:
        - At the end of your program before exiting
        - When switching to a different PSU configuration
        - If you encounter errors and want to start fresh
        - As part of proper "shutdown" procedures
    
    Notes:
        - Safe to call even if PSU wasn't initialized
        - Safe to call multiple times
        - Doesn't change PSU hardware settings, just releases software control
        - Good programming practice to always clean up resources
    """
    global _psu_instances
    if device_index is None:
        # Clean up all instances
        for idx in list(_psu_instances.keys()):
            cleanup_psu(idx)
    else:
        if device_index in _psu_instances:
            print(f"Cleaning up PSU instance for device {device_index}")
            del _psu_instances[device_index]
        else:
            print(f"PSU instance for device {device_index} already None or not initialized.")
    return True

# Legacy compatibility functions for single PSU operation (device_index=0)
_psu_instance = None  # For backward compatibility

def get_psu_instance_legacy(device_index=0, verb=False):
    """
    Gets a PSU instance using the old-style interface (for compatibility).
    
    This function exists to support older code that was written before this
    multi-PSU system was created. "Legacy" means "old way of doing things" -
    some existing programs might still use the old function names and expect
    them to work the same way.
    
    Instead of using this function, new code should use get_psu_instance() directly.
    
    Args:
        device_index (int): Which PSU device to get (default: 0 for first PSU)
        verb (bool): Enable verbose/detailed logging (default: False)
    
    Returns:
        object: PSU instance if successful, None if initialization fails
    
    Side Effects:
        - Updates the global _psu_instance variable (for old code compatibility)
        - May trigger PSU initialization if not already done
    
    Notes:
        - This is provided only for backward compatibility
        - New code should use get_psu_instance() instead
        - Maintained to avoid breaking existing programs
        - Uses simplified parameter set compared to full get_psu_instance()
    """
    global _psu_instance
    _psu_instance = get_psu_instance(device_index=device_index, verb=verb)
    return _psu_instance

# Main execution block (for testing)
if __name__ == '__main__':
    setup_module_path_and_load()

    if not _module_loaded:
        print("\nExiting script as module could not be loaded.")
        sys.exit(1)

    print("\nTesting dual PSU initialization...")
    
    # Test initializing two PSUs - Pi-compatible order (works on both Mac and Pi)
    print("\nInitializing FUG PSU (device 1)...")
    if initialize_psu(device_index=1, max_v=50000.0, max_c=0.5, verb=True):
        print("FUG PSU initialized successfully.")
    else:
        print("Failed to initialize FUG PSU.")
    time.sleep(3)
    print("\nInitializing Heinzinger PSU (device 0)...")
    if initialize_psu(device_index=0, max_v=30000.0, max_c=2.0, verb=True):
        print("Heinzinger PSU initialized successfully.")
    else:
        print("Failed to initialize Heinzinger PSU.")
    
    print(f"\nTotal PSU instances: {len(_psu_instances)}")
    print(f"Device indices: {list(_psu_instances.keys())}")
    
    # Cleanup
    cleanup_psu()
    print("Test completed.")
