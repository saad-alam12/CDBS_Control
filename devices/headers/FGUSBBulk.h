/*
 * FGUSBBulk.h
 *
 * Created on: Feb 16, 2016
 * Author: Francesco Guatieri
 */

#ifndef SOURCE_FGUSBBULK_H_
#define SOURCE_FGUSBBULK_H_

#include <iomanip>  // For std::setw, std::setfill
#include <iostream> // For std::cout, std::endl, std::hex, std::dec
#include <libusb-1.0/libusb.h>
#include <string>
#include <unistd.h> // For usleep
#include <vector>

#include "CommonIncludes.h"
#include "Error.h" // For Shout, Utter, and global Verbosity
#include "FGBulk.h"
#include "Hex.h" // For DestToHex
#include "StringUtils.h"

class FGUSBBulk;

bool FGUSBBulk_PrototypeWrite(FGUSBBulk *Params, unsigned char Endpoint,
                              unsigned char *Buffer, unsigned int Length);
bool FGUSBBulk_PrototypeRead(FGUSBBulk *Params, unsigned char Endpoint,
                             unsigned char *Buffer, unsigned int Length);

const int MaxUSBAttempts = 10;

class FGUSBDevice : public libusb_device_descriptor {
public:
  void Dump() {
    std::cout << "Descriptor length: " << (int)bLength << "\t"
              << " Descriptor type: " << (int)bDescriptorType << "\n";
    std::cout << "bcdUSB: " << ToHex(bcdUSB) << "\n";
    std::cout << "ClassCodes: " << (int)bDeviceClass << ":"
              << (int)bDeviceSubClass << "\n";
    std::cout << "Device protocol: " << (int)bDeviceProtocol << "\n";
    std::cout << "Max packet size @ep0: " << (int)bMaxPacketSize0 << "\n";
    std::cout << "VID:PID: " << ToHex(idVendor) << ":" << ToHex(idProduct)
              << "\n";
    std::cout << "BCD Release: " << ToHex(bcdDevice) << "\n";
    std::cout << "Manifacturer: " << (int)iManufacturer << "\n";
    std::cout << "Product: " << (int)iProduct << "\n";
    std::cout << "SerialNo.: " << (int)iSerialNumber << "\n";
    std::cout << "Possible configurations: " << (int)bNumConfigurations
              << "\n\n";
  };
};

inline std::string LibusbErrorName(int ErrorCode) {
  // ... (LibusbErrorName implementation as before) ...
  if (ErrorCode == LIBUSB_SUCCESS)
    return "Success (no error)";
  if (ErrorCode == LIBUSB_ERROR_IO)
    return "Input/output error.";
  if (ErrorCode == LIBUSB_ERROR_INVALID_PARAM)
    return "Invalid parameter.";
  if (ErrorCode == LIBUSB_ERROR_ACCESS)
    return "Access denied (insufficient permissions)";
  if (ErrorCode == LIBUSB_ERROR_NO_DEVICE)
    return "No such device (it may have been disconnected)";
  if (ErrorCode == LIBUSB_ERROR_NOT_FOUND)
    return "Entity not found.";
  if (ErrorCode == LIBUSB_ERROR_BUSY)
    return "Resource busy.";
  if (ErrorCode == LIBUSB_ERROR_TIMEOUT)
    return "Operation timed out.";
  if (ErrorCode == LIBUSB_ERROR_OVERFLOW)
    return "Overflow.";
  if (ErrorCode == LIBUSB_ERROR_PIPE)
    return "Pipe error.";
  if (ErrorCode == LIBUSB_ERROR_INTERRUPTED)
    return "System call interrupted (perhaps due to signal)";
  if (ErrorCode == LIBUSB_ERROR_NO_MEM)
    return "Insufficient memory.";
  if (ErrorCode == LIBUSB_ERROR_NOT_SUPPORTED)
    return "Operation not supported or unimplemented on this platform.";
  if (ErrorCode == LIBUSB_ERROR_OTHER)
    return "Other error.";
  return "Uknown error.";
}

class FGUSBBulk {
  // ... (FGUSBBulk class members and methods as before) ...
private:
  libusb_context *Context;
  libusb_device_handle *Handle;
  bool InterfaceClaimed;
  int InterfaceNo;

public:
  FGBulkBridge Bridge;

