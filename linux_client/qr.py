"""Decode the packed-hex QR bitmap sent by account.2fa.setup into a QImage.

Firmware encoding (account_auth.h captureAccountQr): for each row, groups of
4 modules are packed MSB-first into one hex nibble; a short final group is
zero-padded on the low bits.
"""
from __future__ import annotations

from PySide6.QtGui import QImage, QPixmap, qRgb

BLACK = qRgb(0, 0, 0)
WHITE = qRgb(255, 255, 255)


def decode_qr_bitmap(size: int, hex_data: str) -> QImage:
    chars_per_row = (size + 3) // 4
    image = QImage(size, size, QImage.Format.Format_RGB32)
    image.fill(WHITE)
    for y in range(size):
        row_offset = y * chars_per_row
        for c in range(chars_per_row):
            if row_offset + c >= len(hex_data):
                break
            nibble = int(hex_data[row_offset + c], 16)
            for bit in range(4):
                x = c * 4 + bit
                if x >= size:
                    break
                on = (nibble >> (3 - bit)) & 1
                if on:
                    image.setPixel(x, y, BLACK)
    return image


def qr_pixmap(size: int, hex_data: str, scale: int = 8) -> QPixmap:
    image = decode_qr_bitmap(size, hex_data)
    scaled = image.scaled(size * scale, size * scale)
    return QPixmap.fromImage(scaled)
