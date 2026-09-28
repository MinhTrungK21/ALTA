"""WebSocket client for talking to the HUB66S local web JSON protocol.

The firmware (see local_web.h) exposes a WebSocket server on port 81 that
accepts {"action": "...", "data": {...}} commands and pushes JSON events
(state, device.upsert, job, ...). This class is a thin Qt wrapper around
QWebSocket so the rest of the app can just connect to Qt signals.
"""
from __future__ import annotations

import json

from PySide6.QtCore import QObject, QUrl, Signal
from PySide6.QtNetwork import QAbstractSocket
from PySide6.QtWebSockets import QWebSocket

DEFAULT_PORT = 81


class HubClient(QObject):
    connected = Signal()
    disconnected = Signal()
    socketError = Signal(str)
    message = Signal(dict)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._socket = QWebSocket()
        self._socket.connected.connect(self.connected)
        self._socket.disconnected.connect(self.disconnected)
        self._socket.errorOccurred.connect(self._on_error)
        self._socket.textMessageReceived.connect(self._on_text)

    def connect_to(self, host: str, port: int = DEFAULT_PORT) -> None:
        self._socket.open(QUrl(f"ws://{host}:{port}"))

    def close(self) -> None:
        self._socket.close()

    def is_connected(self) -> bool:
        return self._socket.state() == QAbstractSocket.SocketState.ConnectedState

    def send(self, action: str, data: dict | None = None) -> None:
        if not self.is_connected():
            return
        payload = {"action": action, "data": data or {}}
        self._socket.sendTextMessage(json.dumps(payload))

    def _on_text(self, text: str) -> None:
        try:
            obj = json.loads(text)
        except json.JSONDecodeError:
            return
        if isinstance(obj, dict):
            self.message.emit(obj)

    def _on_error(self, _code) -> None:
        self.socketError.emit(self._socket.errorString())
