"""One clickable tile representing a single ESP node, used by the Devices
dashboard grid in main_window.py (replaces the old flat table)."""
from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QMimeData, QSize, Qt, Signal
from PySide6.QtGui import QDrag, QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QApplication,
    QCheckBox,
    QFrame,
    QHBoxLayout,
    QLabel,
    QListView,
    QListWidget,
    QListWidgetItem,
    QVBoxLayout,
)

DEVICE_MAC_MIME_TYPE = "application/x-hub66s-device-mac"

import theme
from meters import MeterChip, license_fraction, license_level

CARD_SIZE = QSize(186, 300)  # width kept small so 5 columns fit a 1366px screen; height fits 3 chip rows


def _chip_html(label: str, value: str, accent: str) -> str:
    return (
        f'<span style="color:{theme.MUTED};font-weight:700">{label}</span>'
        f'&nbsp;&nbsp;<span style="color:{accent};font-weight:800">{value}</span>'
    )


def _fmt_mmss(minutes) -> str:
    """Minutes as M' (whole) or M'SS\" (if a future firmware ever reports
    sub-minute remaining time). e.g. 44 -> 44', 44.5 -> 44'30\"."""
    if minutes is None:
        return "—"
    try:
        total_s = int(round(float(minutes) * 60))
    except (TypeError, ValueError):
        return "—"
    m, s = divmod(total_s, 60)
    return f"{m}'" if s == 0 else f"{m}'{s:02d}\""


def _fmt_uptime(minutes) -> str:
    if minutes is None:
        return "—"
    try:
        m = int(minutes)
    except (TypeError, ValueError):
        return "—"
    if m < 60:
        return f"{m} phút"
    h, mm = divmod(m, 60)
    if h < 24:
        return f"{h}h{mm:02d}"
    d, hh = divmod(h, 24)
    return f"{d}n {hh}h"

_LOGO_DIR = Path(__file__).resolve().parent.parent / "logo"
BOARD_IMAGE_PATH = _LOGO_DIR / "CardRoi.png"  # "R" - card rời (removable card)
BOARD_IMAGE_PATH_OB = _LOGO_DIR / "On_board.png"   # "OB" - card onboard
_BOARD_IMAGE_PATHS = {"R": BOARD_IMAGE_PATH, "OB": BOARD_IMAGE_PATH_OB}
_board_pixmap_cache: dict[str, QPixmap | None] = {}


def board_pixmap(card_type: str = "R") -> QPixmap | None:
    """The HUB66S board photo for this card variant - see node_info_t's
    cardType / the Set Card Type job (local_web.h). Falls back to the
    removable-card photo if the onboard one hasn't been added to logo/ yet,
    and to None if neither file exists (dialogs/cards just skip the picture
    then). Loaded once per variant and cached."""
    if card_type not in _board_pixmap_cache:
        path = _BOARD_IMAGE_PATHS.get(card_type, BOARD_IMAGE_PATH)
        if not path.exists():
            path = BOARD_IMAGE_PATH
        _board_pixmap_cache[card_type] = QPixmap(str(path)) if path.exists() else None
    return _board_pixmap_cache[card_type]


_CARD_IMAGE_WIDTH = 110
_scaled_board_pixmap_cache: dict[str, QPixmap] = {}


_CARD_IMAGE_MAX_HEIGHT = 118  # ~ the squarish OB/legacy R photos' height at _CARD_IMAGE_WIDTH


def _scaled_board_pixmap(card_type: str) -> QPixmap | None:
    """board_pixmap(), pre-scaled to fit the card's fixed image box and
    cached — every DeviceCard of the same variant reuses the same scaled
    pixmap instead of rescaling the source image on every set_node() call.
    Bounded on both width and height (not just scaledToWidth()): the card
    variants' source photos don't all share the same aspect ratio (e.g.
    CardRoi.jpeg is a tall 3:4 phone photo, not the older cutouts' near-
    square crop), and scaling to width alone let a tall photo grow past the
    fixed-size card's own layout, crowding the title/chips below it."""
    if card_type not in _scaled_board_pixmap_cache:
        raw = board_pixmap(card_type)
        if raw is None:
            return None
        _scaled_board_pixmap_cache[card_type] = raw.scaled(
            _CARD_IMAGE_WIDTH, _CARD_IMAGE_MAX_HEIGHT,
            Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation,
        )
    return _scaled_board_pixmap_cache[card_type]