  FGUSBBulk()
      : Context(nullptr), Handle(nullptr), InterfaceClaimed(false),
        InterfaceNo(0),
        Bridge(this, (BulkBridgeCallback)FGUSBBulk_PrototypeWrite,
               (BulkBridgeCallback)FGUSBBulk_PrototypeRead) {};

  FGUSBBulk(FGUSBDevice Device, int Interface)
      : Context(nullptr), Handle(nullptr), InterfaceClaimed(false),
        InterfaceNo(0),
        Bridge(this, (BulkBridgeCallback)FGUSBBulk_PrototypeWrite,
               (BulkBridgeCallback)FGUSBBulk_PrototypeRead) {
    InterfaceNo = Interface;
    OpenDevice(Device.idVendor, Device.idProduct,
               Interface); // Call the specific OpenDevice
  };

  FGUSBBulk(uint16_t VID, uint16_t PID, int Interface)
      : Context(nullptr), Handle(nullptr), InterfaceClaimed(false),
        InterfaceNo(Interface),
        Bridge(this, (BulkBridgeCallback)FGUSBBulk_PrototypeWrite,
               (BulkBridgeCallback)FGUSBBulk_PrototypeRead) {
    OpenDevice(VID, PID, Interface);
  };

  ~FGUSBBulk() {
    if (Handle != nullptr && Context != nullptr && InterfaceClaimed)
      if (libusb_release_interface(Handle, InterfaceNo) < 0)
        Shout("Unable to release USB interface");

    if (Handle != nullptr && Context != nullptr)
      libusb_close(Handle);
    if (Context != nullptr)
      libusb_exit(Context);
  };

  bool OpenDevice(FGUSBDevice Device, int Interface) // Keep this overload
  {
    InterfaceNo = Interface;
    return OpenDevice(Device.idVendor, Device.idProduct, Interface, 0);
  }

  // New USB path-based device opening method
  bool OpenDeviceByPath(uint16_t VID, uint16_t PID, int Interface, const std::string& target_usb_path) {
    this->InterfaceNo = Interface;
    if (Context == nullptr && libusb_init(&Context) < 0) {
      Shout("Unable to initialize USB context.");
      return false;
    };

    if (Handle != nullptr)
      CloseDevice();

    // Use system command to find device by USB path (Mac-specific)
    std::string cmd = "ioreg -p IOUSB -l | grep -A 25 'Analog PSU Interface" + target_usb_path + "'";
    
    // Map USB paths to device indexes based on actual enumeration order
    int skip_value = 0;
    if (target_usb_path == "@00110000") {
      skip_value = 0; // Heinzinger path -> device_index 0 (first enumerated)
    } else if (target_usb_path == "@00120000") {
      skip_value = 1; // FUG path -> device_index 1 (second enumerated)
    } else {
      return Shout("Unknown USB path: " + target_usb_path, 0);
    }
    
    return OpenDevice(VID, PID, Interface, skip_value);
  }

