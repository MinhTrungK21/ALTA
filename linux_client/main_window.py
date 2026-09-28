from __future__ import annotations

import json
import time
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QButtonGroup,
    QComboBox,
    QDialog,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QHeaderView,
    QInputDialog,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QSplitter,
    QStackedWidget,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

import theme
from meters import MeterBar, freshness_color, freshness_level, license_fraction, license_level
from device_card import DeviceCard, DeviceListView
from dialogs import (
    CardTypeDialog,
    ChangePasswordDialog,
    ConfigDeviceDialog,
    CreateAccountDialog,
    DeviceDetailDialog,
    GroupEditDialog,
    LicenseSetDialog,
    LoginDialog,
    TwoFactorSetupDialog,
    mac_to_decimal,
)
from hub_client import DEFAULT_PORT, HubClient
from serial_client import DEFAULT_BAUD_RATE, SerialHubClient, list_serial_ports

DEVICE_COLUMNS = [
    "STT", "UID", "Alias", "Device ID", "MAC", "LID", "Remain",
    "Status", "State", "NOD", "Last seen (s)",
]
ACCOUNT_COLUMNS = ["Tài khoản", "2FA", ""]
STT_COLUMN = DEVICE_COLUMNS.index("STT")
UID_COLUMN = DEVICE_COLUMNS.index("UID")
MAC_COLUMN = DEVICE_COLUMNS.index("MAC")
STATE_COLUMN = DEVICE_COLUMNS.index("State")
LOGO_PATH = Path(__file__).resolve().parent.parent / "logo" / "twt_logo.png"
DEFAULT_DEVICE_COLUMNS = 5
# Only this account sees exact times (license minutes, uptime, last-seen
# seconds, "Set License" timestamp); every other account gets bars/gauges
# (see meters.py). Display-only: the Hub still sends the full numbers to the
# app, this just decides how the app draws them.
ADMIN_USERNAME = "admin"
REMAIN_COLUMN = DEVICE_COLUMNS.index("Remain")
LAST_SEEN_COLUMN = DEVICE_COLUMNS.index("Last seen (s)")
DEVICE_ORDER_PATH = Path.home() / ".config" / "hub66s_control" / "device_order.json"
LICENSE_TOTALS_PATH = Path.home() / ".config" / "hub66s_control" / "license_totals.json"
LICENSE_SET_AT_PATH = Path.home() / ".config" / "hub66s_control" / "license_set_at.json"
LAST_EXPIRED_LED_SETTINGS_PATH = Path.home() / ".config" / "hub66s_control" / "last_expired_led_settings.json"
# How often the timer ticks, alternating Refresh (Online/Offline) and Get
# Info (Uptime/firmware/voltage/temp) each tick — so each one individually
# lands every 2*AUTO_REFRESH_INTERVAL_MS, not every tick.
IDENTIFY_SECONDS = 300           # how long a node keeps blinking without a renewal
IDENTIFY_RENEW_MS = 2 * 60 * 1000  # < IDENTIFY_SECONDS, so ticked boards never stop mid-selection
AUTO_REFRESH_INTERVAL_MS = 1 * 60 * 1000


ALL_DEVICES_SCOPE = "all"  # the flat, ungrouped matrix - every known device


def _load_layouts() -> dict[str, dict]:
    """Per-scope saved grid arrangement: {scope_key: {"grid": [mac,...],
    "columns": int}}. scope_key is ALL_DEVICES_SCOPE for the flat matrix, or
    a Group id (as str) so each physical location/rack — a Group's members —
    keeps its own independent HxCy layout and column count (see Devices
    page's location dropdown). Falls back to migrating whatever older format
    is on disk (single flat "grid" list, or the even older two-section
    {online, offline} shape) into ALL_DEVICES_SCOPE."""
    try:
        data = json.loads(DEVICE_ORDER_PATH.read_text())
    except (OSError, ValueError):
        return {}
    if not isinstance(data, dict):
        return {}
    if "scopes" in data:
        scopes: dict[str, dict] = {}
        for key, value in (data.get("scopes") or {}).items():
            if not isinstance(value, dict):
                continue
            grid = value.get("grid")
            if not isinstance(grid, list):
                continue
            columns = value.get("columns")
            # None = a deliberately-vacated slot (see
            # DeviceCard.emptySlotRequested/EmptySlotCard), kept as-is;
            # anything else that isn't a real mac string is dropped.
            scopes[str(key)] = {
                "grid": [m for m in grid if m is None or isinstance(m, str)],
                "columns": columns if isinstance(columns, int) and columns > 0 else DEFAULT_DEVICE_COLUMNS,
            }
        return scopes
    if "grid" in data:
        grid = list(data.get("grid", []))
    else:
        merged = list(data.get("online", [])) + list(data.get("offline", []))
        grid = list(dict.fromkeys(merged))  # de-dupe, keep order
    return {ALL_DEVICES_SCOPE: {"grid": grid, "columns": DEFAULT_DEVICE_COLUMNS}}


def _save_layouts(layouts: dict[str, dict]) -> None:
    try:
        DEVICE_ORDER_PATH.parent.mkdir(parents=True, exist_ok=True)
        DEVICE_ORDER_PATH.write_text(json.dumps({"scopes": layouts}))
    except OSError:
        pass


def _load_license_totals() -> dict[str, int]:
    """mac -> the durationMinutes this app last *set* for that device. The
    protocol only ever reports what license time is left (`remain`), never
    the original duration, so DeviceDetailDialog's countdown bar has nothing
    to measure "% used" against unless this app remembers what it asked
    for. Only covers devices actually Set from this app (persisted, so it
    survives restarts) — devices never Set here just show remain as-is."""
    try:
        data = json.loads(LICENSE_TOTALS_PATH.read_text())
        return {str(k): int(v) for k, v in data.items()}
    except (OSError, ValueError):
        return {}


def _save_license_totals(totals: dict[str, int]) -> None:
    try:
        LICENSE_TOTALS_PATH.parent.mkdir(parents=True, exist_ok=True)
        LICENSE_TOTALS_PATH.write_text(json.dumps(totals))
    except OSError:
        pass


def _load_license_set_at() -> dict[str, str]:
    """mac -> "YYYY-MM-DD HH:MM:SS" in this machine's local time, the moment
    this app last sent a Set License command for that device. The protocol
    has no concept of a server clock (a node only knows its own uptime), so
    this is purely an app-side record of when *we* issued the command."""
    try:
        data = json.loads(LICENSE_SET_AT_PATH.read_text())
        return {str(k): str(v) for k, v in data.items()}
    except (OSError, ValueError):
        return {}


def _save_license_set_at(set_at: dict[str, str]) -> None:
    try:
        LICENSE_SET_AT_PATH.parent.mkdir(parents=True, exist_ok=True)
        LICENSE_SET_AT_PATH.write_text(json.dumps(set_at))
    except OSError:
        pass


def _load_last_expired_led_settings() -> dict:
    """The expiredLedMode/cycle values the operator picked the last time they
    hit OK on Set License, anywhere in the app - LicenseSetDialog defaults to
    these instead of always resetting to "Ngẫu nhiên" on every open. Unlike
    license_totals/license_set_at this isn't per-mac: it's one shared "last
    choice", since duration/LID always have to be re-entered anyway but the
    LED mode is easy to forget to re-pick and silently revert by omission."""
    try:
        data = json.loads(LAST_EXPIRED_LED_SETTINGS_PATH.read_text())
        return {
            "mode": int(data.get("mode", 0)),
            "cycleMin": int(data.get("cycleMin", 60)),
            "cycleMax": int(data.get("cycleMax", 630)),
        }
    except (OSError, ValueError):
        return {"mode": 0, "cycleMin": 60, "cycleMax": 630}


def _save_last_expired_led_settings(mode: int, cycle_min: int, cycle_max: int) -> None:
    try:
        LAST_EXPIRED_LED_SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
        LAST_EXPIRED_LED_SETTINGS_PATH.write_text(json.dumps({"mode": mode, "cycleMin": cycle_min, "cycleMax": cycle_max}))
    except OSError:
        pass


def node_label(node: dict) -> str:
    alias = (node.get("alias") or "").strip()
    uid = node.get("uid") or node.get("id_src") or ""
    mac_display = mac_to_decimal(node.get("mac", ""))
    base = alias if alias else uid
    return f"{base} ({mac_display})" if base else mac_display


def _centered(widget: QWidget) -> QWidget:
    wrapper = QWidget()
    layout = QHBoxLayout(wrapper)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.addWidget(widget, alignment=Qt.AlignmentFlag.AlignCenter)
    return wrapper