class DeviceCard(QFrame):
    """Click the body to open device details; click the checkbox to include
    this device in a bulk Get/Set License/Config/Remove from the toolbar."""

    clicked = Signal(str)  # mac
    userToggled = Signal()  # the card's own checkbox was clicked (not set_checked())
    emptySlotRequested = Signal(str)  # mac - "Để trống vị trí này" from the right-click menu

    def __init__(self, parent=None, *, show_position: bool = True):
        super().__init__(parent)
        self.setObjectName("Card")
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFixedSize(CARD_SIZE)
        theme.card_shadow(self, blur=12)  # smaller blur: up to ~100 of these on screen at once
        self.mac: str | None = None
        self._press_pos = None
        self._dragging = False
        # The "Position" chip compares the node's self-reported matrix slot
        # against this card's current grid slot - only meaningful in a
        # Group's own matrix grid (see main_window's Groups page), not on
        # the flat "All Devices" list, which no longer has a single
        # physical layout to compare against (see _refresh_devices_view).
        self._show_position = show_position

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(6)

        top_row = QHBoxLayout()
        top_row.setSpacing(6)
        # Only shown on the first card of a row (see DeviceListView.set_nodes)
        # - ticking it bulk-checks every card in that row in one click, so
        # selecting e.g. 20 boards for a bulk action doesn't mean clicking 20
        # individual checkboxes one at a time.
        self.row_checkbox = QCheckBox()
        self.row_checkbox.setObjectName("RowCheckbox")  # round+purple, see theme.py
        self.row_checkbox.setCursor(Qt.CursorShape.ArrowCursor)
        self.row_checkbox.setToolTip("Chọn/bỏ chọn cả hàng này")
        # Reserve its footprint even hidden (every other card in the row) so
        # every card's own checkbox still lines up at the same x position
        # instead of only row-start cards being shifted right.
        row_checkbox_policy = self.row_checkbox.sizePolicy()
        row_checkbox_policy.setRetainSizeWhenHidden(True)
        self.row_checkbox.setSizePolicy(row_checkbox_policy)
        self.row_checkbox.setVisible(False)
        top_row.addWidget(self.row_checkbox)
        top_row.addSpacing(10)  # breathing room so it doesn't read as a
        # second copy of the checkbox right next to it
        self.checkbox = QCheckBox()
        self.checkbox.setCursor(Qt.CursorShape.ArrowCursor)
        self.checkbox.setToolTip("Chọn để thao tác hàng loạt")
        # set_checked() blocks signals, so this only fires for real clicks.
        self.checkbox.toggled.connect(lambda _checked: self.userToggled.emit())
        top_row.addWidget(self.checkbox)
        top_row.addStretch(1)
        self.status_label = QLabel()
        top_row.addWidget(self.status_label)
        layout.addLayout(top_row)

        # Corner badge, upper-left. Shows the device's alias once one is set
        # (the operator's own board number), otherwise the auto HxCy slot
        # label. See _render_badge().
        self.corner_badge = QLabel("—")
        self.corner_badge.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.corner_badge.setFixedHeight(24)
        self.corner_badge.setMaximumWidth(140)
        self.corner_badge.setStyleSheet(
            f"background:{theme.ORANGE_SOFT}; color:{theme.ORANGE_DARK};"
            f" border:2px solid {theme.ORANGE}; border-radius:12px;"
            f" font-size:13px; font-weight:800; padding:0 10px; min-width:24px;"
        )
        layout.addWidget(self.corner_badge, alignment=Qt.AlignmentFlag.AlignLeft)

        # Pixmap is set per-node in set_node() — which board photo to show
        # (removable-card vs. onboard-card) depends on the device's cardType.
        self.image_label = QLabel()
        self.image_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.image_label)

        self.title_label = QLabel()
        self.title_label.setStyleSheet("font-size:15px; font-weight:800;")
        layout.addWidget(self.title_label)

        # Three highlighted chip rows. "Position" is the matrix position the
        # NODE has stored for itself (via Set Matrix) — green if it matches
        # this card's current grid slot, red if the board is in the wrong
        # place, muted if unset. "License" is time left / time set, green
        # while there's time left, red once expired. "Uptime" is uptime
        # (needs Get Info).
        self._uptime_accent = theme.TILE_COLORS["blue"][1]
        self.saved_pos_label = self._make_chip("#f2efec")  # restyled per-state in _render_saved_pos
        self.license_label = self._make_chip("#e8f8f1")     # restyled per-state in set_node
        self.uptime_label = self._make_chip(theme.TILE_COLORS["blue"][0])
        # Accounts that may not see exact times get a gauge on these two
        # chips instead of the numbers (node["_showTimes"], see set_node).
        if self._show_position:
            layout.addWidget(self.saved_pos_label)
        layout.addWidget(self.license_label)
        layout.addWidget(self.uptime_label)
        self._slot_row = 0
        self._slot_col = 0
        self._node: dict = {}
        # The Group name this card's grid currently represents (set by
        # DeviceListView.set_nodes' expected_group), so the "Position" chip
        # can also flag a board that reports belonging to a *different*
        # Group than the one it's been dragged into — not just a wrong
        # row/col within the right one. None = don't check (flat/ungrouped
        # views, where show_position is False anyway).
        self._expected_group: str | None = None

    @staticmethod
    def _make_chip(bg: str) -> MeterChip:
        chip = MeterChip()
        chip.setTextFormat(Qt.TextFormat.RichText)
        chip.setStyleSheet(
            f"background:{bg}; border-radius:8px; padding:3px 8px; font-size:11px;"
        )
        return chip

    def set_position(self, row: int, col: int) -> None:
        """This card's slot in the grid (1-based). Also re-renders the corner
        badge and the 'Board lưu' chip, both of which depend on the slot."""
        self._slot_row, self._slot_col = row, col
        self._render_badge()
        self._render_saved_pos()

    def _render_badge(self) -> None:
        # Once a device has an alias, that's the operator's own number for
        # the board — show it instead of the auto HxCy slot label. Aliases
        # are only unique *within* a Group though (each Group numbers its
        # own boards independently), so on the flat "All Devices" view
        # (show_position False — see Devices vs. Groups page) two different
        # boards can show the same alias. Suffix with "/<group>" there so
        # they're still distinguishable; not needed on a Group's own grid,
        # where every card is already known to be in the same group.
        alias = (self._node.get("alias") or "").strip()
        if alias and not self._show_position:
            # Prefer the app's own current Group membership over the node's
            # self-reported "nodeGroup" - the latter is only refreshed by a
            # Set Matrix job, so it would still show a Group's old name for
            # a while right after renaming it even though nothing about the
            # device itself changed. Fall back to the self-report only for a
            # device the app doesn't currently have in any Group.
            group = (self._node.get("_currentGroupName") or self._node.get("nodeGroup") or "").strip()
            if group:
                alias = f"{alias}/{group}"
        text = alias or f"H{self._slot_row}C{self._slot_col}"
        elided = self.corner_badge.fontMetrics().elidedText(
            text, Qt.TextElideMode.ElideRight, self.corner_badge.maximumWidth() - 24
        )
        self.corner_badge.setText(elided)
        self.corner_badge.setToolTip(text if elided != text else "")

    def set_expected_group(self, group_name: str | None) -> None:
        self._expected_group = group_name
        self._render_saved_pos()

    def _render_saved_pos(self) -> None:
        if not self._show_position:
            return
        r = self._node.get("nodeRow") or 0
        c = self._node.get("nodeCol") or 0
        reported_group = (self._node.get("nodeGroup") or "").strip()
        group_mismatch = bool(
            self._expected_group and reported_group and reported_group != self._expected_group
        )
        if r <= 0 or c <= 0:
            bg, fg, val = "#f2efec", theme.MUTED, "__"
            tip = "Board chưa lưu vị trí — bấm Set Matrix để ghi xuống."
        elif group_mismatch:
            bg, fg, val = "#ffeded", "#df4545", f"H{r}C{c} ✗"
            tip = (f'Board tự lưu group "{reported_group}" nhưng đang xếp ở group '
                   f'"{self._expected_group}" — có thể xếp nhầm vị trí.')
        elif r == self._slot_row and c == self._slot_col:
            bg, fg, val = "#e8f8f1", "#168a5b", f"H{r}C{c} ✓"
            tip = "Board đang nằm đúng vị trí đã lưu."
        else:
            bg, fg, val = "#ffeded", "#df4545", f"H{r}C{c} ✗"
            tip = (f"Board tự lưu H{r}C{c} nhưng đang xếp ở "
                   f"H{self._slot_row}C{self._slot_col} — có thể xếp nhầm chỗ.")
        self.saved_pos_label.setStyleSheet(
            f"background:{bg}; border-radius:8px; padding:3px 8px; font-size:11px;"
        )
        self.saved_pos_label.setToolTip(tip)
        self.saved_pos_label.setText(
            f'<span style="color:{theme.MUTED};font-weight:700">Position</span>'
            f'&nbsp;&nbsp;<span style="color:{fg};font-weight:800">{val}</span>'
        )

    def set_node(self, node: dict) -> None:
        self._node = node
        self.mac = node.get("mac")
        self.title_label.setText(node.get("uid") or "—")
        pixmap = _scaled_board_pixmap(node.get("cardType") or "R")
        if pixmap is not None:
            self.image_label.setPixmap(pixmap)
        self._render_badge()
        self._render_saved_pos()
        remain = _fmt_mmss(node.get("remain"))
        total = _fmt_mmss(node.get("_licenseTotalMinutes"))
        try:
            has_license = float(node.get("remain")) > 0
        except (TypeError, ValueError):
            has_license = False
        lic_bg, lic_fg = ("#e8f8f1", "#168a5b") if has_license else ("#ffeded", "#df4545")
        self.license_label.setStyleSheet(
            f"background:{lic_bg}; border-radius:8px; padding:3px 8px; font-size:11px;"
        )
        if node.get("_showTimes", True):
            self.license_label.clear_meter()
            self.uptime_label.clear_meter()
            self.license_label.setText(_chip_html("License", f"{remain} / {total}", lic_fg))
            self.uptime_label.setText(
                _chip_html("Uptime", _fmt_uptime(node.get("uptimeMinutes")), self._uptime_accent)
            )
        else:
            fraction = license_fraction(node.get("remain"), node.get("_licenseTotalMinutes"))
            # License is a 5-step "Status" gauge with no minutes; uptime stays
            # an exact reading for everyone.
            self.license_label.set_caption("Status", theme.MUTED)
            self.license_label.set_meter(
                theme.countdown_color(fraction * 100),
                level=license_level(node.get("remain"), node.get("_licenseTotalMinutes")),
            )
            self.uptime_label.clear_meter()
            self.uptime_label.setText(
                _chip_html("Uptime", _fmt_uptime(node.get("uptimeMinutes")), self._uptime_accent)
            )
        state = theme.effective_response_state(node)
        color, bg = theme.STATUS_COLORS.get(state, theme.STATUS_COLORS["UNKNOWN"])
        # Offline devices stay in their grid slot; only this pill turns red.
        is_offline = state in ("NO_RESPONSE", "OFFLINE")
        label = "OFFLINE" if is_offline else state
        self.status_label.setText(label)
        self.status_label.setStyleSheet(
            f"color:{color}; background:{bg}; border-radius:9px; padding:3px 9px;"
            f"font-size:10px; font-weight:800;"
        )
        # Whole-card highlight so an offline board jumps out of the grid
        # instead of only the small status pill changing color.
        if is_offline:
            self.setStyleSheet(
                f"#Card {{ background:{theme.STATUS_COLORS['OFFLINE'][1]};"
                f" border:2px solid {theme.RED}; border-radius:18px; }}"
            )
        else:
            self.setStyleSheet("")

    def set_checked(self, checked: bool) -> None:
        self.checkbox.blockSignals(True)
        self.checkbox.setChecked(checked)
        self.checkbox.blockSignals(False)

    def is_checked(self) -> bool:
        return self.checkbox.isChecked()

    def set_row_checkbox_visible(self, visible: bool) -> None:
        self.row_checkbox.setVisible(visible)

    def mousePressEvent(self, event) -> None:  # noqa: N802 (Qt override)
        # Don't open the detail dialog on press: that's when a drag gesture
        # also begins (see DeviceListView), and popping a modal dialog right
        # then steals the mouse before Qt can ever recognize a drag. Only
        # decide "click vs. drag" once the button comes back up.
        if event.button() == Qt.MouseButton.LeftButton:
            self._press_pos = event.position().toPoint()
            self._dragging = False
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event) -> None:  # noqa: N802 (Qt override)
        if (
            self._press_pos is not None
            and not self._dragging
            and self.mac
            and event.buttons() & Qt.MouseButton.LeftButton
            and (event.position().toPoint() - self._press_pos).manhattanLength()
            > QApplication.startDragDistance()
        ):
            self._dragging = True
            self._start_drag()
            return
        super().mouseMoveEvent(event)

    def _start_drag(self) -> None:
        # A QListWidget's built-in InternalMove drag only engages when the
        # VIEW itself sees the press+move — but a widget set via
        # setItemWidget() sits on top and swallows those events first, so
        # the view never notices. Starting the QDrag directly from here,
        # carrying just the mac string, sidesteps that entirely: the view
        # only needs to accept the drop (see DeviceListView.dropEvent).
        drag = QDrag(self)
        mime = QMimeData()
        mime.setData(DEVICE_MAC_MIME_TYPE, self.mac.encode("utf-8"))
        mime.setText(self.mac)
        drag.setMimeData(mime)
        preview = self.grab().scaled(
            self.width() // 2, self.height() // 2,
            Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation,
        )
        drag.setPixmap(preview)
        drag.setHotSpot(preview.rect().center())
        drag.exec(Qt.DropAction.MoveAction)
        self._press_pos = None
        self._dragging = False

    def mouseReleaseEvent(self, event) -> None:  # noqa: N802 (Qt override)
        if event.button() == Qt.MouseButton.LeftButton and self._press_pos is not None and not self._dragging and self.mac:
            self.clicked.emit(self.mac)
        self._press_pos = None
        self._dragging = False
        super().mouseReleaseEvent(event)

    def contextMenuEvent(self, event) -> None:  # noqa: N802 (Qt override)
        # Lets the operator carve gaps into the grid to match a physical
        # layout that isn't a plain rectangle (a shape with corners cut,
        # a ring, ...) - see DeviceListView/EmptySlotCard for the other
        # half of this (dropping a card onto an emptied slot, or removing
        # one). Vacating just moves this card to the end of the grid, same
        # place a plain drag-past-the-last-card already lands it, and
        # leaves the slot it was in as a hole the operator can later drag
        # any other card into.
        if not self.mac:
            return
        menu = theme.SimpleMenu(self)
        menu.add_action("Để trống vị trí này", lambda: self.emptySlotRequested.emit(self.mac))
        menu.popup(event.globalPos())


