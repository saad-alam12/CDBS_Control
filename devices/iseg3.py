import serial
import time

class IsegPSU:
    def __init__(self, port, baudrate=9600, timeout=1):
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=8,
            stopbits=1,
            timeout=timeout
        )
        if self.ser.isOpen():
            print('Serial port is open')
        else:
            print('Serial port is not open')


    def send_command(self, command):
        self.ser.write((command + '\r\n').encode())
        time.sleep(0.1)
        print('Sent command', command, 'to Iseg')

    def read_response(self):
        time.sleep(0.1)
        response1 = self.ser.readline().decode()
        response2 = self.ser.readline().decode()
        if response2:
            print('Received response ', response2)
            return response2
        elif response1:
            print('Echoed back command: ', response1)
            return response1
        else:
            print('No response received')
            return ''

    def send_and_read(self, command):
        self.send_command(command)
        return self.read_response()

    def read_bytes(self, n=1):
        time.sleep(0.3)
        response = self.ser.read(n)
        print(f'Raw bytes: {response}')
        try:
            return response.decode()
        except Exception:
            return response

    def send_and_readbytes(self, command):
        self.send_command(command)
        return self.read_bytes()

    def close(self):
        self.ser.close()

    
    def idn(self):
        return self.send_and_read('*IDN?')

    def conf_hvmicc_query(self):
        return self.send_and_read(':CONF:HVMICC?')

    def conf_hvmicc_ok(self):
        return self.send_and_read(':CONF:HVMICC HV_OK')

    def v_nom_query(self, ch=0):
        return self.send_and_read(f':SYStem:USER:VOLTage:NOMinal? (@{ch})')

    def set_voltage(self, volts, ch=0):
        return self.send_and_read(f':VOLT {volts}, (@{ch})')

    def hv_on(self, ch=0):
        return self.send_and_read(f':VOLT ON,(@{ch})')

    def hv_off(self, ch=0):
        return self.send_and_read(f':VOLT OFF,(@{ch})')

    def read_voltage_set(self, ch=0):
        return self.send_and_read(f':READ:VOLT? (@{ch})')

    def meas_voltage(self, ch=0):
        return self.send_and_read(f'MEAS:VOLT? (@{ch})')
    

if __name__ == '__main__':
    psu = IsegPSU(port='/dev/tty.usbserial-A600BYA0', baudrate=9600, timeout=1)

    psu.idn()

    psu.set_voltage(1000, ch=0)
    state = psu.conf_hvmicc_query().strip()
    if 'HV_NOT_OK' in state:
        psu.conf_hvmicc_ok()
    psu.hv_on(ch=0)
    time.sleep(2)

    psu.read_voltage_set(ch=0)
    psu.meas_voltage(ch=0)

    psu.close()

