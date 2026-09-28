"""Visual theme matching html/master_web.html's palette (see :root CSS vars).

Centralizing the QSS + small style helpers here keeps main_window.py focused
on structure/behaviour instead of pixel values.
"""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPalette
from PySide6.QtWidgets import QGraphicsDropShadowEffect, QLabel, QVBoxLayout, QWidget

ORANGE = "#ff6b1a"
ORANGE_DARK = "#df5008"
ORANGE_SOFT = "#fff0e6"
CREAM = "#fff9f3"
CARD = "#ffffff"
INK = "#29251f"
MUTED = "#81786f"
LINE = "#eee3d8"
GREEN = "#16a56b"
RED = "#df4545"
# A deliberately-vacated grid slot (see EmptySlotCard) - a warm light tint,
# visibly distinct from a filled card's plain white (CARD) but still light,
# with a border/border dark enough to read as an outline rather than a
# near-invisible hairline against it.
EMPTY_SLOT_BG = "#faf5ef"
EMPTY_SLOT_BORDER = "#d9cbb8"

STATUS_COLORS = {
    "ONLINE": ("#168a5b", "#e8f8f1"),
    "NO_RESPONSE": ("#df4545", "#ffeded"),
    "OFFLINE": ("#df4545", "#ffeded"),
    "REFRESHING": (ORANGE, ORANGE_SOFT),
    "UNKNOWN": (MUTED, "#f2efec"),
}

# The Hub only updates a node's reported responseState on an actual reply
# (Refresh/Scan/Get*/its own 5-minute auto info-push) - it never flips a
# node to offline just because time passed. A board that was online and
# then got unplugged keeps showing "ONLINE" forever until something happens
# to query it again. 6 minutes is a little past that 5-minute auto-push
# cycle, so a board still genuinely running isn't flagged offline just for
# landing between two of its own pushes.
OFFLINE_STALE_MS = 6 * 60 * 1000


def effective_response_state(node: dict) -> str:
    """node.get("responseState") with staleness applied: an "ONLINE" node
    whose lastSeenAgoMs is older than OFFLINE_STALE_MS renders as
    "NO_RESPONSE" instead, so the UI doesn't have to wait for an operator
    to notice and hit Refresh before an unplugged board stops looking
    online. Every other state passes through unchanged."""
    state = node.get("responseState", "UNKNOWN")
    if state == "ONLINE":
        last_seen = node.get("lastSeenAgoMs")
        if last_seen is not None and last_seen > OFFLINE_STALE_MS:
            return "NO_RESPONSE"
    return state


def status_pill(text: str) -> QLabel:
    color, bg = STATUS_COLORS.get(text, STATUS_COLORS["UNKNOWN"])
    label = QLabel(text)
    label.setAlignment(Qt.AlignmentFlag.AlignCenter)
    label.setStyleSheet(
        f"color:{color}; background:{bg}; border-radius:9px; padding:3px 11px;"
        f"font-size:11px; font-weight:800;"
    )
    return label


# Soft pastel (bg, accent-text) pairs for small labeled stat tiles (e.g.
# DeviceDetailDialog's info grid) — same light-tint-on-white-card language
# as STATUS_COLORS/the dashboard stat cards, just more hues so a row of
# tiles doesn't read as one flat block.
TILE_COLORS = {
    "orange": ("#fff0e6", "#d95a13"),
    "blue": ("#edf4ff", "#3478c9"),
    "purple": ("#f1edfd", "#6a5cff"),
    "teal": ("#e6f9f6", "#0f9488"),
    "pink": ("#fdeef5", "#c2478a"),
    "amber": ("#fff8e1", "#b8860b"),
}

# Progress-bar track color, shared by every plain (non-gradient) QProgressBar
# in the app (this file's own QProgressBar rule below, plus dialogs.py's
# license countdown bar) so both stay in sync from one place.
BAR_TRACK = "#f2ede4"


def countdown_color(percent: float) -> str:
    """Traffic-light color for a license time-remaining bar: plenty left is
    green, getting low is orange, about to expire is red."""
    if percent >= 50:
        return GREEN
    if percent >= 20:
        return ORANGE
    return RED


