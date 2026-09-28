"""Bar/segment gauges used in place of exact times for accounts that are not
allowed to see them (see MainWindow.is_admin).

Everything here maps a raw time value onto a coarse level, so the picture
conveys "plenty left / getting low / stale" without the reader being able to
read a number of minutes or seconds back off it.
"""
from __future__ import annotations

import math

from PySide6.QtCore import QRectF, QSize, Qt
from PySide6.QtGui import QColor, QFontMetrics, QPainter
from PySide6.QtWidgets import QLabel, QSizePolicy, QWidget

import theme

SEGMENTS = 5
BAR_HEIGHT = 8
_FRESH_STEPS_MS = (60_000, 120_000, 240_000)  # <=1', <=2', <=4' — then stale until OFFLINE_STALE_MS


def license_fraction(remain, total_minutes) -> float:
    """Remaining/total in 0..1. `total_minutes` is what this app last Set
    (unknown → treat the current remainder as the whole, i.e. full while
    there is any time left)."""
    try:
        remain_f = float(remain)
    except (TypeError, ValueError):
        return 0.0
    if remain_f <= 0:
        return 0.0
    total = total_minutes if total_minutes and total_minutes > 0 else remain_f
    return max(0.0, min(1.0, remain_f / total))


def license_level(remain, total_minutes) -> int:
    """license_fraction() in SEGMENTS steps: 0 = none left, else 1..SEGMENTS
    (rounded up, so any time left shows at least one segment)."""
    fraction = license_fraction(remain, total_minutes)
    if fraction <= 0:
        return 0
    return max(1, min(SEGMENTS, math.ceil(fraction * SEGMENTS)))


def freshness_level(ago_ms) -> int:
    """0 = no data / stale, else 1..SEGMENTS (5 = heard from just now)."""
    if ago_ms is None:
        return 0
    try:
        ago = float(ago_ms)
    except (TypeError, ValueError):
        return 0
    if ago >= theme.OFFLINE_STALE_MS:
        return 0
    if ago <= _FRESH_STEPS_MS[0]:
        return 5
    if ago <= _FRESH_STEPS_MS[1]:
        return 4
    if ago <= _FRESH_STEPS_MS[2]:
        return 3
    return 2


def freshness_color(level: int) -> str:
    if level >= 4:
        return theme.GREEN
    if level >= 2:
        return theme.ORANGE
    return theme.RED


def paint_meter(painter: QPainter, rect: QRectF, color: str, *, fraction: float | None = None,
                level: int | None = None) -> None:
    """Continuous bar (`fraction`) or SEGMENTS-step bar (`level`) in `rect`."""
    painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
    painter.setPen(Qt.PenStyle.NoPen)
    radius = rect.height() / 2
    if level is None:
        painter.setBrush(QColor(theme.BAR_TRACK))
        painter.drawRoundedRect(rect, radius, radius)
        fill = max(0.0, min(1.0, fraction or 0.0))
        if fill > 0:
            width = max(rect.height(), rect.width() * fill)  # keep a visible nub for tiny values
            painter.setBrush(QColor(color))
            painter.drawRoundedRect(QRectF(rect.x(), rect.y(), width, rect.height()), radius, radius)
        return
    gap = 3.0
    seg_w = (rect.width() - gap * (SEGMENTS - 1)) / SEGMENTS
    for i in range(SEGMENTS):
        painter.setBrush(QColor(color if i < level else theme.BAR_TRACK))
        painter.drawRoundedRect(
            QRectF(rect.x() + i * (seg_w + gap), rect.y(), seg_w, rect.height()), radius, radius
        )


class MeterBar(QWidget):
    """Standalone gauge (dialog tiles, table cells)."""

    def __init__(self, parent=None, *, min_width: int = 60):
        super().__init__(parent)
        self._color = theme.GREEN
        self._fraction: float | None = 0.0
        self._level: int | None = None
        self.setMinimumWidth(min_width)
        self.setFixedHeight(BAR_HEIGHT + 4)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

    def set_fraction(self, fraction: float, color: str) -> None:
        self._fraction, self._level, self._color = fraction, None, color
        self.update()

    def set_level(self, level: int, color: str) -> None:
        self._fraction, self._level, self._color = None, level, color
        self.update()

    def sizeHint(self) -> QSize:  # noqa: N802 (Qt override)
        return QSize(max(self.minimumWidth(), 90), BAR_HEIGHT + 4)

    def paintEvent(self, _event) -> None:  # noqa: N802 (Qt override)
        painter = QPainter(self)
        rect = QRectF(0, (self.height() - BAR_HEIGHT) / 2, self.width(), BAR_HEIGHT)
        paint_meter(painter, rect, self._color, fraction=self._fraction, level=self._level)


class MeterChip(QLabel):
    """The card's "License"/"Uptime" chip. Normally plain rich text; in
    meter mode the same label keeps its caption and paints a gauge to the
    right of it, so the chip's height (and the fixed-size card) is unchanged
    whichever mode is showing."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._meter: tuple | None = None
        self._caption = ""

    def set_caption(self, caption: str, muted: str) -> None:
        """Bare caption (no value) for meter mode."""
        self._caption = caption
        self.setText(f'<span style="color:{muted};font-weight:700">{caption}</span>')

    def set_meter(self, color: str, *, fraction: float | None = None, level: int | None = None) -> None:
        self._meter = (color, fraction, level)
        self.update()

    def clear_meter(self) -> None:
        self._caption = ""
        if self._meter is not None:
            self._meter = None
            self.update()

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt override)
        super().paintEvent(event)
        if self._meter is None:
            return
        color, fraction, level = self._meter
        bold = self.font()
        bold.setBold(True)
        # Fixed caption width sets the gauge's left edge, so every
        # chip's gauge lines up.
        caption_w = QFontMetrics(bold).horizontalAdvance("Uptime")
        left = 8 + caption_w + 14  # chip's own left padding + caption + gap
        right = self.width() - 10
        painter = QPainter(self)
        rect = QRectF(left, (self.height() - BAR_HEIGHT) / 2, max(20, right - left), BAR_HEIGHT)
        paint_meter(painter, rect, color, fraction=fraction, level=level)
