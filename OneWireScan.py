import board
from adafruit_onewire.bus import OneWireBus
ow_bus = OneWireBus(board.A0)
devices = ow_bus.scan()
for device in devices:
    print("ROM = {} \tFamily = 0x{:02x}".format([hex(i) for i in device.rom], device.family_code))