class SimpleMenu(QWidget):
    """A small right-click context menu, built from plain QLabel rows
    instead of QMenu.

    QMenu's popup window kept rendering as a solid black bar on affected
    systems no matter what was tried against it here in turn - no
    border-radius on the popup, WA_TranslucentBackground/
    WA_NoSystemBackground forced off, NoDropShadowWindowHint added. All of
    those target the *symptom* (Qt or the platform theme requesting an
    ARGB/alpha surface for the popup's rounded corners or drop shadow) but
    on a window manager/X server that can't actually composite that
    surface, whatever ends up requesting it wins and the uncomposited
    "transparent" pixels paint solid black instead of see-through - and at
    least one more thing (very likely the platform/style theme plugin
    itself, e.g. a KDE/Kvantum or GTK integration, re-applying its own
    popup translucency after each show()) was still doing that even with
    every app-level flag/attribute above turned off.
    This sidesteps the whole question: Qt::Popup for click-outside-to-close
    (the same window type QMenu itself uses - unrelated to the shadow
    behavior above) but FramelessWindowHint instead of a native menu, a
    palette background set directly (more fundamental than a stylesheet,
    and not something a style plugin swaps out the way it can override
    WA_TranslucentBackground) on top of the same background via QSS, and
    no rounded corners anywhere on the top-level window's own shape - so
    there's never a reason for Qt or the platform theme to ask for an
    alpha-channel surface for this window in the first place."""

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent, Qt.WindowType.Popup | Qt.WindowType.FramelessWindowHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, False)
        self.setAttribute(Qt.WidgetAttribute.WA_NoSystemBackground, False)
        self.setAutoFillBackground(True)
        palette = self.palette()
        palette.setColor(QPalette.ColorRole.Window, QColor(CARD))
        self.setPalette(palette)
        self.setStyleSheet(f"background:{CARD}; border:1px solid {LINE};")
        self._layout = QVBoxLayout(self)
        self._layout.setContentsMargins(4, 4, 4, 4)
        self._layout.setSpacing(0)

    def add_action(self, text: str, callback) -> None:
        row = QLabel(text)
        row.setCursor(Qt.CursorShape.PointingHandCursor)
        # Same border width idle vs hover (transparent -> orange) so the row
        # doesn't change size/shift its neighbors the moment the cursor
        # enters it - only 1px reserved either way, just not painted idle.
        idle = f"padding:6px 15px; border-radius:7px; border:1px solid transparent; color:{INK}; font-weight:600;"
        hover = f"padding:6px 15px; border-radius:7px; border:1px solid {ORANGE}; color:{INK}; font-weight:600;"
        row.setStyleSheet(idle)
        row.enterEvent = lambda event, w=row: w.setStyleSheet(hover)
        row.leaveEvent = lambda event, w=row: w.setStyleSheet(idle)

        def handle_click(event, cb=callback) -> None:
            self.close()
            cb()

        row.mousePressEvent = handle_click
        self._layout.addWidget(row)

    def popup(self, global_pos) -> None:
        self.adjustSize()
        self.move(global_pos)
        self.show()


def card_shadow(widget: QWidget, *, blur: int = 30) -> None:
    # QGraphicsDropShadowEffect renders its widget to an offscreen buffer and
    # blurs it in software on every repaint - fine for the handful of
    # dashboard/section cards this is normally used on, but up to ~100
    # DeviceCard tiles on the Devices grid each carrying their own full-size
    # effect used to make scrolling/clicking visibly stutter. DeviceCard
    # passes a smaller `blur` to keep the same look at a fraction of the
    # compositing cost (see device_card.py).
    effect = QGraphicsDropShadowEffect(widget)
    effect.setBlurRadius(blur)
    effect.setOffset(0, 8)
    effect.setColor(QColor(135, 56, 0, 18))
    widget.setGraphicsEffect(effect)


