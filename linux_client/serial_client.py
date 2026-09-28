"""USB serial transport for the same {"action":...} JSON protocol HubClient
speaks over WebSocket (see hub_client.py and local_web.h's "USB serial
transport" section on the firmware side).

The ESP32-S3 in this project runs Serial over its native USB (hwcdc), so the
same cable used to flash/monitor the board doubles as a wired control link —
one newline-delimited JSON message per line, in both directions. Exposing the
exact same signals/methods as HubClient lets MainWindow treat both transports
interchangeably (see MainWindow.client in main_window.py).
"""
from __future__ import annotations

import json

from PySide6.QtCore import QObject, Signal
from PySide6.QtSerialPort import QSerialPort, QSerialPortInfo

DEFAULT_BAUD_RATE = 115200


def list_serial_ports() -> list[str]:
    return [info.portName() for info in QSerialPortInfo.availablePorts()]


class SerialHubClient(QObject):
    connected = Signal()
    disconnected = Signal()
    socketError = Signal(str)
    message = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._port = QSerialPort(self)
        self._port.errorOccurred.connect(self._on_error)
        self._port.readyRead.connect(self._on_ready_read)
        self._buffer = bytearray()

    def connect_to(self, port_name: str, baud_rate: int = DEFAULT_BAUD_RATE) -> None:
        if self._port.isOpen():
            self._port.close()
        # /dev/ttyACM0 works as-is; a bare device name like "ttyACM0" also
        # needs setPortName (Qt resolves it under /dev on Linux).
        self._port.setPortName(port_name)
        self._port.setBaudRate(baud_rate)
        self._buffer.clear()
        if self._port.open(QSerialPort.OpenModeFlag.ReadWrite):
            # The ESP32-S3's native USB-Serial/JTAG controller maps DTR/RTS
            # control-line requests to its auto-reset circuit (the same one
            # esptool uses to reset into the app vs. drop into the ROM
            # bootloader). Whatever state Qt/the OS driver leaves those
            # lines in by default on open() is unspecified — if it happens
            # to land on the "hold in bootloader" combination, the board
            # sits there silently waiting for esptool sync bytes and never
            # answers anything we send. Explicitly clearing both right after
            # open forces the normal "just run the app" state.
            self._port.setDataTerminalReady(False)
            self._port.setRequestToSend(False)
            self.connected.emit()
        else:
            self.socketError.emit(self._port.errorString())

    def close(self) -> None:
        was_open = self._port.isOpen()
        self._port.close()
        if was_open:
            self.disconnected.emit()

    def is_connected(self) -> bool:
        return self._port.isOpen()

    def send(self, action: str, data: dict | None = None) -> None:
        if not self.is_connected():
            return
        line = json.dumps({"action": action, "data": data or {}}) + "\n"
        self._port.write(line.encode("utf-8"))

    def _on_ready_read(self) -> None:
        self._buffer += bytes(self._port.readAll())
        while b"\n" in self._buffer:
            line, _, rest = self._buffer.partition(b"\n")
            self._buffer = bytearray(rest)
            text = line.strip()
            if not text:
                continue
            try:
                obj = json.loads(text.decode("utf-8", errors="replace"))
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                self.message.emit(obj)

    def _on_error(self, error: QSerialPort.SerialPortError) -> None:
        if error == QSerialPort.SerialPortError.NoError:
            return
        self.socketError.emit(self._port.errorString())
        if self._port.isOpen():
            self._port.close()
        self.disconnected.emit()