def _configure_table_columns(
    table: QTableWidget, stretch_column: int | None, fixed_widths: dict[int, int] | None = None
) -> None:
    header = table.horizontalHeader()
    fixed_widths = fixed_widths or {}
    # stretch_column=None: every column sizes to its own content instead of
    # one column being force-stretched to soak up the rest of the table's
    # width — for short, fixed-shape data (aliases, MACs, counters...)
    # stretching just one column leaves a big awkward internal gap around
    # its tiny content instead of one plain strip past the last column.
    header.setStretchLastSection(False)
    for col in range(table.columnCount()):
        if col in fixed_widths:
            header.setSectionResizeMode(col, QHeaderView.ResizeMode.Fixed)
            table.setColumnWidth(col, fixed_widths[col])
        elif col == stretch_column:
            header.setSectionResizeMode(col, QHeaderView.ResizeMode.Stretch)
        else:
            header.setSectionResizeMode(col, QHeaderView.ResizeMode.ResizeToContents)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("TWT Led Control System")
        self.resize(1220, 780)

        # Two interchangeable transports for the exact same JSON protocol
        # (see hub_client.py / serial_client.py); `client` below always
        # resolves to whichever one is currently selected/active.
        self._active_is_usb = False
        self.ws_client = HubClient(self)
        self.serial_client = SerialHubClient(self)
        for transport in (self.ws_client, self.serial_client):
            transport.connected.connect(self._on_connected)
            transport.disconnected.connect(self._on_disconnected)
            transport.socketError.connect(self._on_socket_error)
            transport.message.connect(self._on_message)

        self.username: str | None = None
        self.devices: dict[str, dict] = {}
        self.device_cards: dict[str, DeviceCard] = {}
        # Per-location matrix layouts (see _load_layouts): one saved
        # {grid, columns} per Group id, plus ALL_DEVICES_SCOPE for the flat
        # "Tất cả thiết bị" view. self.current_scope is whichever of those
        # the Devices page's own "Group" filter is currently showing (see
        # _set_grid_scope) — self.grid_order/self.columns_spin/self.grid_list
        # always reflect that one scope. Group *creation/editing* itself
        # stays on the separate, plain Groups page.
        self.layouts: dict[str, dict] = _load_layouts()
        self.current_scope: str = ALL_DEVICES_SCOPE
        self.grid_order: list[str | None] = list(
            self.layouts.get(ALL_DEVICES_SCOPE, {}).get("grid", [])
        )
        # Every node ever seen this session, keyed by mac — kept so a device
        # that a re-Scan drops from `devices` still renders in its grid slot
        # as OFFLINE instead of vanishing. Pruned only on explicit Remove or
        # Clear.
        self.last_known: dict[str, dict] = {}
        self._pending_layout_reset = False
        self.license_totals: dict[str, int] = _load_license_totals()
        self.license_set_at: dict[str, str] = _load_license_set_at()
        self.groups: list[dict] = []
        # False until the Hub's real Group list has actually arrived once
        # (state/groups.changed) — guards _prune_orphan_layouts() against
        # the very first _refresh_grid_scope_combo() call during UI
        # construction, before self.groups is anything but the empty
        # placeholder above. Without this, that call would see "no Groups
        # exist yet" and delete every saved Group layout in
        # device_order.json before the real list ever loads.
        self._groups_loaded = False
        self.accounts: list[dict] = []
        self.busy = False
        self._last_job_success = 0
        self._last_job_total = 0
        self._hub_subtitle_text = "Điểm truy cập nội bộ"
        self._login_dialog: LoginDialog | None = None
        self._twofa_dialog: TwoFactorSetupDialog | None = None
        self._open_detail_mac: str | None = None
        self._open_detail_dialog: DeviceDetailDialog | None = None
        self._login_timeout = QTimer(self)
        self._login_timeout.setSingleShot(True)
        self._login_timeout.timeout.connect(self._on_login_timeout)
        self._usb_probe_timer = QTimer(self)
        self._usb_probe_timer.timeout.connect(self._probe_usb_auth)
        # Online/Offline is only ever as fresh as the last time something
        # actually queried each board (see theme.effective_response_state) -
        # without this, a board that went silent keeps showing whatever it
        # last reported until an operator remembers to hit Refresh. Auto-
        # firing the same safe, sequential, self-retrying Refresh job on a
        # timer means that happens without anyone having to ask.
        # Boards ticked on the Devices page blink their LED 46 fast so the
        # operator can find them physically (device.identify on the Hub).
        # _identify_active = MACs the Hub was last told to blink; the node
        # stops by itself after IDENTIFY_SECONDS, so the renew timer re-arms
        # them while they stay ticked, and an app crash/disconnect leaves at
        # most a few minutes of blinking.
        self._identify_active: set[str] = set()
        self._identify_debounce = QTimer(self)
        self._identify_debounce.setSingleShot(True)
        self._identify_debounce.setInterval(150)
        self._identify_debounce.timeout.connect(self._sync_identify)
        self._identify_renew_timer = QTimer(self)
        self._identify_renew_timer.setInterval(IDENTIFY_RENEW_MS)
        self._identify_renew_timer.timeout.connect(self._renew_identify)
        self._identify_renew_timer.start()
        self._auto_refresh_timer = QTimer(self)
        self._auto_refresh_timer.setInterval(AUTO_REFRESH_INTERVAL_MS)
        self._auto_refresh_timer.timeout.connect(self._on_auto_refresh_timeout)
        # Alternates Refresh (license/remain + Online/Offline) and Get Info
        # (firmware/voltage/temperature/uptime) on successive ticks instead
        # of firing both every tick - same end result (both stay roughly
        # fresh within one full Refresh/Info cycle each) without doubling
        # every single tick's ESP-NOW traffic on top of itself.
        self._auto_refresh_next_is_info = False

        self._build_ui()
        self._set_signed_in(False)

    @property
    def client(self):
        return self.serial_client if self._active_is_usb else self.ws_client

    def _send(self, action: str, data: dict | None = None, *, log: bool = True) -> None:
        if not self.client.is_connected():
            self.log(f"Bỏ qua '{action}': socket không kết nối")
            # This used to be silent beyond that one Activity Log line — easy
            # to miss (e.g. the Activity Log lives on the Dashboard page,
            # not wherever the action was triggered from), which meant an
            # action fired while disconnected (a Wi-Fi hiccup, the hub
            # rebooting mid-session, USB unplugged) looked exactly like
            # nothing happened at all: e.g. "New Group" → fill it in → OK →
            # the dialog just closes and no group ever appears. A popup
            # makes that failure impossible to miss instead of looking
            # identical to a silent bug.
            QMessageBox.warning(
                self, "Chưa kết nối",
                f"Không thể thực hiện '{action}': chưa kết nối tới HUB.\n"
                "Kiểm tra kết nối WiFi/USB rồi thử lại.",
            )
            return
        if log:
            self.log(f">> {action} {data or {}}")
        self.client.send(action, data)

    # ------------------------------------------------------------------ UI
    def _build_ui(self) -> None:
        central = QWidget()
        central.setObjectName("Root")
        self.setCentralWidget(central)
        root = QHBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)
        root.addWidget(self._build_sidebar())
        root.addWidget(self._build_main_area(), 1)

    def _build_sidebar(self) -> QWidget:
        sidebar = QFrame()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(230)
        layout = QVBoxLayout(sidebar)
        layout.setContentsMargins(18, 24, 18, 18)
        layout.setSpacing(6)

        brand_col = QVBoxLayout()
        brand_col.setSpacing(6)
        if LOGO_PATH.exists():
            pixmap = QPixmap(str(LOGO_PATH)).scaledToHeight(40, Qt.TransformationMode.SmoothTransformation)
            logo_label = QLabel()
            logo_label.setPixmap(pixmap)
            brand_col.addWidget(logo_label)
        brand_label = QLabel("TWT Led Control System")
        brand_label.setObjectName("Brand")
        brand_col.addWidget(brand_label)
        layout.addLayout(brand_col)
        layout.addSpacing(18)

        self.nav_group = QButtonGroup(self)
        self.nav_group.setExclusive(True)
        self.nav_buttons: dict[str, QPushButton] = {}
        for key, text in (
            ("dashboard", "Dashboard"),
            ("devices", "Devices"),
            ("groups", "Groups"),
            ("accounts", "Accounts"),
        ):
            button = QPushButton(text)
            button.setObjectName("NavButton")
            button.setCheckable(True)
            button.setCursor(Qt.CursorShape.PointingHandCursor)
            button.clicked.connect(lambda _checked, k=key: self._on_nav_clicked(k))
            self.nav_group.addButton(button)
            self.nav_buttons[key] = button
            layout.addWidget(button)
        self.nav_buttons["dashboard"].setChecked(True)

        layout.addStretch(1)

        settings_btn = QPushButton("Settings")
        settings_btn.setObjectName("FootButton")
        settings_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        settings_btn.clicked.connect(self._open_connection_settings)
        layout.addWidget(settings_btn)

        signout_btn = QPushButton("Sign Out")
        signout_btn.setObjectName("FootButton")
        signout_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        signout_btn.clicked.connect(self._action_sign_out)
        layout.addWidget(signout_btn)

        return sidebar

    def _build_main_area(self) -> QWidget:
        wrapper = QWidget()
        layout = QVBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._build_topbar())

        self.stack = QStackedWidget()
        self.dashboard_page = self._build_dashboard_page()
        self.devices_page = self._build_devices_page()
        self.groups_page = self._build_groups_page()
        self.accounts_page = self._build_accounts_page()
        for page in (self.dashboard_page, self.devices_page, self.groups_page, self.accounts_page):
            self.stack.addWidget(page)
        layout.addWidget(self.stack, 1)
        # Populate the Devices-page "Group" filter with at least "Tất cả
        # thiết bị" before any server state arrives (self.groups is [] here).
        self._refresh_grid_scope_combo()
        return wrapper

    def _build_topbar(self) -> QWidget:
        bar = QWidget()
        bar.setObjectName("Topbar")
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(30, 24, 30, 12)
        layout.setSpacing(16)

        title_box = QVBoxLayout()
        title_box.setSpacing(4)
        self.page_title_label = QLabel("CONTROLS SYSTEM")
        self.page_title_label.setObjectName("PageTitle")
        self.page_subtitle_label = QLabel(self._hub_subtitle_text)
        self.page_subtitle_label.setObjectName("PageSubtitle")
        title_box.addWidget(self.page_title_label)
        title_box.addWidget(self.page_subtitle_label)
        layout.addLayout(title_box)
        layout.addStretch(1)

        self.transport_combo = QComboBox()
        # USB first (and selected by default) — it's the connection actually
        # used most, WiFi is the fallback.
        self.transport_combo.addItems(["USB", "WiFi"])
        self.transport_combo.currentTextChanged.connect(self._on_transport_changed)
        layout.addWidget(self.transport_combo)

        self.host_edit = QLineEdit("192.168.4.1")
        self.host_edit.setFixedWidth(130)
        self.host_edit.setVisible(False)
        layout.addWidget(self.host_edit)

        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)  # allow typing a path Qt didn't enumerate
        self.port_combo.setFixedWidth(130)
        layout.addWidget(self.port_combo)
        self.port_refresh_btn = QPushButton("⟳")
        self.port_refresh_btn.setFixedWidth(32)
        self.port_refresh_btn.setToolTip("Dò lại danh sách cổng USB")
        self.port_refresh_btn.clicked.connect(self._refresh_serial_ports)
        layout.addWidget(self.port_refresh_btn)
        self._refresh_serial_ports()

        self.connect_btn = QPushButton("Kết nối")
        self.connect_btn.setProperty("variant", "primary")
        self.connect_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        self.connect_btn.clicked.connect(self._toggle_connection)
        layout.addWidget(self.connect_btn)

        pill = QFrame()
        pill.setObjectName("StatusPill")
        pill_layout = QHBoxLayout(pill)
        pill_layout.setContentsMargins(12, 7, 14, 7)
        pill_layout.setSpacing(8)
        self.status_dot = QFrame()
        self.status_dot.setObjectName("StatusDot")
        self.status_dot.setFrameShape(QFrame.Shape.NoFrame)
        self.status_dot.setFixedSize(10, 10)
        self.status_dot.setProperty("connected", "false")
        self.status_text = QLabel("Chưa kết nối")
        pill_layout.addWidget(self.status_dot)
        pill_layout.addWidget(self.status_text)
        layout.addWidget(pill)
        return bar

    def _make_card(self, title: str, subtitle: str = "", header_widget: QWidget | None = None):
        card = QFrame()
        card.setObjectName("Card")
        theme.card_shadow(card)
        outer = QVBoxLayout(card)
        outer.setContentsMargins(22, 20, 22, 20)
        outer.setSpacing(14)

        head = QHBoxLayout()
        text_box = QVBoxLayout()
        text_box.setSpacing(4)
        title_label = QLabel(title)
        title_label.setObjectName("CardTitle")
        text_box.addWidget(title_label)
        subtitle_label = QLabel(subtitle)
        subtitle_label.setObjectName("CardSubtitle")
        subtitle_label.setVisible(bool(subtitle))
        text_box.addWidget(subtitle_label)
        head.addLayout(text_box)
        head.addStretch(1)
        if header_widget is not None:
            head.addWidget(header_widget)
        outer.addLayout(head)

        body = QVBoxLayout()
        body.setSpacing(12)
        # Explicit stretch=1 (not the addLayout() default of 0) so that any
        # leftover height this card is given (it's usually added to its page
        # with its own stretch factor, filling the viewport) always lands
        # inside body, never split into the gap between head and body. Once
        # this is the only thing above 0, whatever inside body should absorb
        # that leftover (a stretchy table/splitter, or else a trailing
        # body.addStretch(1) after fixed-height content — see e.g.
        # _build_devices_page) claims it; anything left over pins to the top
        # instead of "jumping" apart across head/body.
        outer.addLayout(body, 1)
        return card, body, subtitle_label

    def _make_stat_card(self, variant: str, title: str, subtitle: str):
        card = QFrame()
        card.setObjectName("StatCard")
        card.setProperty("variant", variant)
        theme.card_shadow(card)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(22, 18, 22, 18)
        layout.setSpacing(6)
        title_label = QLabel(title.upper())
        title_label.setObjectName("StatTitle")
        value_label = QLabel("0")
        value_label.setObjectName("StatValue")
        subtitle_label = QLabel(subtitle)
        subtitle_label.setObjectName("StatSubtitle")
        layout.addWidget(title_label)
        layout.addWidget(value_label)
        layout.addWidget(subtitle_label)
        return card, value_label

    def _add_gradient_row(self, body: QVBoxLayout, object_name: str, title: str):
        """One labeled gradient progress bar (Overview card). Returns the bar
        and the percentage QLabel so callers can update both later."""
        header = QHBoxLayout()
        header.addWidget(QLabel(title))
        header.addStretch(1)
        value_label = QLabel("0%")
        value_label.setObjectName("CardSubtitle")
        header.addWidget(value_label)
        body.addLayout(header)
        bar = QProgressBar()
        bar.setObjectName(object_name)
        bar.setProperty("gradientBar", "true")
        bar.setTextVisible(False)
        bar.setRange(0, 100)
        body.addWidget(bar)
        return bar, value_label

    def _build_dashboard_page(self) -> QWidget:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 6, 30, 30)
        layout.setSpacing(18)

        stats_row = QHBoxLayout()
        stats_row.setSpacing(18)
        online_card, self.stat_online_value = self._make_stat_card("online", "Online", "Thiết bị đang online")
        offline_card, self.stat_offline_value = self._make_stat_card("offline", "Offline", "Thiết bị mất kết nối")
        groups_card, self.stat_groups_value = self._make_stat_card("groups", "Groups", "Nhóm đã cấu hình")
        stats_row.addWidget(online_card)
        stats_row.addWidget(offline_card)
        stats_row.addWidget(groups_card)
        layout.addLayout(stats_row)

        overview_card, overview_body, _ = self._make_card(
            "Overview", "Tỉ lệ thiết bị online và kết quả thao tác gần nhất."
        )
        self.online_ratio_bar, self.online_ratio_value = self._add_gradient_row(
            overview_body, "GradientOnline", "Thiết bị online"
        )
        self.job_ratio_bar, self.job_ratio_value = self._add_gradient_row(
            overview_body, "GradientJob", "Phản hồi lần thao tác gần nhất"
        )
        layout.addWidget(overview_card)

        detection_card, detection_body, _ = self._make_card(
            "Device Detection", "Quét và nhận diện các thiết bị HUB66S đang có."
        )
        row = QHBoxLayout()
        row.addWidget(QLabel("Số lượng thiết bị"))
        self.scan_limit = QSpinBox()
        self.scan_limit.setRange(1, 100)
        self.scan_limit.setValue(15)
        row.addWidget(self.scan_limit)
        self.scan_btn = QPushButton("Scan")
        self.scan_btn.setProperty("variant", "primary")
        self.scan_btn.setToolTip(
            "Dò lại từ đầu: XOÁ toàn bộ danh sách rồi chỉ thêm lại thiết bị\n"
            "thực sự phản hồi. Thiết bị mất điện sẽ biến mất khỏi danh sách,\n"
            "không hiện Offline. Dùng khi thêm thiết bị mới."
        )
        self.scan_btn.clicked.connect(self._action_scan)
        row.addWidget(self.scan_btn)
        self.refresh_btn = QPushButton("Refresh")
        self.refresh_btn.setToolTip(
            "Hỏi lại từng thiết bị ĐÃ BIẾT, giữ nguyên danh sách. Thiết bị\n"
            "không phản hồi (vd. mất điện) sẽ chuyển sang Offline thay vì\n"
            "biến mất. Dùng khi muốn kiểm tra thiết bị nào đang mất kết nối."
        )
        self.refresh_btn.clicked.connect(lambda: self._send("scan.refresh"))
        row.addWidget(self.refresh_btn)
        self.clear_btn = QPushButton("Clear")
        self.clear_btn.setProperty("variant", "danger")
        self.clear_btn.clicked.connect(self._action_clear_all)
        row.addWidget(self.clear_btn)
        row.addStretch(1)
        detection_body.addLayout(row)
        self.job_status_label = QLabel("Sẵn sàng — nhập số lượng thiết bị rồi bấm Scan.")
        self.job_status_label.setObjectName("CardSubtitle")
        detection_body.addWidget(self.job_status_label)
        self.progress = QProgressBar()
        self.progress.setTextVisible(False)
        detection_body.addWidget(self.progress)
        layout.addWidget(detection_card)

        log_card, log_body, _ = self._make_card("Activity Log", "Thao tác qua WebSocket/USB và hoạt động của thiết bị.")
        self.log_view = QPlainTextEdit()
        self.log_view.setObjectName("LogView")
        self.log_view.setReadOnly(True)
        self.log_view.setMaximumBlockCount(2000)
        self.log_view.setFixedHeight(160)
        log_body.addWidget(self.log_view)
        layout.addWidget(log_card)

        layout.addStretch(1)
        scroll.setWidget(content)
        return scroll

    def _build_devices_page(self) -> QWidget:
        self.devices_scroll = QScrollArea()
        scroll = self.devices_scroll
        scroll.setWidgetResizable(True)
        content = QWidget()
        self.devices_content = content
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 6, 30, 30)

        card, body, self.device_count_label = self._make_card("All Devices", "0 / 0 thiết bị")
        self.devices_card = card

        # Two rows instead of one long one: a wide window/small "Số cột" is
        # fine, but cramming search + column-count + 4 bulk-action buttons
        # into a single row was what actually forced the page to scroll
        # horizontally (the device grid itself already fit).
        filter_row = QHBoxLayout()
        filter_row.addWidget(QLabel("Tìm theo UID:"))
        self.search_edit = QLineEdit()
        self.search_edit.setFixedWidth(160)
        filter_row.addWidget(self.search_edit)
        self.search_btn = QPushButton("Search")
        self.search_btn.clicked.connect(self._action_search)
        filter_row.addWidget(self.search_btn)
        filter_row.addSpacing(16)
        filter_row.addWidget(QLabel("Group:"))
        # "Tất cả thiết bị" (every known device) + one entry per Group — each
        # Group is its own physical location/rack with an independent saved
        # matrix layout + column count (see _load_layouts). Group
        # creation/editing itself stays on the separate Groups page; this
        # only picks which one to view/arrange here.
        self.grid_scope_combo = QComboBox()
        self.grid_scope_combo.setMinimumWidth(180)
        self.grid_scope_combo.currentIndexChanged.connect(self._on_grid_scope_changed)
        filter_row.addWidget(self.grid_scope_combo)
        filter_row.addSpacing(16)
        filter_row.addWidget(QLabel("Số cột:"))
        self.columns_spin = QSpinBox()
        self.columns_spin.setRange(1, 10)
        self.columns_spin.setValue(DEFAULT_DEVICE_COLUMNS)
        self.columns_spin.setToolTip("Số cột của ma trận — đặt bằng số cột bảng phần cứng thật")
        self.columns_spin.valueChanged.connect(self._on_columns_changed)
        filter_row.addWidget(self.columns_spin)
        filter_row.addStretch(1)
        body.addLayout(filter_row)

        actions_row = QHBoxLayout()
        actions_label = QLabel("Tick chọn để thao tác hàng loạt:")
        actions_row.addWidget(actions_label)
        self.get_license_btn = QPushButton("Get License")
        self.get_license_btn.clicked.connect(lambda: self._action_get_license_devices())
        actions_row.addWidget(self.get_license_btn)
        self.get_info_btn = QPushButton("Get Info")
        self.get_info_btn.setToolTip(
            "Đọc firmware/điện áp/nhiệt độ/uptime (LIC_INFO) — chỉ thiết bị đã update code mới trả đủ dữ liệu"
        )
        self.get_info_btn.clicked.connect(lambda: self._action_get_info_devices())
        actions_row.addWidget(self.get_info_btn)
        set_license_box = QVBoxLayout()
        set_license_box.setSpacing(2)
        set_license_box.setContentsMargins(0, 0, 0, 0)
        self.set_license_btn = QPushButton("Set License")
        self.set_license_btn.setProperty("variant", "primary")
        self.set_license_btn.clicked.connect(lambda: self._action_set_license_devices())
        set_license_box.addWidget(self.set_license_btn)
        self.last_set_license_label = QLabel()
        self.last_set_license_label.setObjectName("CardSubtitle")
        self.last_set_license_label.setStyleSheet("font-weight:700;")
        self.last_set_license_label.setAlignment(Qt.AlignmentFlag.AlignHCenter)
        # Hidden (not removed) for non-admin accounts so the button row keeps
        # the same height/alignment for everyone.
        retain = self.last_set_license_label.sizePolicy()
        retain.setRetainSizeWhenHidden(True)
        self.last_set_license_label.setSizePolicy(retain)
        set_license_box.addWidget(self.last_set_license_label)
        actions_row.addLayout(set_license_box)
        self._update_last_set_license_label()
        self.config_device_btn = QPushButton("Config Device")
        self.config_device_btn.clicked.connect(lambda: self._action_config_device())
        actions_row.addWidget(self.config_device_btn)
        self.set_card_type_btn = QPushButton("Set Card Type")
        self.set_card_type_btn.setToolTip(
            "Ghi loại board (Card_R rời / Card_OB onboard) xuống node — để lần\n"
            "scan sau app tự biết dùng ảnh board nào, không cần cấu hình lại."
        )
        self.set_card_type_btn.clicked.connect(lambda: self._action_set_card_type())
        actions_row.addWidget(self.set_card_type_btn)
        self.delete_device_btn = QPushButton("Remove")
        self.delete_device_btn.setProperty("variant", "danger")
        self.delete_device_btn.clicked.connect(lambda: self._action_delete_devices())
        actions_row.addWidget(self.delete_device_btn)
        actions_row.addStretch(1)
        # set_license_box is taller than every plain button beside it (button
        # + caption stacked). Left at the row's default cross-axis alignment,
        # Qt centers the plain buttons in that extra height while the boxed
        # button stays pinned to its own top — visibly out of line with the
        # rest. Pinning every item to the top keeps the Set License button
        # exactly where a plain button would sit, with only the caption
        # extending below it.
        for item in (actions_label, self.get_license_btn, self.get_info_btn, set_license_box,
                     self.config_device_btn, self.set_card_type_btn, self.delete_device_btn):
            actions_row.setAlignment(item, Qt.AlignmentFlag.AlignTop)
        self.set_matrix_btn = QPushButton("Set Matrix")
        self.set_matrix_btn.setProperty("variant", "primary")
        self.set_matrix_btn.setToolTip(
            "Ghi vị trí Hàng/Cột + tên Group hiện tại xuống từng board online\n"
            "trong Group, để lần scan sau board tự báo lại — dễ phát hiện xếp nhầm.\n"
            "Chỉ dùng được khi đang lọc theo 1 Group cụ thể (không phải \"Tất cả\")."
        )
        self.set_matrix_btn.clicked.connect(self._action_set_matrix)
        self.set_matrix_btn.setEnabled(False)
        actions_row.addWidget(self.set_matrix_btn)
        body.addLayout(actions_row)

        # A drag-to-reorder grid: "Tất cả thiết bị" shows every device flat
        # (no position chip — there's no single physical layout to compare
        # against); picking a Group instead filters this same grid down to
        # that Group's members and switches on the "Position" chip + Set
        # Matrix, since a Group is its own physical location/rack with its
        # own independent saved layout + column count (see _load_layouts).
        grid_header = QHBoxLayout()
        grid_header.setContentsMargins(0, 10, 0, 4)
        grid_title = QLabel("Danh sách thiết bị (kéo thả để sắp xếp)")
        grid_title.setStyleSheet("font-weight:800; font-size:13px;")
        grid_header.addWidget(grid_title)
        self.grid_count_label = QLabel("0 thiết bị")
        self.grid_count_label.setObjectName("CardSubtitle")
        grid_header.addWidget(self.grid_count_label)
        grid_header.addStretch(1)
        body.addLayout(grid_header)

        self.grid_list = DeviceListView(DEFAULT_DEVICE_COLUMNS, show_position=False)
        self.grid_list.cardClicked.connect(self._open_device_detail)
        self.grid_list.reordered.connect(self._on_grid_reordered)
        self.grid_list.selectionChanged.connect(self._identify_debounce.start)
        self.grid_list.emptySlotRequested.connect(self._on_empty_slot_requested)
        self.grid_list.emptySlotRemoveRequested.connect(self._on_empty_slot_remove_requested)
        body.addWidget(self.grid_list)
        # grid_list is fixed-height (see DeviceListView._recompute_height),
        # so it can't itself absorb the leftover space body's own stretch=1
        # now guarantees it (see _make_card) — claim that leftover here
        # instead, so it collapses below the grid rather than spreading back
        # out into gaps between filter_row/actions_row/grid_header above.
        body.addStretch(1)

        layout.addWidget(card, 1)
        scroll.setWidget(content)

        self.device_action_buttons = [
            self.scan_btn, self.refresh_btn, self.clear_btn, self.search_btn,
            self.get_license_btn, self.get_info_btn, self.set_license_btn,
            self.config_device_btn, self.delete_device_btn,
            self.set_card_type_btn,
        ]
        return scroll

    def _build_groups_page(self) -> QWidget:
        """Plain Group management: create/rename/delete a Group and pick its
        members. Arranging a Group's own physical matrix happens elsewhere
        now — see "Ma trận theo vị trí (Group)" on the Devices page."""
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 6, 30, 30)

        card, body, _ = self._make_card("Groups", "Tạo và quản lý nhóm thiết bị.")
        splitter = QSplitter()
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        self.group_list = QListWidget()
        self.group_list.currentItemChanged.connect(self._on_group_selected)
        left_layout.addWidget(self.group_list, 1)
        left_buttons = QHBoxLayout()
        self.new_group_btn = QPushButton("Mới")
        self.new_group_btn.setProperty("variant", "primary")
        self.new_group_btn.clicked.connect(self._action_new_group)
        left_buttons.addWidget(self.new_group_btn)
        self.edit_group_btn = QPushButton("Sửa")
        self.edit_group_btn.clicked.connect(self._action_edit_group)
        left_buttons.addWidget(self.edit_group_btn)
        self.delete_group_btn = QPushButton("Xoá")
        self.delete_group_btn.setProperty("variant", "danger")
        self.delete_group_btn.clicked.connect(self._action_delete_group)
        left_buttons.addWidget(self.delete_group_btn)
        left_layout.addLayout(left_buttons)
        splitter.addWidget(left)

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.setContentsMargins(0, 0, 0, 0)
        self.group_member_table = QTableWidget(0, len(DEVICE_COLUMNS))
        self.group_member_table.setHorizontalHeaderLabels(DEVICE_COLUMNS)
        _configure_table_columns(
            self.group_member_table,
            stretch_column=DEVICE_COLUMNS.index("Alias"),
            fixed_widths={STT_COLUMN: 48, STATE_COLUMN: 120},
        )
        self.group_member_table.verticalHeader().setVisible(False)
        self.group_member_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        right_layout.addWidget(self.group_member_table, 1)
        group_actions = QHBoxLayout()
        self.group_get_license_btn = QPushButton("Get License (cả group)")
        self.group_get_license_btn.clicked.connect(self._action_get_license_group)
        group_actions.addWidget(self.group_get_license_btn)
        self.group_get_info_btn = QPushButton("Get Info (cả group)")
        self.group_get_info_btn.clicked.connect(self._action_get_info_group)
        group_actions.addWidget(self.group_get_info_btn)
        self.group_set_license_btn = QPushButton("Set License (cả group)")
        self.group_set_license_btn.setProperty("variant", "primary")
        self.group_set_license_btn.clicked.connect(self._action_set_license_group)
        group_actions.addWidget(self.group_set_license_btn)
        group_actions.addStretch(1)
        right_layout.addLayout(group_actions)
        splitter.addWidget(right)
        splitter.setSizes([260, 700])
        body.addWidget(splitter, 1)

        layout.addWidget(card, 1)

        self.group_action_buttons = [
            self.new_group_btn, self.edit_group_btn, self.delete_group_btn,
            self.group_get_license_btn, self.group_get_info_btn, self.group_set_license_btn,
        ]
        return content

    def _build_accounts_page(self) -> QWidget:
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(30, 6, 30, 30)

        self.create_account_btn = QPushButton("Add Account")
        self.create_account_btn.setProperty("variant", "primary")
        self.create_account_btn.clicked.connect(self._action_create_account)

        card, body, _ = self._make_card(
            "Accounts", "Tạo tài khoản, đổi mật khẩu và cấu hình Google Authenticator.",
            header_widget=self.create_account_btn,
        )

        self.account_table = QTableWidget(0, len(ACCOUNT_COLUMNS))
        self.account_table.setHorizontalHeaderLabels(ACCOUNT_COLUMNS)
        _configure_table_columns(self.account_table, stretch_column=0)
        self.account_table.verticalHeader().setVisible(False)
        self.account_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.account_table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        body.addWidget(self.account_table, 1)

        actions = QHBoxLayout()
        self.change_password_btn = QPushButton("Password")
        self.change_password_btn.clicked.connect(self._action_change_password)
        actions.addWidget(self.change_password_btn)
        self.toggle_2fa_btn = QPushButton("Enable/Disable 2FA")
        self.toggle_2fa_btn.clicked.connect(self._action_toggle_2fa)
        actions.addWidget(self.toggle_2fa_btn)
        self.delete_account_btn = QPushButton("Delete")
        self.delete_account_btn.setProperty("variant", "danger")
        self.delete_account_btn.clicked.connect(self._action_delete_account)
        actions.addWidget(self.delete_account_btn)
        actions.addStretch(1)
        body.addLayout(actions)

        layout.addWidget(card, 1)

        self.account_action_buttons = [
            self.create_account_btn, self.change_password_btn,
            self.delete_account_btn, self.toggle_2fa_btn,
        ]
        return content

    # ------------------------------------------------------------ helpers
    def log(self, text: str) -> None:
        self.log_view.appendPlainText(text)

    def _on_nav_clicked(self, key: str) -> None:
        if key == "accounts":
            self.stack.setCurrentWidget(self.accounts_page)
            self.page_title_label.setText("Accounts")
            self.page_subtitle_label.setText(
                "Tạo tài khoản, đổi mật khẩu và cấu hình Google Authenticator."
            )
        else:
            page = {
                "dashboard": self.dashboard_page,
                "devices": self.devices_page,
                "groups": self.groups_page,
            }[key]
            self.stack.setCurrentWidget(page)
            self.page_title_label.setText("CONTROLS SYSTEM")
            self.page_subtitle_label.setText(self._hub_subtitle_text)

    def _set_hub_subtitle(self, text: str) -> None:
        self._hub_subtitle_text = text
        if self.stack.currentWidget() != self.accounts_page:
            self.page_subtitle_label.setText(text)

    def _set_connection_status(self, connected: bool, text: str) -> None:
        self.status_dot.setProperty("connected", "true" if connected else "false")
        self.status_dot.style().unpolish(self.status_dot)
        self.status_dot.style().polish(self.status_dot)
        self.status_text.setText(text)

    def _open_connection_settings(self) -> None:
        if self.transport_combo.currentText() == "USB":
            port, ok = QInputDialog.getText(
                self, "Kết nối", "Cổng USB (vd: /dev/ttyACM0):", text=self.port_combo.currentText()
            )
            if ok and port.strip():
                self.port_combo.setEditText(port.strip())
            return
        host, ok = QInputDialog.getText(
            self, "Kết nối", "Địa chỉ Hub (IP):", text=self.host_edit.text()
        )
        if ok and host.strip():
            self.host_edit.setText(host.strip())

    def _set_signed_in(self, signed_in: bool) -> None:
        self._update_action_buttons()

    def _update_action_buttons(self) -> None:
        enabled = self.username is not None and not self.busy
        for button in (
            self.device_action_buttons + self.group_action_buttons + self.account_action_buttons
        ):
            button.setEnabled(enabled)

    def _grid_online_total(self) -> tuple[int, int]:
        """(online, total) over the whole matrix (grid_order), not just
        what's live in `devices` — an offline slot still counts as a
        device, just offline. `None` entries are deliberately-vacated
        slots (see DeviceCard.emptySlotRequested), not devices — excluded
        from both counts."""
        macs = [m for m in self.grid_order if m is not None]
        online = sum(1 for m in macs if theme.effective_response_state(self.devices.get(m, {})) == "ONLINE")
        return online, len(macs)

    def _update_stat_cards(self) -> None:
        online, total = self._grid_online_total()
        self.stat_online_value.setText(str(online))
        self.stat_offline_value.setText(str(total - online))
        self.stat_groups_value.setText(str(len(self.groups)))

        self._set_gradient_row(self.online_ratio_bar, self.online_ratio_value, online, total)
        self._set_gradient_row(
            self.job_ratio_bar, self.job_ratio_value, self._last_job_success, self._last_job_total
        )

    @staticmethod
    def _set_gradient_row(bar: QProgressBar, value_label: QLabel, count: int, total: int) -> None:
        percent = round(100 * count / total) if total else 0
        bar.setValue(percent)
        value_label.setText(f"{count}/{total} ({percent}%)" if total else "—")

    def selected_device_macs(self) -> list[str]:
        return [mac for mac, card in self.device_cards.items() if card.is_checked()]

    def _sync_identify(self) -> None:
        """Make the Hub's blinking boards match the ticked cards: tell newly
        ticked boards to blink and newly unticked ones to stop."""
        if not (self.client.is_connected() and self.username is not None):
            self._identify_active.clear()
            return
        wanted = {mac for mac in self.selected_device_macs() if mac in self.devices}
        to_off = sorted(self._identify_active - wanted)
        to_on = sorted(wanted - self._identify_active)
        if to_off:
            self._send("device.identify", {"members": to_off, "on": False}, log=False)
        if to_on:
            self._send("device.identify", {"members": to_on, "on": True, "seconds": IDENTIFY_SECONDS}, log=False)
        self._identify_active = wanted

    def _renew_identify(self) -> None:
        if self._identify_active and self.client.is_connected() and self.username is not None:
            self._send(
                "device.identify",
                {"members": sorted(self._identify_active), "on": True, "seconds": IDENTIFY_SECONDS},
                log=False,
            )

    def closeEvent(self, event) -> None:  # noqa: N802 (Qt override)
        # Best effort: switch the LEDs off rather than leaving them to time out.
        if self._identify_active and self.client.is_connected():
            self._send("device.identify", {"members": sorted(self._identify_active), "on": False}, log=False)
            self._identify_active.clear()
        super().closeEvent(event)

    def _on_columns_changed(self, value: int) -> None:
        self.grid_list.set_columns(value)
        self.layouts.setdefault(self.current_scope, {})["columns"] = value
        _save_layouts(self.layouts)
        # Column count changes what row/col every slot maps to → re-badge.
        self._refresh_devices_view()

    def _on_grid_reordered(self, order: list[str | None]) -> None:
        self.grid_order = order
        self.layouts.setdefault(self.current_scope, {})["grid"] = order
        _save_layouts(self.layouts)
        # Rebuild rather than trust the drag to have left every card's
        # checkbox/click wiring intact (see DeviceListView's docstring).
        self._refresh_devices_view()

    def _on_empty_slot_requested(self, mac: str) -> None:
        """Right-click a card → "Để trống vị trí này": lets the current
        Group's matrix mirror a physical layout that isn't a plain filled
        rectangle (corners cut, a ring, a shape with gaps...) by carving out
        specific slots. The card moves to the end of the grid - same place
        a plain drag past the last card already lands it - and its old slot
        becomes an empty one the operator can later drag any other card
        into (DeviceListView.dropEvent already swaps onto an empty slot the
        same way it swaps two real cards)."""
        if mac not in self.grid_order:
            return
        self.grid_order[self.grid_order.index(mac)] = None
        self.grid_order.append(mac)
        self.layouts.setdefault(self.current_scope, {})["grid"] = self.grid_order
        _save_layouts(self.layouts)
        self._refresh_devices_view()

    def _on_empty_slot_remove_requested(self, index: int) -> None:
        """Right-click an empty slot → "Bỏ ô trống này": closes that one
        gap, shifting every later slot up by one - the opposite of
        _on_empty_slot_requested."""
        if 0 <= index < len(self.grid_order) and self.grid_order[index] is None:
            del self.grid_order[index]
            self.layouts.setdefault(self.current_scope, {})["grid"] = self.grid_order
            _save_layouts(self.layouts)
            self._refresh_devices_view()

    def _group_by_id(self, gid) -> dict | None:
        return next((g for g in self.groups if g["id"] == gid), None)

    def _prune_orphan_layouts(self) -> None:
        """Drops saved layouts for Groups that no longer exist (deleted on
        the Groups page) — otherwise they'd sit orphaned in
        device_order.json forever with no way to reach them again."""
        if not self._groups_loaded:
            # self.groups is still the empty startup placeholder, not a real
            # answer from the Hub — every Group would look "deleted".
            return
        live_keys = {ALL_DEVICES_SCOPE, *(str(g["id"]) for g in self.groups)}
        for stale_key in list(self.layouts):
            if stale_key not in live_keys:
                del self.layouts[stale_key]

    def _group_for_scope(self, scope: str) -> dict | None:
        if scope == ALL_DEVICES_SCOPE:
            return None
        return next((g for g in self.groups if str(g["id"]) == scope), None)

    def _refresh_grid_scope_combo(self) -> None:
        """Keeps the Devices-page "Group" filter in sync with self.groups
        (add/rename/remove) — Groups themselves are only ever
        created/edited/deleted on the separate Groups page; this picker
        just chooses what the one grid here shows: every device flat
        ("Tất cả thiết bị") or one Group's members."""
        previous = self.current_scope
        self.grid_scope_combo.blockSignals(True)
        self.grid_scope_combo.clear()
        self.grid_scope_combo.addItem("Tất cả thiết bị", ALL_DEVICES_SCOPE)
        restore_index = 0
        for group in self.groups:
            key = str(group["id"])
            self.grid_scope_combo.addItem(
                f"{group['name']} ({group.get('memberCount', 0)})", key
            )
            if key == previous:
                restore_index = self.grid_scope_combo.count() - 1
        self.grid_scope_combo.setCurrentIndex(restore_index)
        self.grid_scope_combo.blockSignals(False)
        self._prune_orphan_layouts()
        self._set_grid_scope(self.grid_scope_combo.currentData())

    def _on_grid_scope_changed(self, _index: int) -> None:
        scope = self.grid_scope_combo.currentData()
        if scope is not None:
            self._set_grid_scope(scope)

    def _set_grid_scope(self, scope: str) -> None:
        """Switches the one Devices-page grid between the flat "Tất cả
        thiết bị" view and a specific Group's own matrix. Each scope keeps
        its own saved order + column count in self.layouts (see
        _load_layouts); a Group-scoped grid is seeded from (and pruned to)
        that Group's current member list."""
        self.current_scope = scope
        saved = self.layouts.setdefault(scope, {"grid": [], "columns": DEFAULT_DEVICE_COLUMNS})
        grid = list(saved.get("grid", []))
        group = self._group_for_scope(scope)
        if group is not None:
            members = list(group.get("members", []))
            member_set = set(members)
            # None = a deliberately-vacated slot (see
            # DeviceCard.emptySlotRequested/EmptySlotCard) - keep it; it's
            # not a member and would otherwise get pruned right along with
            # macs that actually left the Group, closing every gap the
            # operator carved out the next time this scope loads.
            grid = [mac for mac in grid if mac is None or mac in member_set]
            grid += [mac for mac in members if mac not in grid]
            saved["grid"] = grid
        self.grid_order = grid
        columns = saved.get("columns") or DEFAULT_DEVICE_COLUMNS
        self.columns_spin.blockSignals(True)
        self.columns_spin.setValue(columns)
        self.columns_spin.blockSignals(False)
        self.grid_list.set_columns(columns)
        self.set_matrix_btn.setEnabled(group is not None)
        _save_layouts(self.layouts)
        self._refresh_devices_view()
        # Switching scope changes the grid's total height (different device
        # count/row count) — resetting to the top avoids the page appearing
        # to "jump" (an old scroll offset now landing partway down content
        # of a different height). Deferred one event-loop tick so it runs
        # after Qt has finished recomputing the scroll area's content
        # geometry for the new row count, rather than racing it.
        QTimer.singleShot(0, lambda: self.devices_scroll.verticalScrollBar().setValue(0))

    def _action_set_matrix(self) -> None:
        group = self._group_for_scope(self.current_scope)
        if group is None:
            return
        cols = self.columns_spin.value()
        members = []
        for i, mac in enumerate(self.grid_order):
            node = self.devices.get(mac)
            if node is None or theme.effective_response_state(node) != "ONLINE":
                continue  # offline boards can't receive it now — do them next time
            members.append({
                "mac": mac,
                "row": i // cols + 1,
                "col": i % cols + 1,
                "alias": (node.get("alias") or "").strip(),
                "group": group["name"],
            })
        if not members:
            QMessageBox.information(
                self, "Set Matrix", "Không có board online nào để ghi vị trí."
            )
            return
        if QMessageBox.question(
            self, "Set Matrix",
            f'Ghi vị trí Hàng/Cột + tên group "{group["name"]}" xuống {len(members)} board online?\n'
            "(Board phải chạy firmware đọc được \"Matrix\":{x,y} và \"group\" trong opcode Config)",
        ) != QMessageBox.StandardButton.Yes:
            return
        self._send("matrix.set", {"members": members})

    def selected_group(self) -> dict | None:
        item = self.group_list.currentItem()
        if item is None:
            return None
        gid = item.data(Qt.ItemDataRole.UserRole)
        return self._group_by_id(gid)

    def selected_account(self) -> dict | None:
        row = self.account_table.currentRow()
        if row < 0 or row >= len(self.accounts):
            return None
        return self.accounts[row]

    # --------------------------------------------------------- connection
    def _on_transport_changed(self, text: str) -> None:
        is_usb = text == "USB"
        self.host_edit.setVisible(not is_usb)
        self.port_combo.setVisible(is_usb)
        self.port_refresh_btn.setVisible(is_usb)
        if is_usb:
            self._refresh_serial_ports()

    def _refresh_serial_ports(self) -> None:
        current = self.port_combo.currentText()
        self.port_combo.clear()
        ports = list_serial_ports()
        self.port_combo.addItems(ports)
        if current:
            self.port_combo.setEditText(current)
        elif ports:
            self.port_combo.setCurrentIndex(0)

    def _toggle_connection(self) -> None:
        if self.client.is_connected():
            self.client.close()
            return
        self._active_is_usb = self.transport_combo.currentText() == "USB"
        self._set_connection_status(False, "Đang kết nối…")
        if self._active_is_usb:
            port = self.port_combo.currentText().strip()
            if not port:
                QMessageBox.warning(self, "Chưa chọn cổng", "Chọn cổng USB (vd: /dev/ttyACM0)")
                self._set_connection_status(False, "Chưa kết nối")
                return
            self.serial_client.connect_to(port, DEFAULT_BAUD_RATE)
        else:
            host = self.host_edit.text().strip() or "192.168.4.1"
            self.ws_client.connect_to(host, DEFAULT_PORT)

    def _on_connected(self) -> None:
        self.connect_btn.setText("Ngắt kết nối")
        self._set_connection_status(False, "Chờ xác thực…")
        if self._active_is_usb:
            self.log("USB connected")
            # Opening the native-USB-CDC port resets the ESP32-S3 (DTR/RTS
            # toggle). The reboot can take anywhere from ~1s to several
            # seconds (WiFi AP bring-up, NVS init right after a full-chip
            # erase, ...), so a single delayed probe can land on a board
            # that isn't listening yet and get silently dropped — leaving
            # the UI stuck on "waiting" forever with no way to recover
            # short of reconnecting. Keep probing every second instead;
            # _probe_usb_auth()/_stop_usb_probe() stop it once either the
            # login dialog is up (auth.required) or login succeeds.
            self.job_status_label.setText("Đang chờ board khởi động lại…")
            self._usb_probe_timer.start(1000)
        else:
            self.log("Socket connected")

    def _probe_usb_auth(self) -> None:
        if not (self._active_is_usb and self.client.is_connected()) or self.username is not None:
            self._stop_usb_probe()
            return
        self._send("state.get")

    def _stop_usb_probe(self) -> None:
        self._usb_probe_timer.stop()

    def _on_disconnected(self) -> None:
        self._stop_usb_probe()
        self._login_timeout.stop()
        self._auto_refresh_timer.stop()
        self._identify_active.clear()
        if self._login_dialog is not None:
            self._login_dialog.reject()
        if self._twofa_dialog is not None:
            self._twofa_dialog.reject()
        self._set_connection_status(False, "Chưa kết nối")
        self.connect_btn.setText("Kết nối")
        self.username = None
        self.busy = False
        self.devices.clear()
        self.groups.clear()
        # Same reasoning as at startup (see _prune_orphan_layouts): this
        # empties self.groups without it meaning any Group actually got
        # deleted, so pruning must not run again until a real list comes
        # back in on reconnect.
        self._groups_loaded = False
        self.accounts.clear()
        self._refresh_devices_view()
        self._refresh_group_list()
        self._refresh_account_table()
        self._update_stat_cards()
        self._set_hub_subtitle("Điểm truy cập nội bộ")
        self.setWindowTitle("TWT Led Control System")
        self._set_signed_in(False)
        self.log("Socket disconnected")

    def _on_socket_error(self, message: str) -> None:
        self._set_connection_status(False, "Lỗi kết nối")
        self.log(f"Socket error: {message}")

    def _action_sign_out(self) -> None:
        if not self.client.is_connected():
            return
        if self.username is not None:
            self._send("auth.logout")
        else:
            self.client.close()

    # -------------------------------------------------------------- auth
    def _open_login_dialog(self) -> None:
        # Non-blocking on purpose: a blocking exec() here starts a nested
        # event loop, and while that loop is the only one on the stack the
        # server's auth.success/state replies can take seconds to be
        # delivered back to this same slot (observed ~4s in testing). Using
        # open() keeps the normal event loop in charge so replies arrive
        # promptly, and the dialog is closed explicitly by the auth handlers.
        if self._login_dialog is not None:
            return
        dialog = LoginDialog(self)
        self._login_dialog = dialog
        dialog.submit.connect(self._on_login_submit)
        dialog.finished.connect(self._on_login_dialog_finished)
        dialog.open()

    def _on_login_dialog_finished(self, result: int) -> None:
        self._login_dialog = None
        self._login_timeout.stop()
        if result != QDialog.DialogCode.Accepted:
            self.client.close()

    def _on_login_submit(self) -> None:
        dialog = self._login_dialog
        if dialog is None:
            return
        dialog.set_error("")
        if dialog.waiting_for_otp():
            self._send("auth.verify_otp", {"code": dialog.otp_code(), "epoch": int(time.time())})
        else:
            user, password = dialog.credentials()
            self._send(
                "auth.login", {"username": user, "password": password, "epoch": int(time.time())}
            )
        self._login_timeout.start(6000)

    def _on_login_timeout(self) -> None:
        if self._login_dialog is None:
            return
        self._login_dialog.set_error(
            "Hub không phản hồi sau 6 giây — kết nối có thể đã bị treo. "
            "Bấm Cancel rồi Kết nối lại."
        )
        self.log("Login timed out waiting for a response; closing the stale socket")
        self.client.close()

    # ---------------------------------------------------------- messages
    def _on_message(self, obj: dict) -> None:
        msg_type = obj.get("type")
        handler = {
            "auth.required": self._handle_auth_required,
            "auth.otp_required": self._handle_otp_required,
            "auth.success": self._handle_auth_success,
            "error": self._handle_error,
            "ack": self._handle_ack,
            "state": self._handle_state,
            "device.upsert": self._handle_device_upsert,
            "device.deleted": self._handle_device_deleted,
            "devices.cleared": self._handle_devices_cleared,
            "groups.changed": self._handle_groups_changed,
            "job": self._handle_job,
            "accounts": self._handle_accounts,
            "account.2fa.setup": self._handle_2fa_setup,
        }.get(msg_type)
        if handler:
            handler(obj)
        else:
            self.log(f"<< {obj}")

    def _handle_auth_required(self, _obj: dict) -> None:
        self._stop_usb_probe()
        self._login_timeout.stop()
        self.username = None
        self._identify_active.clear()
        self.devices.clear()
        self.groups.clear()
        self.accounts.clear()
        self._refresh_devices_view()
        self._refresh_group_list()
        self._refresh_account_table()
        self._update_stat_cards()
        self._set_hub_subtitle("Điểm truy cập nội bộ")
        self.setWindowTitle("TWT Led Control System")
        self._set_connection_status(False, "Chờ xác thực…")
        self._set_signed_in(False)
        self._open_login_dialog()

    def _handle_otp_required(self, _obj: dict) -> None:
        self._login_timeout.stop()
        if self._login_dialog is not None:
            self._login_dialog.show_otp()

    def _handle_auth_success(self, obj: dict) -> None:
        self._stop_usb_probe()
        self._login_timeout.stop()
        self.username = obj.get("username")
        if self._login_dialog is not None:
            self._login_dialog.accept()
        self._set_connection_status(True, "Đã kết nối (USB)" if self._active_is_usb else "Đã kết nối")
        self.setWindowTitle(f"TWT Led Control System — {self.username}")
        self._set_signed_in(True)
        self._apply_role_view()
        self._send("account.list")
        self.log(f"Signed in as {self.username}")

    def _handle_error(self, obj: dict) -> None:
        code = obj.get("code", "")
        message = obj.get("message", "")
        if self._login_dialog is not None and code in ("LOGIN_FAILED", "OTP_FAILED"):
            self._login_timeout.stop()
            self._login_dialog.set_error(message)
            return
        if self._twofa_dialog is not None and code == "OTP_FAILED":
            self._twofa_dialog.set_error(message)
            return
        self.log(f"Error [{code}]: {message}")
        self.job_status_label.setText(f"Lỗi: {message}")
        # job_status_label lives on the Dashboard page only, so an error
        # triggered from Devices/Groups/Accounts would otherwise be silent —
        # the status bar is part of QMainWindow's chrome and stays visible
        # no matter which page is showing.
        self.statusBar().showMessage(f"Lỗi [{code}]: {message}", 8000)

    def _handle_ack(self, obj: dict) -> None:
        action = obj.get("action", "")
        self.log(f"Ack: {action}")
        self.statusBar().showMessage(f"Đã thực hiện: {action}", 4000)
        if action == "account.2fa.confirm" and self._twofa_dialog is not None:
            self._twofa_dialog.accept()

    def _seed_last_known_from_groups(self) -> None:
        """The Hub keeps a durable per-Group backup of each member's last
        known alias/matrix/cardType/status (groupDevices[], persisted to
        NVS — see appendGroup() in local_web.h) that survives a Scan
        wiping the Hub's live "currently seen" device table entirely, and
        self-heals from a member's own next reply regardless. The flat
        `devices` list this app keeps in self.devices only reflects that
        live table, though, so without this, a Group member the Hub hasn't
        seen again *this app session* renders as a bare placeholder (no
        alias/position) in the matrix until it happens to respond. Folding
        every group's own `devices` array (already sent alongside `state`/
        `groups.changed`) into last_known means the matrix shows each
        member's real last-known info immediately instead."""
        for group in self.groups:
            for device in group.get("devices", []):
                mac = device.get("mac")
                if mac:
                    self.last_known[mac] = device

    def _handle_state(self, obj: dict) -> None:
        self.devices = {d["mac"]: d for d in obj.get("devices", [])}
        self.groups = obj.get("groups", [])
        self._groups_loaded = True
        self._seed_last_known_from_groups()
        if not self._auto_refresh_timer.isActive():
            self._auto_refresh_timer.start()
        ip = obj.get("ip", "")
        mac = obj.get("hubMac", "")
        self._set_hub_subtitle(f"AP {ip} | {mac}")
        self._refresh_devices_view()
        self._refresh_group_list()
        self._update_stat_cards()

    def _group_name_for_mac(self, mac: str) -> str | None:
        for g in self.groups:
            if mac in g.get("members", []):
                return g["name"]
        return None

    def _handle_device_upsert(self, obj: dict) -> None:
        data = obj.get("data", {})
        mac = data.get("mac")
        if not mac:
            return
        was_known = mac in self.devices
        self.devices[mac] = data
        self.last_known[mac] = data
        card = self.device_cards.get(mac)
        # Fast path: a node already on screen just gets its own card
        # repainted in place. `device.upsert` fires once per node per reply
        # - during a Scan/Refresh/bulk job across 70-100 boards that's up to
        # 100 of these in quick succession, and the old code called the full
        # _refresh_devices_view() (clear() + rebuild every DeviceCard, each
        # carrying its own QGraphicsDropShadowEffect - see
        # theme.card_shadow()) for EVERY one of them: up to ~100 full-grid
        # rebuilds of ~100 cards each, which is what made the UI stutter for
        # seconds at a time. Only fall back to the full rebuild when the
        # grid's own shape might actually have changed: a brand-new mac (All
        # Devices auto-adopts it into grid_order) or a node that isn't part
        # of the grid currently on screen at all (nothing to fast-update).
        if was_known and card is not None:
            node = dict(data)
            node["_licenseTotalMinutes"] = self.license_totals.get(mac)
            node["_currentGroupName"] = self._group_name_for_mac(mac)
            node["_showTimes"] = self.is_admin
            card.set_node(node)
            self.grid_list.sync_row_checkboxes()
            online, total = self._grid_online_total()
            self.device_count_label.setText(f"{online} / {total} online")
            self.grid_count_label.setText(f"{total} thiết bị · {total - online} offline")
        else:
            self._refresh_devices_view()
        self._update_stat_cards()
        if mac == self._open_detail_mac and self._open_detail_dialog is not None:
            self._open_detail_dialog.refresh(data)

    def _handle_device_deleted(self, obj: dict) -> None:
        mac = obj.get("mac")
        self.devices.pop(mac, None)
        # A Remove is deliberate — drop its matrix slot in every location's
        # saved layout, not just whichever one is on screen right now.
        self.last_known.pop(mac, None)
        if mac in self.grid_order:
            self.grid_order.remove(mac)
        for layout in self.layouts.values():
            grid = layout.get("grid")
            if isinstance(grid, list) and mac in grid:
                grid.remove(mac)
        _save_layouts(self.layouts)
        self._refresh_devices_view()
        self._update_stat_cards()

    def _handle_devices_cleared(self, _obj: dict) -> None:
        self.devices.clear()
        # A Scan also emits devices.cleared before re-discovering; only wipe
        # saved matrix layouts (every location, not just the one on screen)
        # when the operator hit "Clear" on purpose.
        if self._pending_layout_reset:
            self._pending_layout_reset = False
            self.last_known.clear()
            self.grid_order = []
            self.layouts = {}
            _save_layouts(self.layouts)
        self._refresh_devices_view()
        self._update_stat_cards()

    def _handle_groups_changed(self, obj: dict) -> None:
        self.groups = obj.get("groups", [])
        self._groups_loaded = True
        self._seed_last_known_from_groups()
        # _refresh_group_list() ends by calling _refresh_grid_scope_combo(),
        # which re-renders the Devices-page matrix (_set_grid_scope() ->
        # _refresh_devices_view()) — no separate call needed here.
        self._refresh_group_list()
        self._update_stat_cards()

    def _handle_job(self, obj: dict) -> None:
        state = obj.get("state", "")
        total = obj.get("total") or 0
        done = obj.get("done") or 0
        message = obj.get("message", "")
        self.progress.setMaximum(max(total, 1))
        self.progress.setValue(min(done, total) if total else 0)
        self.job_status_label.setText(f"[{state}] {message} ({done}/{total})")
        self.log(
            f"JOB {state}: {message} done={done} total={total} "
            f"success={obj.get('success')} noResponse={obj.get('noResponse')} "
            f"dropped={obj.get('dropped')}"
        )
        self.busy = state in ("started", "scanning", "retry", "progress")
        self._update_action_buttons()
        if state in ("completed", "failed"):
            self._last_job_success = obj.get("success") or 0
            self._last_job_total = total if total else (obj.get("success") or 0)
            self._update_stat_cards()

    def _handle_accounts(self, obj: dict) -> None:
        self.accounts = obj.get("accounts", [])
        self._refresh_account_table()

    def _handle_2fa_setup(self, obj: dict) -> None:
        # Non-blocking for the same reason as the login dialog: it must stay
        # open while account.2fa.confirm's reply comes back asynchronously.
        dialog = TwoFactorSetupDialog(obj.get("secret", ""), obj.get("qrSize", 0), obj.get("qrData", ""), self)
        self._twofa_dialog = dialog
        dialog.submit.connect(lambda: self._send(
            "account.2fa.confirm", {"code": dialog.code.text().strip(), "epoch": int(time.time())}
        ))
        dialog.finished.connect(lambda _result: setattr(self, "_twofa_dialog", None))
        dialog.open()

    # --------------------------------------------------------- rendering
    @property
    def is_admin(self) -> bool:
        return self.username == ADMIN_USERNAME

    def _apply_role_view(self) -> None:
        """Redraw everything that shows times after the signed-in account
        changes (admin → exact numbers, anyone else / signed out → gauges)."""
        self._refresh_devices_view()
        self._refresh_group_members()
        self._update_last_set_license_label()

    @staticmethod
    def _set_cell_widget(table: QTableWidget, row: int, col: int, widget: QWidget) -> None:
        """table.setCellWidget()/removeCellWidget() detach a cell's widget
        from the table but do NOT delete it - confirmed by inspection: the
        old widget stays alive, still parented to the table's viewport,
        and since nothing positions it anymore it renders stuck at the
        viewport's origin - a stray "ONLINE" pill overlapping row 0. Every
        cell that might already hold a widget (State pill, or a License/
        Tín hiệu gauge in non-admin mode) goes through here instead of
        calling setCellWidget()/removeCellWidget() directly, so a repeat
        refresh (auto-refresh tick, a groups.changed push, switching admin
        role...) actually replaces the old widget instead of leaking it."""
        old = table.cellWidget(row, col)
        if old is not None:
            old.deleteLater()
        table.removeCellWidget(row, col)
        table.setCellWidget(row, col, widget)

    @staticmethod
    def _clear_cell_widget(table: QTableWidget, row: int, col: int) -> None:
        old = table.cellWidget(row, col)
        if old is not None:
            old.deleteLater()
            table.removeCellWidget(row, col)

    def _fill_shared_row(self, table: QTableWidget, row: int, node: dict) -> None:
        show_times = self.is_admin
        values = [None] * len(DEVICE_COLUMNS)
        values[STT_COLUMN] = row + 1  # 1-based, so the row count doubles as a running total
        values[UID_COLUMN] = node.get("uid", "")
        values[DEVICE_COLUMNS.index("Alias")] = node.get("alias", "")
        values[DEVICE_COLUMNS.index("Device ID")] = node.get("deviceId", "")
        values[MAC_COLUMN] = mac_to_decimal(node.get("mac", ""))
        values[DEVICE_COLUMNS.index("LID")] = node.get("lid", "")
        values[REMAIN_COLUMN] = node.get("remain", "")
        values[DEVICE_COLUMNS.index("Status")] = node.get("protocolStatus", "")
        values[DEVICE_COLUMNS.index("NOD")] = node.get("numberOfDevices", "")
        values[LAST_SEEN_COLUMN] = round((node.get("lastSeenAgoMs") or 0) / 1000, 1)
        for col, value in enumerate(values):
            if col == STATE_COLUMN:
                self._set_cell_widget(
                    table, row, col, _centered(theme.status_pill(theme.effective_response_state(node)))
                )
                continue
            if not show_times and col in (REMAIN_COLUMN, LAST_SEEN_COLUMN):
                bar = MeterBar(min_width=70)
                bar.setFixedWidth(70)
                if col == REMAIN_COLUMN:
                    total = node.get("_licenseTotalMinutes")
                    fraction = license_fraction(node.get("remain"), total)
                    bar.set_level(license_level(node.get("remain"), total), theme.countdown_color(fraction * 100))
                else:
                    fresh = freshness_level(node.get("lastSeenAgoMs"))
                    bar.set_level(fresh, freshness_color(fresh))
                table.setItem(row, col, QTableWidgetItem(""))
                self._set_cell_widget(table, row, col, _centered(bar))
                continue
            self._clear_cell_widget(table, row, col)  # a gauge left over from a non-admin render
            item = QTableWidgetItem(str(value))
            item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            if col == UID_COLUMN:
                item.setData(Qt.ItemDataRole.UserRole, node.get("mac"))
            if col == MAC_COLUMN:
                item.setToolTip(node.get("mac", ""))
            table.setItem(row, col, item)

    def _refresh_devices_view(self) -> None:
        checked_macs = {mac for mac, card in self.device_cards.items() if card.is_checked()}

        # Remember everything currently live. In the flat "Tất cả thiết bị"
        # scope, also append brand-new devices to the end of the grid -
        # slots are only ever removed by an explicit Remove or Clear (see
        # those handlers), never here, so the layout survives a re-Scan
        # even for devices that didn't answer it. A Group-scoped grid does
        # NOT auto-adopt new devices this way: its membership only ever
        # changes through the Group itself (Groups page / _set_grid_scope),
        # so an unrelated device coming online elsewhere can't leak in.
        for mac, node in self.devices.items():
            self.last_known[mac] = node
        if self.current_scope == ALL_DEVICES_SCOPE:
            for mac in self.devices:
                if mac not in self.grid_order:
                    self.grid_order.append(mac)
        self.layouts.setdefault(self.current_scope, {})["grid"] = self.grid_order
        _save_layouts(self.layouts)

        # mac -> the Group it currently belongs to *in this app's own data*.
        # Used to disambiguate same-numbered aliases on the flat page (see
        # DeviceCard._render_badge) with the group's up-to-date name, rather
        # than the node's own self-reported "nodeGroup" - that field only
        # updates when Set Matrix is next sent, so right after renaming a
        # Group it would still show the old name even though nothing about
        # the device itself changed.
        mac_to_group_name = {
            mac: g["name"] for g in self.groups for mac in g.get("members", [])
        }

        # A slot with no live node renders from its last-known snapshot (or a
        # bare placeholder if we've never actually seen it — e.g. right after
        # startup, before any device connects), forced to OFFLINE so its
        # status pill is red. It does not vanish.
        # `None` is a deliberately-vacated slot (see
        # DeviceCard.emptySlotRequested/EmptySlotCard) - passed straight
        # through, no device data to attach to it.
        nodes: list[dict | None] = []
        for mac in self.grid_order:
            if mac is None:
                nodes.append(None)
                continue
            live = self.devices.get(mac)
            if live is not None:
                node = dict(live)
            else:
                node = dict(self.last_known.get(mac) or {"mac": mac, "uid": mac})
                node["responseState"] = "NO_RESPONSE"
            # The card shows "License: <remain> / <set>", but only this app
            # knows the duration it last Set (the protocol only reports what
            # is left) — hand it to the card via the node dict.
            node["_licenseTotalMinutes"] = self.license_totals.get(mac)
            node["_currentGroupName"] = mac_to_group_name.get(mac)
            node["_showTimes"] = self.is_admin
            nodes.append(node)

        group = self._group_for_scope(self.current_scope)
        self.device_cards = {}
        # "Position" only means something once a Group is picked (its own
        # matrix/location) - the flat "Tất cả thiết bị" view has no single
        # physical layout to compare a card's slot against.
        self.grid_list.set_nodes(
            nodes,
            show_position=group is not None,
            expected_group=group["name"] if group is not None else None,
        )
        for mac in self.grid_order:
            if mac is None:
                continue
            card = self.grid_list.card_for(mac)
            if card is None:
                continue
            card.set_checked(mac in checked_macs)
            self.device_cards[mac] = card
        self.grid_list.sync_row_checkboxes()
        # Ticked set can change without a click (scope switch rebuilds the
        # cards) - reconcile which boards are blinking; a no-op if unchanged.
        self._identify_debounce.start()

        online, total = self._grid_online_total()
        self.device_count_label.setText(f"{online} / {total} online")
        self.device_count_label.setVisible(True)
        self.grid_count_label.setText(f"{total} thiết bị · {total - online} offline")
        self._update_last_set_license_label()

    def _refresh_group_list(self) -> None:
        current_id = None
        if self.group_list.currentItem() is not None:
            current_id = self.group_list.currentItem().data(Qt.ItemDataRole.UserRole)
        self.group_list.blockSignals(True)
        self.group_list.clear()
        for group in self.groups:
            item = QListWidgetItem(f"{group['name']} ({group.get('memberCount', 0)})")
            item.setData(Qt.ItemDataRole.UserRole, group["id"])
            self.group_list.addItem(item)
            if group["id"] == current_id:
                self.group_list.setCurrentItem(item)
        self.group_list.blockSignals(False)
        self._prune_orphan_layouts()
        self._refresh_group_members()
        self._refresh_grid_scope_combo()

    def _on_group_selected(self, *_args) -> None:
        self._refresh_group_members()

    def _refresh_group_members(self) -> None:
        group = self.selected_group()
        if group is None:
            self.group_member_table.setRowCount(0)
            return
        # group["devices"] only carries full node info for members the Hub
        # doesn't currently have live (see appendGroup() in local_web.h) -
        # sending it for every member too used to double an already-large
        # payload and could silently truncate state.get on this board (no
        # PSRAM). A member that IS live has its full info in self.devices
        # already (same mac), so fall back to that first, same pattern as
        # _refresh_devices_view()'s last_known fallback.
        backfill = {d.get("mac"): d for d in group.get("devices", []) if d.get("mac")}
        members = [
            dict(self.devices.get(mac) or backfill.get(mac) or {"mac": mac, "uid": mac, "responseState": "NO_RESPONSE"})
            for mac in group.get("members", [])
        ]
        for node in members:
            node["_licenseTotalMinutes"] = self.license_totals.get(node.get("mac"))
        headers = list(DEVICE_COLUMNS)
        if not self.is_admin:
            headers[REMAIN_COLUMN] = "Trạng thái"  # not "Status": that column already exists (protocolStatus)
            headers[LAST_SEEN_COLUMN] = "Tín hiệu"
        self.group_member_table.setHorizontalHeaderLabels(headers)
        self.group_member_table.setRowCount(len(members))
        for row, node in enumerate(members):
            self._fill_shared_row(self.group_member_table, row, node)
        self.group_member_table.resizeColumnsToContents()
        # resizeColumnsToContents() re-measures every column regardless of its
        # resize mode, so the Fixed-width columns from _configure_table_columns
        # need pinning back afterward.
        self.group_member_table.setColumnWidth(STT_COLUMN, 48)
        self.group_member_table.setColumnWidth(STATE_COLUMN, 120)

    def _refresh_account_table(self) -> None:
        self.account_table.setRowCount(len(self.accounts))
        for row, account in enumerate(self.accounts):
            self.account_table.setItem(row, 0, QTableWidgetItem(account.get("username", "")))
            enabled = bool(account.get("twoFactorEnabled"))
            badge = QLabel("Enabled" if enabled else "Disabled")
            badge.setAlignment(Qt.AlignmentFlag.AlignCenter)
            color, bg = theme.STATUS_COLORS["ONLINE"] if enabled else theme.STATUS_COLORS["UNKNOWN"]
            badge.setStyleSheet(
                f"color:{color}; background:{bg}; border-radius:9px; padding:3px 9px;"
                f"font-size:11px; font-weight:800;"
            )
            self.account_table.setCellWidget(row, 1, _centered(badge))
            current_label = " (current)" if account.get("username") == self.username else ""
            self.account_table.setItem(row, 2, QTableWidgetItem(current_label.strip()))
        self.account_table.resizeColumnsToContents()

    # ----------------------------------------------------------- actions
    def _open_device_detail(self, mac: str) -> None:
        node = self.devices.get(mac)
        if node is None:
            # Offline slot — show its last-known snapshot, marked offline.
            last = self.last_known.get(mac)
            if last is None:
                return
            node = dict(last)
            node["responseState"] = "NO_RESPONSE"
        dialog = DeviceDetailDialog(
            node,
            on_get_license=lambda: self._action_get_license_devices([mac]),
            on_get_info=lambda: self._action_get_info_devices([mac]),
            on_set_license=lambda: self._action_set_license_devices([mac]),
            on_config=lambda: self._action_config_device([mac]),
            on_delete=lambda: self._action_delete_devices([mac]),
            on_set_card_type=lambda: self._action_set_card_type([mac]),
            on_alias_save=lambda alias: self._save_alias(mac, alias),
            total_minutes=self.license_totals.get(mac),
            show_times=self.is_admin,
            parent=self,
        )
        # Tracked so _handle_device_upsert can push live device.upsert
        # updates (e.g. a Get Info/Get License reply) straight into this
        # still-open dialog via refresh(), instead of the user having to
        # close and reopen it to see the result.
        self._open_detail_mac = mac
        self._open_detail_dialog = dialog
        if mac in self.devices:
            # Auto-refresh on open so the dialog always shows current
            # firmware/voltage/temp/uptime without the operator having to
            # press Get Info first - the reply lands via the device.upsert
            # hook above. (Not also auto-firing Get License here: the master
            # only runs one ESP-NOW job at a time, and queuing both back to
            # back on open would make the second one bounce off "Master is
            # running another operation".)
            self._action_get_info_devices([mac])
        try:
            dialog.exec()
        finally:
            self._open_detail_mac = None
            self._open_detail_dialog = None

    def _save_alias(self, mac: str, alias: str) -> None:
        if alias:
            self._send("alias.save", {"mac": mac, "alias": alias})
        else:
            self._send("alias.delete", {"mac": mac})

    def _action_scan(self) -> None:
        self._send("scan.start", {"limit": self.scan_limit.value()})

    def _on_auto_refresh_timeout(self) -> None:
        if self.busy:
            # Hub would just answer BUSY anyway (see webLicenseJobBusy() in
            # local_web.h) - skip this tick rather than spam the log with an
            # error for something the next timer tick will retry regardless.
            return
        if self._auto_refresh_next_is_info:
            macs = list(self.devices.keys())
            if macs:
                self._send("device.getInfo", {"targetType": "devices", "members": macs})
        else:
            self._send("scan.refresh")
        self._auto_refresh_next_is_info = not self._auto_refresh_next_is_info

    def _action_clear_all(self) -> None:
        if QMessageBox.question(
            self, "Xác nhận",
            "Xoá toàn bộ danh sách thiết bị và bố cục ma trận đã lưu?",
        ) != QMessageBox.StandardButton.Yes:
            return
        self._pending_layout_reset = True
        self._send("devices.clear")

    def _action_search(self) -> None:
        uid = self.search_edit.text().strip()
        if not uid:
            return
        self._send("device.search", {"deviceId": uid})

    def _action_get_license_devices(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if not macs:
            QMessageBox.warning(self, "Chưa chọn", "Chọn ít nhất 1 thiết bị (tick chọn trên card)")
            return
        self._send("license.get", {"targetType": "devices", "members": macs})

    def _action_get_info_devices(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if not macs:
            QMessageBox.warning(self, "Chưa chọn", "Chọn ít nhất 1 thiết bị (tick chọn trên card)")
            return
        self._send("device.getInfo", {"targetType": "devices", "members": macs})

    def _update_last_set_license_label(self) -> None:
        """Caption under the Devices page's "Set License" button — the most
        recent time (this app's own clock) any Set License command was sent
        to a device in the CURRENTLY VIEWED scope (self.grid_order — every
        device for "Tất cả thiết bị", just that Group's members otherwise).
        Scoped rather than global so switching from a Group Set at 10h to
        one Set at 11h shows that Group's own time, not whichever was set
        most recently across every Group."""
        self.last_set_license_label.setVisible(self.is_admin)
        times = [self.license_set_at[mac] for mac in self.grid_order if mac in self.license_set_at]
        if times:
            self.last_set_license_label.setText(max(times))
        else:
            self.last_set_license_label.setText("Chưa Set License")

    def _action_set_license_devices(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if not macs:
            QMessageBox.warning(self, "Chưa chọn", "Chọn ít nhất 1 thiết bị (tick chọn trên card)")
            return
        last_led = _load_last_expired_led_settings()
        dialog = LicenseSetDialog(
            f"{len(macs)} thiết bị", self,
            preset_mode=last_led["mode"], preset_cycle_min_minutes=last_led["cycleMin"],
            preset_cycle_max_minutes=last_led["cycleMax"],
        )
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        duration = dialog.duration.value()
        expired_led_mode = dialog.expired_led_mode.currentData()
        cycle_min = dialog.cycle_min_minutes()
        cycle_max = dialog.cycle_max_minutes()
        self._send("license.set", {
            "targetType": "devices",
            "members": macs,
            "lid": dialog.lid.value(),
            "durationMinutes": duration,
            "expired": 1 if dialog.expired.isChecked() else 0,
            "expiredLedMode": expired_led_mode,
            "expiredCycleMinMinutes": cycle_min,
            "expiredCycleMaxMinutes": cycle_max,
        })
        _save_last_expired_led_settings(expired_led_mode, cycle_min, cycle_max)
        # Remember what we asked for so DeviceDetailDialog's countdown bar
        # has a "total" to measure the reported remain against later — the
        # protocol itself never echoes the original duration back.
        set_at = time.strftime("%Y-%m-%d %H:%M:%S")
        for mac in macs:
            self.license_totals[mac] = duration
            self.license_set_at[mac] = set_at
        _save_license_totals(self.license_totals)
        _save_license_set_at(self.license_set_at)
        self._update_last_set_license_label()

    def _action_config_device(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if len(macs) != 1:
            QMessageBox.warning(self, "Chưa hợp lệ", "Chọn đúng 1 thiết bị để Config")
            return
        dialog = ConfigDeviceDialog(macs[0], self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        new_lid = dialog.new_lid.text().strip()
        if not new_lid:
            QMessageBox.warning(self, "Chưa hợp lệ", "LID mới không được để trống")
            return
        self._send("device.config", {"targetType": "devices", "members": macs, "new_lid": new_lid})

    def _action_set_card_type(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if not macs:
            QMessageBox.warning(self, "Chưa chọn", "Chọn ít nhất 1 thiết bị (tick chọn trên card)")
            return
        dialog = CardTypeDialog(f"{len(macs)} thiết bị", self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        card_type = dialog.selected_card_type()
        self._send("cardtype.set", {
            "members": [{"mac": mac, "cardType": card_type} for mac in macs],
        })

    def _action_delete_devices(self, macs: list[str] | None = None) -> None:
        macs = macs if macs is not None else self.selected_device_macs()
        if not macs:
            return
        if QMessageBox.question(self, "Xác nhận", f"Xoá {len(macs)} thiết bị khỏi danh sách?") != QMessageBox.StandardButton.Yes:
            return
        stale = [m for m in macs if m not in self.devices]
        for mac in macs:
            if mac not in stale:
                self._send("node.delete", {"mac": mac})
        # Offline slots aren't in the hub's list, so it won't echo a
        # device.deleted for them — drop them from the matrix here.
        if stale:
            for mac in stale:
                self.last_known.pop(mac, None)
                if mac in self.grid_order:
                    self.grid_order.remove(mac)
                for layout in self.layouts.values():
                    grid = layout.get("grid")
                    if isinstance(grid, list) and mac in grid:
                        grid.remove(mac)
            _save_layouts(self.layouts)
            self._refresh_devices_view()
            self._update_stat_cards()

    def _macs_in_other_groups(self, exclude_group_id=None) -> set[str]:
        """Every mac that already belongs to some Group other than
        exclude_group_id — a board is meant to live in one location/group
        at a time, so hiding these from the member picker keeps the list to
        just what's actually still available to assign."""
        macs: set[str] = set()
        for group in self.groups:
            if group["id"] == exclude_group_id:
                continue
            macs.update(group.get("members", []))
        return macs

    def _action_new_group(self) -> None:
        taken = self._macs_in_other_groups()
        candidates = {
            mac: node_label(node) for mac, node in self.devices.items() if mac not in taken
        }
        dialog = GroupEditDialog("", candidates, set(), self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        name = dialog.name.text().strip()
        if not name:
            QMessageBox.warning(self, "Chưa hợp lệ", "Tên group không được để trống")
            return
        self._send("group.save", {"id": 0, "name": name, "members": dialog.selected_macs()})

    def _action_edit_group(self) -> None:
        group = self.selected_group()
        if group is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 group để sửa")
            return
        taken = self._macs_in_other_groups(exclude_group_id=group["id"])
        candidates = {
            mac: node_label(node) for mac, node in self.devices.items() if mac not in taken
        }
        for member in group.get("devices", []):
            mac = member.get("mac")
            if mac and mac not in taken and mac not in candidates:
                candidates[mac] = node_label(member)
        dialog = GroupEditDialog(group["name"], candidates, set(group.get("members", [])), self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        name = dialog.name.text().strip()
        if not name:
            QMessageBox.warning(self, "Chưa hợp lệ", "Tên group không được để trống")
            return
        self._send("group.save", {"id": group["id"], "name": name, "members": dialog.selected_macs()})

    def _action_delete_group(self) -> None:
        group = self.selected_group()
        if group is None:
            return
        if QMessageBox.question(self, "Xác nhận", f"Xoá group '{group['name']}'?") != QMessageBox.StandardButton.Yes:
            return
        self._send("group.delete", {"id": group["id"]})

    def _action_get_license_group(self) -> None:
        group = self.selected_group()
        if group is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 group")
            return
        self._send("license.get", {"targetType": "group", "groupId": group["id"]})

    def _action_get_info_group(self) -> None:
        group = self.selected_group()
        if group is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 group")
            return
        self._send("device.getInfo", {"targetType": "group", "groupId": group["id"]})

    def _action_set_license_group(self) -> None:
        group = self.selected_group()
        if group is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 group")
            return
        last_led = _load_last_expired_led_settings()
        dialog = LicenseSetDialog(
            group["name"], self,
            preset_mode=last_led["mode"], preset_cycle_min_minutes=last_led["cycleMin"],
            preset_cycle_max_minutes=last_led["cycleMax"],
        )
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        duration = dialog.duration.value()
        expired_led_mode = dialog.expired_led_mode.currentData()
        cycle_min = dialog.cycle_min_minutes()
        cycle_max = dialog.cycle_max_minutes()
        self._send("license.set", {
            "targetType": "group",
            "groupId": group["id"],
            "lid": dialog.lid.value(),
            "durationMinutes": duration,
            "expired": 1 if dialog.expired.isChecked() else 0,
            "expiredLedMode": expired_led_mode,
            "expiredCycleMinMinutes": cycle_min,
            "expiredCycleMaxMinutes": cycle_max,
        })
        _save_last_expired_led_settings(expired_led_mode, cycle_min, cycle_max)
        set_at = time.strftime("%Y-%m-%d %H:%M:%S")
        for mac in group.get("members", []):
            self.license_totals[mac] = duration
            self.license_set_at[mac] = set_at
        _save_license_totals(self.license_totals)
        _save_license_set_at(self.license_set_at)
        self._update_last_set_license_label()

    def _action_create_account(self) -> None:
        dialog = CreateAccountDialog(self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        username = dialog.username.text().strip()
        password = dialog.password.text()
        if not username or len(password) < 6:
            QMessageBox.warning(self, "Chưa hợp lệ", "Tài khoản hợp lệ và mật khẩu >= 6 ký tự")
            return
        self._send("account.create", {"username": username, "password": password})

    def _action_change_password(self) -> None:
        account = self.selected_account()
        if account is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 tài khoản")
            return
        dialog = ChangePasswordDialog(account["username"], self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        password = dialog.password.text()
        if len(password) < 6:
            QMessageBox.warning(self, "Chưa hợp lệ", "Mật khẩu phải >= 6 ký tự")
            return
        self._send("account.password", {"username": account["username"], "password": password})

    def _action_delete_account(self) -> None:
        account = self.selected_account()
        if account is None:
            return
        if account["username"] == self.username:
            QMessageBox.warning(self, "Không thể xoá", "Không thể xoá tài khoản đang đăng nhập")
            return
        if QMessageBox.question(self, "Xác nhận", f"Xoá tài khoản '{account['username']}'?") != QMessageBox.StandardButton.Yes:
            return
        self._send("account.delete", {"username": account["username"]})

    def _action_toggle_2fa(self) -> None:
        account = self.selected_account()
        if account is None:
            QMessageBox.warning(self, "Chưa chọn", "Chọn 1 tài khoản")
            return
        if account.get("twoFactorEnabled"):
            if QMessageBox.question(self, "Xác nhận", f"Tắt 2FA cho '{account['username']}'?") != QMessageBox.StandardButton.Yes:
                return
            self._send("account.2fa.disable", {"username": account["username"]})
        else:
            if account["username"] != self.username:
                QMessageBox.warning(self, "Không hỗ trợ", "Chỉ có thể bật 2FA cho tài khoản đang đăng nhập")
                return
            self._send("account.2fa.setup", {})