  // Legacy device opening method (keep for compatibility)
  bool OpenDevice(uint16_t VID, uint16_t PID, int Interface, int Skip = 0) {
    this->InterfaceNo = Interface;
    if (Context == nullptr && libusb_init(&Context) < 0) {
      Shout("Unable to initialize USB context.");
      return false;
    };

    if (Handle != nullptr)
      CloseDevice();

    libusb_device **DevList;
    ssize_t DeviceCount = libusb_get_device_list(Context, &DevList);
    if (DeviceCount < 0)
      return Shout("Unable to get USB device list", 0);

    int FoundId = -1;
    int current_skip = Skip; // Use a local copy for the skip counter

    for (ssize_t i = 0; i < DeviceCount; ++i) {
      libusb_device_descriptor TempDesc; // Use base descriptor
      int ret = libusb_get_device_descriptor(DevList[i], &TempDesc);
      if (ret < 0) {
        Shout("Failed to get device descriptor for a device.");
        continue; // Skip this device
      }
      if (TempDesc.idVendor == VID && TempDesc.idProduct == PID) {
        if (current_skip > 0) {
          current_skip--;
        } else {
          FoundId = (int)i;
          break;
        }
      }
    };

    if (FoundId == -1) {
      libusb_free_device_list(DevList, 1);
      std::string msg = "Unable to locate requested device VID:0x" +
                        ToHex(VID) + " PID:0x" + ToHex(PID);
      if (Skip > 0)
        msg += " (with skip " + itos(Skip) + ")";
      return Shout(msg, 0);
    }

    int open_ret = libusb_open(DevList[FoundId], &Handle);
    if (open_ret < 0) {
      Handle = nullptr;
      Shout("Unable to open USB device. Libusb error: " +
            LibusbErrorName(open_ret) + " (" + itos(open_ret) + ")");
    };

    libusb_free_device_list(DevList, 1);
    if (Handle == nullptr)
      return false; // Could not open

    // Detach kernel driver if necessary (important on Linux)
    if (libusb_kernel_driver_active(Handle, this->InterfaceNo) == 1) {
      if (Verbosity > 0)
        std::cout << "Kernel driver active on interface " << this->InterfaceNo
                  << ", attempting to detach." << std::endl;
      if (libusb_detach_kernel_driver(Handle, this->InterfaceNo) != 0) {
        Shout("Could not detach kernel driver!");
        libusb_close(Handle);
        Handle = nullptr;
        return false;
      }
    }

    int ICResponse;
    InterfaceClaimed =
        !(ICResponse = libusb_claim_interface(Handle, this->InterfaceNo));

    if (!InterfaceClaimed) {
      Shout("Unable to claim USB interface " + itos(this->InterfaceNo) +
            ". Libusb error: " + LibusbErrorName(ICResponse) + " [" +
            itos(ICResponse) + "]");
      libusb_close(Handle); // Close handle if claim fails
      Handle = nullptr;
    } else {
      if (Verbosity > 0)
        std::cout << "Successfully claimed USB interface " << this->InterfaceNo
                  << std::endl;
    }
    return InterfaceClaimed;
  };

  bool CloseDevice() {
    bool TempRes = true;
    if (Handle != nullptr) {
      if (InterfaceClaimed) {
        int release_ret = libusb_release_interface(Handle, InterfaceNo);
        if (release_ret < 0) {
          TempRes = Shout("Unable to release USB interface. Libusb error: " +
                              LibusbErrorName(release_ret),
                          0);
        } else {
          if (Verbosity > 0)
            std::cout << "Successfully released USB interface " << InterfaceNo
                      << std::endl;
        }
        // libusb_attach_kernel_driver(Handle, InterfaceNo); // Reattach kernel
        // driver if it was detached. Often not needed if app is the sole
        // controller.
      }
      InterfaceClaimed = false;
      libusb_close(Handle);
      Handle = nullptr;
    }
    return TempRes;
  }

  operator bool() {
    return (Context != nullptr) && (Handle != nullptr) && InterfaceClaimed;
  };
  libusb_context *GetContext() { return Context; };
  libusb_device_handle *GetHandle() { return Handle; };
  operator FGBulkBridge *() { return &Bridge; };
};

#ifdef USBTIMEOUTMS
const int USBTransferTimeout = USBTIMEOUTMS;
#else
const int USBTransferTimeout = 100; // Milliseconds
#endif

inline bool FGUSBBulk_PrototypeWrite(FGUSBBulk *Params, unsigned char Endpoint,
                                     unsigned char *Buffer,
                                     unsigned int Length) {
  if (!Params || !*Params ||
      !Params->GetHandle()) { // Added check for GetHandle()
    if (Verbosity > 0)
      std::cerr << "FGUSBBulk_PrototypeWrite: Invalid Params or USB Handle."
                << std::endl;
    return false;
  }

  if (Verbosity > 1) { // Use global Verbosity from Error.h
    std::cout << "USB Write (Endpoint: 0x" << std::hex << (int)Endpoint
              << std::dec << ", Length: " << Length << "): ";
    // DestToHex is from Hex.h, which is included by FGUSBBulk.h
    std::cout << DestToHex(Buffer, Length) << std::endl;
  }

  Endpoint &= 0x0F; // Keep lower 4 bits for endpoint number, OUT is implicit by
                    // direction flag
  int Response = 0;
  int Transferred = 0;
  int Iterations = MaxUSBAttempts;

  while (Transferred < (int)Length && Iterations--) {
    if (Iterations != MaxUSBAttempts - 1)
      usleep(1000 * 10); // 10ms delay on retries

    int Actual = 0;
    Response = libusb_bulk_transfer(
        Params->GetHandle(),
        (Endpoint | LIBUSB_ENDPOINT_OUT), // Endpoint direction explicitly OUT
        Buffer + Transferred, Length - Transferred, &Actual,
        USBTransferTimeout);

    if (Verbosity > 2) { // More detailed logging for each attempt
      std::cout << "  Attempt Write: Ep=0x" << std::hex
                << (Endpoint | LIBUSB_ENDPOINT_OUT)
                << ", Sent=" << (Length - Transferred) << ", Actual=" << Actual
                << ", Resp=" << LibusbErrorName(Response) << " (" << std::dec
                << Response << ")" << std::endl;
    }

    if (Response < 0) {
      // Error already logged by Shout below if total transfer fails
    } else {
      Transferred += Actual;
    };
  };

  if (Transferred != (int)Length) {
    Shout("Unable to write bulk transfer! Wrote " + itos(Transferred) + "/" +
              itos(Length) + " bytes. Last Error: [" + itos(Response) + " " +
              LibusbErrorName(Response) + "]",
          0);
    return false; // Indicate failure
  }
  return true; // Indicate success
}