STYLESHEET = f"""
QWidget {{
    font-family: "Inter", "Segoe UI", Arial, sans-serif;
    font-size: 13px;
    color: {INK};
}}
#Root {{ background: {CREAM}; }}
#Sidebar {{ background: {CARD}; border-right: 1px solid {LINE}; }}
#Brand {{ font-size: 12px; font-weight: 800; letter-spacing: 0; color: {INK}; }}

QPushButton#NavButton {{
    text-align: left;
    padding: 11px 14px;
    border: none;
    border-radius: 12px;
    background: transparent;
    color: #6f675f;
    font-weight: 700;
}}
QPushButton#NavButton:hover {{ background: #fff8f2; color: #d95a13; }}
QPushButton#NavButton:checked {{ background: {ORANGE_SOFT}; color: {ORANGE}; }}

QPushButton#FootButton {{
    text-align: left;
    padding: 10px 14px;
    border: none;
    border-radius: 12px;
    background: transparent;
    color: #6f675f;
    font-weight: 700;
}}
QPushButton#FootButton:hover {{ background: #fff8f2; color: #d95a13; }}

#Topbar {{ background: transparent; }}
#PageTitle {{ font-size: 22px; font-weight: 800; color: {INK}; }}
#PageSubtitle {{ color: {MUTED}; font-size: 12px; }}

#StatusPill {{
    background: {CARD};
    border: 1px solid {LINE};
    border-radius: 999px;
}}
#StatusPill QLabel {{ font-weight: 700; color: {MUTED}; }}
#StatusDot {{ border: none; border-radius: 5px; background: {RED}; }}
#StatusDot[connected="true"] {{ background: {GREEN}; }}

#Card {{
    background: {CARD};
    border: 1px solid {LINE};
    border-radius: 18px;
}}
#CardTitle {{ font-size: 16px; font-weight: 800; color: {INK}; }}
#CardSubtitle {{ color: {MUTED}; font-size: 12px; }}

#StatCard {{ border-radius: 16px; border: 1px solid transparent; }}
#StatCard[variant="online"] {{ background: #eaf8f1; border-color: #ccebdd; }}
#StatCard[variant="offline"] {{ background: #fff0f0; border-color: #f5d4d4; }}
#StatCard[variant="groups"] {{ background: #edf4ff; border-color: #d4e4fa; }}
#StatTitle {{ font-size: 12px; font-weight: 800; letter-spacing: .06em; }}
#StatCard[variant="online"] #StatTitle, #StatCard[variant="online"] #StatValue {{ color: #168a5b; }}
#StatCard[variant="offline"] #StatTitle, #StatCard[variant="offline"] #StatValue {{ color: #d94a4a; }}
#StatCard[variant="groups"] #StatTitle, #StatCard[variant="groups"] #StatValue {{ color: #3478c9; }}
#StatValue {{ font-size: 30px; font-weight: 800; }}
#StatSubtitle {{ color: {MUTED}; font-size: 11px; font-weight: 600; }}

/* Explicit indicator so a DeviceCard's bulk-select checkbox never depends
   on the OS/Qt style's own checkbox artwork rendering visibly against the
   card's white background (it doesn't reliably, at least in this app's
   test environment - it painted as blank/invisible with no styling at all,
   easy to miss entirely on a device card). */
QCheckBox::indicator {{
    width: 16px;
    height: 16px;
    border: 1px solid {LINE};
    border-radius: 4px;
    background: {CARD};
}}
QCheckBox::indicator:hover {{ border-color: {ORANGE}; }}
QCheckBox::indicator:checked {{ background: {ORANGE}; border-color: {ORANGE}; }}

/* The row-select checkbox (first card of each row - see DeviceCard) needs
   to read as a clearly different control from the plain per-card one right
   next to it, not just a second copy of the same square box - round
   instead of square does that at a glance - but should still turn the
   same orange as every other checkbox once checked, not a different
   color, to read as "selected" consistently across the app. */
QCheckBox#RowCheckbox::indicator {{
    width: 16px;
    height: 16px;
    border: 2px solid {LINE};
    border-radius: 9px;
    background: {CARD};
}}
QCheckBox#RowCheckbox::indicator:hover {{ border-color: {ORANGE}; }}
QCheckBox#RowCheckbox::indicator:checked {{ background: {ORANGE}; border-color: {ORANGE}; }}

QPushButton {{
    border: 1px solid {LINE};
    border-radius: 10px;
    background: {CARD};
    color: {INK};
    padding: 8px 14px;
    font-weight: 700;
}}
QPushButton:hover {{ border-color: #f0b58f; }}
QPushButton:disabled {{ color: #bbb2aa; background: #fffaf6; border-color: {LINE}; }}
QPushButton[variant="primary"] {{ background: {ORANGE}; border-color: {ORANGE}; color: white; }}
QPushButton[variant="primary"]:hover {{ background: {ORANGE_DARK}; }}
QPushButton[variant="danger"] {{ color: {RED}; background: #fff5f5; border-color: #ffd6d6; }}
QPushButton[variant="danger"]:hover {{ background: #ffeaea; }}

/* Group picker chips (Groups page header) - a row of checkable buttons,
   one per Group, standing in for the old vertical QListWidget. */
QPushButton#GroupChip {{
    border-radius: 999px;
    padding: 6px 14px;
    font-weight: 700;
}}
QPushButton#GroupChip:checked {{ background: {ORANGE_SOFT}; border-color: {ORANGE}; color: {ORANGE}; }}

QLineEdit, QSpinBox, QComboBox {{
    border: 1px solid {LINE};
    border-radius: 10px;
    background: {CARD};
    padding: 7px 10px;
    selection-background-color: {ORANGE_SOFT};
}}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {{ border-color: {ORANGE}; }}

QTableWidget {{
    background: {CARD};
    border: none;
    gridline-color: {LINE};
    selection-background-color: {ORANGE_SOFT};
    selection-color: {INK};
}}
QHeaderView::section {{
    background: {CARD};
    color: {MUTED};
    border: none;
    border-bottom: 1px solid {LINE};
    padding: 8px;
    font-weight: 800;
    font-size: 11px;
    text-transform: uppercase;
}}
QTableWidget::item {{ padding: 4px; border-bottom: 1px solid {LINE}; }}

QListWidget {{
    background: {CARD};
    border: 1px solid {LINE};
    border-radius: 12px;
    padding: 4px;
}}
QListWidget::item {{ padding: 9px 10px; border-radius: 9px; }}
QListWidget::item:selected {{ background: {ORANGE_SOFT}; color: {ORANGE}; }}

QProgressBar {{
    border: none;
    border-radius: 7px;
    background: {ORANGE_SOFT};
    height: 10px;
    text-align: center;
    color: transparent;
}}
QProgressBar::chunk {{ background: {ORANGE}; border-radius: 7px; }}

/* Thin multi-color gradient bars (Dashboard "Overview" card) — same idea as
   the colorful metric bars in logo/dashboard.jpg, kept on the light track
   used everywhere else instead of that reference's dark theme. */
QProgressBar[gradientBar="true"] {{
    border: none;
    border-radius: 5px;
    background: #f2ede4;
    min-height: 8px;
    max-height: 8px;
    text-align: center;
    color: transparent;
}}
QProgressBar#GradientOnline::chunk {{
    border-radius: 5px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 {ORANGE}, stop:1 #ff4d8d);
}}
QProgressBar#GradientJob::chunk {{
    border-radius: 5px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff4d8d, stop:1 #6a5cff);
}}
QProgressBar#GradientGroups::chunk {{
    border-radius: 5px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #6a5cff, stop:1 #14b8a6);
}}

QPlainTextEdit#LogView {{
    background: #fffaf6;
    border: 1px solid {LINE};
    border-radius: 12px;
    color: {MUTED};
    font-family: "Consolas", "Menlo", monospace;
    font-size: 11.5px;
    padding: 8px;
}}

QScrollArea {{ border: none; background: transparent; }}
QTabWidget::pane {{ border: none; }}
QStatusBar {{ background: {CARD}; border-top: 1px solid {LINE}; color: {INK}; font-weight: 600; }}
QStatusBar::item {{ border: none; }}
QSplitter::handle {{ background: transparent; }}
"""