class EmptySlotCard(QFrame):
    """A deliberately-vacated grid slot (see DeviceCard.emptySlotRequested)
    - same footprint as DeviceCard so it lines up in the grid, but carries
    no device and isn't draggable itself. It only exists as a drop target:
    dragging a real DeviceCard onto it (DeviceListView.dropEvent, the same
    swap path as swapping two real cards) moves that card here and leaves
    its old slot empty instead. Lets a Group's matrix mirror a physical
    layout that isn't a plain filled rectangle (corners cut, a ring, ...)."""

    removeRequested = Signal()  # right-click -> "Bỏ ô trống này"

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("EmptySlot")
        self.setFixedSize(CARD_SIZE)
        # An explicit light background, not "transparent" - a QFrame with no
        # background painted falls through to the OS/Qt style's own default
        # (often dark), which made this card render as a near-black box with
        # low-contrast text on top instead of the app's light theme.
        self.setStyleSheet(
            f"#EmptySlot {{ background:{theme.EMPTY_SLOT_BG}; border:2px dashed {theme.EMPTY_SLOT_BORDER}; border-radius:18px; }}"
        )
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        self._position_label = QLabel("—")
        self._position_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._position_label.setStyleSheet(f"color:{theme.INK}; font-size:13px; font-weight:700;")
        layout.addStretch(1)
        label = QLabel("Ô trống", alignment=Qt.AlignmentFlag.AlignCenter)
        label.setStyleSheet(f"color:{theme.INK}; font-weight:600;")
        layout.addWidget(label)
        layout.addWidget(self._position_label)
        layout.addStretch(1)

    def set_position(self, row: int, col: int) -> None:
        self._position_label.setText(f"H{row}C{col}")

    def contextMenuEvent(self, event) -> None:  # noqa: N802 (Qt override)
        menu = theme.SimpleMenu(self)
        menu.add_action("Bỏ ô trống này (dồn các ô sau lên)", self.removeRequested.emit)
        menu.popup(event.globalPos())