inline bool FGUSBBulk_PrototypeRead(FGUSBBulk *Params, unsigned char Endpoint,
                                    unsigned char *Buffer,
                                    unsigned int Length) {
  if (!Params || !*Params ||
      !Params->GetHandle()) { // Added check for GetHandle()
    if (Verbosity > 0)
      std::cerr << "FGUSBBulk_PrototypeRead: Invalid Params or USB Handle."
                << std::endl;
    return false;
  }

  Endpoint &= 0x0F; // Keep lower 4 bits, IN is implicit
  int Response = 0;
  int Transferred = 0;
  int Iterations = MaxUSBAttempts;

  while (Transferred < (int)Length && Iterations--) {
    if (Iterations != MaxUSBAttempts - 1)
      usleep(1000 * 10); // 10ms delay

    int Actual = 0;
    Response = libusb_bulk_transfer(
        Params->GetHandle(),
        (Endpoint | LIBUSB_ENDPOINT_IN), // Endpoint direction explicitly IN
        Buffer + Transferred,            // Read into this part of the buffer
        Length - Transferred,            // How much more to read
        &Actual, USBTransferTimeout);

    if (Verbosity > 2) {
      std::cout << "  Attempt Read: Ep=0x" << std::hex
                << (Endpoint | LIBUSB_ENDPOINT_IN)
                << ", ToRead=" << (Length - Transferred)
                << ", Actual=" << Actual
                << ", Resp=" << LibusbErrorName(Response) << " (" << std::dec
                << Response << ")" << std::endl;
    }

    if (Response < 0) {
      // Error logged by Shout below if total transfer fails
    } else {
      Transferred += Actual;
    };
  };

  if (Verbosity > 1 &&
      Transferred > 0) { // Log data if any was read, even if not full length
    std::cout << "USB Read (Endpoint: 0x" << std::hex << (int)Endpoint
              << std::dec << ", Expected: " << Length
              << ", Actual Read: " << Transferred << "): ";
    std::cout << DestToHex(Buffer, Transferred) << std::endl;
  }

  if (Transferred != (int)Length) {
    Shout("Unable to read bulk transfer! Read " + itos(Transferred) + "/" +
              itos(Length) + " bytes. Last Error: [" + itos(Response) + " " +
              LibusbErrorName(Response) + "]",
          0);
    return false; // Indicate failure
  }
  return true; // Indicate success
}

inline std::vector<FGUSBDevice> EnumerateUSBDevices() {
  libusb_context *MyContext;
  if (libusb_init(&MyContext) < 0) {
    Utter("Unable to initialize USB context for enumeration.");
    return {}; // Return empty vector on failure
  }

  libusb_device **MyDevList;
  ssize_t DeviceCount = libusb_get_device_list(MyContext, &MyDevList);

  if (DeviceCount < 0) {
    Utter("Unable to get USB device list for enumeration.");
    libusb_exit(MyContext);
    return {};
  }

  std::vector<FGUSBDevice> TempRes;
  TempRes.reserve(DeviceCount); // Reserve space

  for (ssize_t i = 0; i < DeviceCount; ++i) {
    FGUSBDevice current_device_info; // Use your derived class
    int ret = libusb_get_device_descriptor(
        MyDevList[i], (libusb_device_descriptor *)&current_device_info);
    if (ret == 0) {
      TempRes.push_back(current_device_info);
    }
  }

  libusb_free_device_list(MyDevList, 1);
  libusb_exit(MyContext);
  return TempRes;
}

#endif /* SOURCE_FGUSBBULK_H_ */