class DeviceListView(QListWidget):
    """A fixed-column grid of DeviceCard tiles the operator can drag to
    reorder, to mirror the physical panel layout (e.g. a 5-column rack).

    Drag-and-drop is fully manual rather than QListWidget's built-in
    InternalMove: each DeviceCard starts its own QDrag (see
    DeviceCard._start_drag) carrying just its mac, because a widget set via
    setItemWidget() sits on top of the view and swallows the press/move
    events InternalMove would need to see to engage on its own. This view's
    job is only to accept that drop and work out where it landed.
    """

    cardClicked = Signal(str)  # mac, forwarded from whichever DeviceCard was clicked
    reordered = Signal(list)  # new mac order (None = empty slot), emitted right after a drop
    selectionChanged = Signal()  # a card checkbox or row checkbox was clicked
    emptySlotRequested = Signal(str)  # mac, forwarded from DeviceCard's right-click menu
    emptySlotRemoveRequested = Signal(int)  # index of the empty slot to drop, forwarded from EmptySlotCard

    def __init__(self, columns: int = 5, parent=None, *, show_position: bool = True,
                max_visible_rows: int | None = None):
        super().__init__(parent)
        self._show_position = show_position
        # None = grow to fit every row (Devices page — already sits inside
        # its own outer QScrollArea, so an ever-taller grid just makes that
        # page longer, which is fine). A number caps the grid's own height
        # to that many rows and scrolls internally past it instead — used
        # on the Groups page, which is NOT in an outer scroll area, so an
        # unbounded grid there would otherwise crush the member table below
        # it down to a sliver once a group has more than a couple boards.
        self._max_visible_rows = max_visible_rows
        self.setViewMode(QListView.ViewMode.IconMode)
        self.setFlow(QListView.Flow.LeftToRight)
        self.setWrapping(True)
        self.setResizeMode(QListView.ResizeMode.Adjust)
        self.setMovement(QListView.Movement.Snap)
        self.setSelectionMode(QAbstractItemView.SelectionMode.NoSelection)
        self.setAcceptDrops(True)
        self.setDragDropMode(QAbstractItemView.DragDropMode.DropOnly)
        self.setUniformItemSizes(True)
        self.setSpacing(6)
        self.setFrameShape(QFrame.Shape.NoFrame)
        self.setContentsMargins(0, 0, 0, 0)  # NoFrame alone still reserves ~4px, dropping a column
        self.setStyleSheet("background: transparent; border: none;")
        vscroll = Qt.ScrollBarPolicy.ScrollBarAsNeeded if max_visible_rows else Qt.ScrollBarPolicy.ScrollBarAlwaysOff
        self.setVerticalScrollBarPolicy(vscroll)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        cell = CARD_SIZE + QSize(2 * self.spacing(), 2 * self.spacing())
        self.setGridSize(cell)
        self._columns = 1
        self.set_columns(columns)

    def set_columns(self, columns: int) -> None:
        self._columns = max(1, columns)
        cell_w = self.gridSize().width()
        # +24 covers the view's own frame/content margins (observed ~4px a
        # side even with NoFrame + contentsMargins(0,0,0,0), apparently
        # reinstated by the stylesheet's "border: none") plus a little slack
        # so a last column doesn't wrap from landing exactly on the boundary.
        self.setFixedWidth(self._columns * cell_w + 24)
        self._recompute_height()

    def set_nodes(
        self, nodes: list[dict | None], *,
        expected_group: str | None = None, show_position: bool | None = None,
    ) -> None:
        # show_position overrides this view's own default per-call (see
        # Devices page: the one grid there toggles it depending on whether
        # the "Group" filter is "Tất cả thiết bị" or a specific Group).
        # `None` in `nodes` is a deliberately-vacated slot (see
        # DeviceCard.emptySlotRequested/EmptySlotCard) - rendered as an
        # EmptySlotCard instead of a DeviceCard, with no mac of its own.
        effective_show_position = self._show_position if show_position is None else show_position
        self.clear()
        for i, node in enumerate(nodes):
            item = QListWidgetItem()
            item.setSizeHint(self.gridSize() - QSize(2 * self.spacing(), 2 * self.spacing()))
            row, col = i // self._columns + 1, i % self._columns + 1
            if node is None:
                item.setData(Qt.ItemDataRole.UserRole, None)
                self.addItem(item)
                empty = EmptySlotCard()
                empty.set_position(row, col)
                empty.removeRequested.connect(lambda idx=i: self.emptySlotRemoveRequested.emit(idx))
                self.setItemWidget(item, empty)
                continue
            item.setData(Qt.ItemDataRole.UserRole, node.get("mac"))
            self.addItem(item)
            card = DeviceCard(show_position=effective_show_position)
            card.set_node(node)
            card.set_expected_group(expected_group)
            # Row-major position in this fixed-column grid → HxCy badge.
            card.set_position(row, col)
            card.clicked.connect(self.cardClicked)
            card.userToggled.connect(self.selectionChanged)
            card.emptySlotRequested.connect(self.emptySlotRequested)
            if i % self._columns == 0:
                # First card of this row - show its row checkbox and wire it
                # to bulk-check every card in the row (see set_row_checked).
                card.set_row_checkbox_visible(True)
                card.row_checkbox.toggled.connect(
                    lambda checked, r=row: self.set_row_checked(r, checked)
                )
            self.setItemWidget(item, card)
        self._recompute_height()

    def set_row_checked(self, row: int, checked: bool) -> None:
        """Bulk-checks/unchecks every card in `row` (1-based) - the action
        behind a row checkbox (see set_nodes), so selecting e.g. 20 boards
        for a bulk action doesn't mean clicking 20 individual checkboxes."""
        start = (row - 1) * self._columns
        end = min(start + self._columns, self.count())
        for i in range(start, end):
            widget = self.itemWidget(self.item(i))
            if isinstance(widget, DeviceCard):
                widget.set_checked(checked)
        self.selectionChanged.emit()

    def sync_row_checkboxes(self) -> None:
        """Re-derives every row checkbox's own visual state from whether
        every card in that row is currently checked. Call this after
        restoring each individual card's checked state post-rebuild (a live
        device.upsert, the periodic auto-refresh timer, ...) - set_nodes()
        always creates fresh (unchecked-looking) row checkboxes, so without
        this a row you'd selected looks like it "reset" a few seconds later
        even though the boards under it are still actually checked."""
        for start in range(0, self.count(), self._columns):
            row_card = self.itemWidget(self.item(start))
            if not isinstance(row_card, DeviceCard):
                continue
            end = min(start + self._columns, self.count())
            all_checked = True
            for i in range(start, end):
                widget = self.itemWidget(self.item(i))
                if not (isinstance(widget, DeviceCard) and widget.is_checked()):
                    all_checked = False
                    break
            row_card.row_checkbox.blockSignals(True)
            row_card.row_checkbox.setChecked(all_checked)
            row_card.row_checkbox.blockSignals(False)

    def card_for(self, mac: str) -> DeviceCard | None:
        for i in range(self.count()):
            item = self.item(i)
            if item.data(Qt.ItemDataRole.UserRole) == mac:
                widget = self.itemWidget(item)
                return widget if isinstance(widget, DeviceCard) else None
        return None

    def _recompute_height(self) -> None:
        rows = max(1, -(-self.count() // self._columns))  # ceil div
        if self._max_visible_rows:
            rows = min(rows, self._max_visible_rows)
        self.setFixedHeight(rows * self.gridSize().height() + 4)

    def dragEnterEvent(self, event) -> None:  # noqa: N802 (Qt override)
        if event.mimeData().hasFormat(DEVICE_MAC_MIME_TYPE):
            event.acceptProposedAction()
        else:
            super().dragEnterEvent(event)

    def dragMoveEvent(self, event) -> None:  # noqa: N802 (Qt override)
        if event.mimeData().hasFormat(DEVICE_MAC_MIME_TYPE):
            event.acceptProposedAction()
        else:
            super().dragMoveEvent(event)

    def dropEvent(self, event) -> None:  # noqa: N802 (Qt override)
        mime = event.mimeData()
        if not mime.hasFormat(DEVICE_MAC_MIME_TYPE):
            super().dropEvent(event)
            return
        dragged_mac = bytes(mime.data(DEVICE_MAC_MIME_TYPE)).decode("utf-8")
        order = [self.item(i).data(Qt.ItemDataRole.UserRole) for i in range(self.count())]
        # Look up the dragged card's OWN row directly rather than
        # order.index(dragged_mac) - macs are unique so that would still
        # work today, but resolving both sides the same way (by row) keeps
        # this correct even once empty slots (UserRole=None, and there can
        # be several of them) are mixed in below.
        dragged_row = next((i for i, mac in enumerate(order) if mac == dragged_mac), None)
        if dragged_row is None:
            event.ignore()
            return
        pos = event.position().toPoint() if hasattr(event, "position") else event.pos()
        target_item = self.itemAt(pos)
        if target_item is None:
            # Dropped past the last card (empty space) - no slot to swap
            # with, so just move it to the end.
            order.pop(dragged_row)
            order.append(dragged_mac)
        else:
            # Swap the two slots in place instead of removing+reinserting -
            # the latter shifted every card between the old and new position
            # by one, scrambling a layout that only meant to move 2 boards.
            # self.row() finds the target by its actual position, not by
            # searching for a value - with several empty slots (all
            # UserRole=None) a value search would always resolve to the
            # first one regardless of which one was actually dropped on.
            target_row = self.row(target_item)
            if target_row != dragged_row:
                order[dragged_row], order[target_row] = order[target_row], order[dragged_row]
        event.acceptProposedAction()
        self.reordered.emit(order)
