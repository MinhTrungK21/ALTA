from __future__ import annotations

from PySide6.QtCore import QEvent, Qt, QTime, QTimer, Signal
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QProgressBar,
    QPushButton,
    QScrollArea,
    QSpinBox,
    QStyle,
    QTimeEdit,
    QVBoxLayout,
    QWidget,
)

import theme
from meters import MeterBar, freshness_color, freshness_level, license_level
from device_card import board_pixmap
from qr import qr_pixmap


class _ParentCenteredDialog(QDialog):
    """QDialog that opens centered over its parent window, on that window's
    own screen. Left to the window manager, a dialog on a multi-monitor
    setup can be placed on a different monitor than the main window (seen
    when creating a Group: the member picker popped up on the other
    screen), so pin it to the parent's screen explicitly - once when shown,
    and again shortly after in case the window manager relocates it once
    it's already mapped (which a one-time move in showEvent can't catch)."""

    def showEvent(self, event) -> None:  # noqa: N802 (Qt override)
        super().showEvent(event)
        self._center_on_parent_screen(force=True)
        for delay_ms in (0, 120, 400):
            QTimer.singleShot(delay_ms, lambda: self._center_on_parent_screen(force=False))

    def _center_on_parent_screen(self, *, force: bool) -> None:
        parent = self.parentWidget()
        if parent is None or not self.isVisible():
            return
        anchor = parent.window()
        screen = anchor.screen()
        if screen is None:
            return
        handle = self.windowHandle()
        if handle is not None and handle.screen() is not screen:
            handle.setScreen(screen)
        avail = screen.availableGeometry()
        geo = self.frameGeometry()
        # Later passes only step in if the dialog actually ended up off the
        # parent's screen - never fight a user who dragged it elsewhere on it.
        if not force and avail.contains(geo.center()):
            return
        geo.moveCenter(anchor.frameGeometry().center())
        geo.moveLeft(max(avail.left(), min(geo.left(), avail.right() - geo.width() + 1)))
        geo.moveTop(max(avail.top(), min(geo.top(), avail.bottom() - geo.height() + 1)))
        self.move(geo.topLeft())


class LoginDialog(_ParentCenteredDialog):
    """Username/password step. Call show_otp() if the server asks for a code.

    The OK button does not close the dialog by itself: the caller decides
    (via accept()) once the server confirms login, since one submit may need
    to be followed by an OTP prompt instead of finishing.
    """

    submit = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Đăng nhập TWT Led Control System")
        self.setModal(True)
        layout = QFormLayout(self)

        self.username = QLineEdit()
        self.password = QLineEdit()
        self.password.setEchoMode(QLineEdit.EchoMode.Password)
        layout.addRow("Tài khoản:", self.username)
        layout.addRow("Mật khẩu:", self.password)

        self.otp_label = QLabel("Mã xác thực 2FA (6 số):")
        self.otp = QLineEdit()
        self.otp.setMaxLength(6)
        self.otp_label.hide()
        self.otp.hide()
        layout.addRow(self.otp_label, self.otp)

        self.error_label = QLabel("")
        self.error_label.setStyleSheet(f"color: {theme.RED};")
        layout.addRow(self.error_label)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.submit.emit)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)
        self.username.setFocus()

    def credentials(self) -> tuple[str, str]:
        return self.username.text().strip(), self.password.text()

    def otp_code(self) -> str:
        return self.otp.text().strip()

    def show_otp(self) -> None:
        self.username.setEnabled(False)
        self.password.setEnabled(False)
        self.otp_label.show()
        self.otp.show()
        self.otp.setFocus()

    def set_error(self, message: str) -> None:
        self.error_label.setText(message)

    def waiting_for_otp(self) -> bool:
        return self.otp.isVisible()


class TwoFactorSetupDialog(_ParentCenteredDialog):
    """Shows the QR / secret returned by account.2fa.setup and asks for a code.

    Like LoginDialog, OK does not close the dialog directly — the caller
    calls accept() once account.2fa.confirm is acknowledged by the server.
    """

    submit = Signal()

    def __init__(self, secret: str, qr_size: int, qr_hex: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Bật xác thực 2 lớp (2FA)")
        self.setModal(True)
        layout = QVBoxLayout(self)

        layout.addWidget(QLabel("Quét mã QR bằng ứng dụng Authenticator, hoặc nhập thủ công:"))
        qr_label = QLabel()
        if qr_size:
            qr_label.setPixmap(qr_pixmap(qr_size, qr_hex))
        layout.addWidget(qr_label, alignment=Qt.AlignmentFlag.AlignHCenter)

        secret_label = QLabel(secret)
        secret_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        secret_label.setStyleSheet("font-family: monospace; font-size: 14px;")
        layout.addWidget(secret_label, alignment=Qt.AlignmentFlag.AlignHCenter)

        form = QFormLayout()
        self.code = QLineEdit()
        self.code.setMaxLength(6)
        form.addRow("Mã 6 số:", self.code)
        layout.addLayout(form)

        self.error_label = QLabel("")
        self.error_label.setStyleSheet(f"color: {theme.RED};")
        layout.addWidget(self.error_label)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.submit.emit)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def set_error(self, message: str) -> None:
        self.error_label.setText(message)


class CreateAccountDialog(_ParentCenteredDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Tạo tài khoản")
        self.setModal(True)
        layout = QFormLayout(self)
        self.username = QLineEdit()
        self.password = QLineEdit()
        self.password.setEchoMode(QLineEdit.EchoMode.Password)
        layout.addRow("Tài khoản:", self.username)
        layout.addRow("Mật khẩu (>= 6 ký tự):", self.password)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)


class ChangePasswordDialog(_ParentCenteredDialog):
    def __init__(self, username: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Đổi mật khẩu: {username}")
        self.setModal(True)
        layout = QFormLayout(self)
        self.password = QLineEdit()
        self.password.setEchoMode(QLineEdit.EchoMode.Password)
        layout.addRow("Mật khẩu mới (>= 6 ký tự):", self.password)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)


class LicenseSetDialog(_ParentCenteredDialog):
    def __init__(
        self, target_label: str, parent=None, *,
        preset_mode: int = 0, preset_cycle_min_minutes: int = 60, preset_cycle_max_minutes: int = 630,
    ):
        super().__init__(parent)
        self.setWindowTitle(f"Set License — {target_label}")
        self.setModal(True)
        layout = QFormLayout(self)

        self.lid = QSpinBox()
        self.lid.setRange(0, 999999)
        self.lid.setSpecialValueText("(giữ LID hiện tại của Node)")
        layout.addRow("LID mới (0 = giữ nguyên):", self.lid)

        self.duration = QSpinBox()
        self.duration.setRange(0, 10_000_000)
        self.duration.setValue(60)
        layout.addRow("Thời hạn (phút):", self.duration)

        self.expired = QCheckBox("Đánh dấu hết hạn ngay")
        layout.addRow(self.expired)

        # Kieu LED bao loi khi het han - node tu chay (xem
        # rcv/led_display.h), gui kem cung Set License. Node cu chua co
        # tinh nang nay se bo qua truong nay va giu che do dang chay.
        # Dialog nay luon doi lai thoi han/LID moi (khong "nho" gia tri cu la
        # dung y), nhung mode LED thi khac - vao lai de chi gia han thoi
        # gian ma quen doi mode se vo tinh reset ve "Ngau nhien" mac dinh
        # neu khong nho lai lua chon lan truoc, nen dialog nay duoc truyen
        # san preset_mode/preset_cycle_* tu lan Set License gan nhat.
        self.expired_led_mode = QComboBox()
        self.expired_led_mode.addItem("Ngẫu nhiên (nhiều kiểu, đổi liên tục)", 0)
        self.expired_led_mode.addItem("Tắt toàn bộ", 1)
        self.expired_led_mode.addItem("Nhấp nháy đều, chu kỳ 2 giây", 2)
        self.expired_led_mode.addItem("Ngẫu nhiên theo chu kỳ (thỉnh thoảng lỗi)", 3)
        preset_index = self.expired_led_mode.findData(preset_mode)
        self.expired_led_mode.setCurrentIndex(preset_index if preset_index >= 0 else 0)
        layout.addRow("Mode khi hết license:", self.expired_led_mode)

        # Chi dung khi chon mode 3 (Random theo chu ky): khoang thoi gian
        # "bình thường" giữa 2 đợt lỗi - node tự bốc 1 mốc ngẫu nhiên trong
        # khoảng này mỗi lần (xem rcv/led_display.h::scheduleNextQuietPhase()).
        self.cycle_min = QTimeEdit(QTime(preset_cycle_min_minutes // 60, preset_cycle_min_minutes % 60))
        self.cycle_min.setDisplayFormat("H:mm")
        self.cycle_max = QTimeEdit(QTime(preset_cycle_max_minutes // 60, preset_cycle_max_minutes % 60))
        self.cycle_max.setDisplayFormat("H:mm")
        layout.addRow("Từ (giờ:phút):", self.cycle_min)
        layout.addRow("Đến (giờ:phút):", self.cycle_max)

        def _update_cycle_rows_visibility() -> None:
            is_cycle_mode = self.expired_led_mode.currentData() == 3
            for widget in (self.cycle_min, self.cycle_max):
                widget.setVisible(is_cycle_mode)
            for label in (layout.labelForField(self.cycle_min), layout.labelForField(self.cycle_max)):
                if label is not None:
                    label.setVisible(is_cycle_mode)

        self.expired_led_mode.currentIndexChanged.connect(_update_cycle_rows_visibility)
        _update_cycle_rows_visibility()

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)

    def cycle_min_minutes(self) -> int:
        t = self.cycle_min.time()
        return t.hour() * 60 + t.minute()

    def cycle_max_minutes(self) -> int:
        t = self.cycle_max.time()
        return t.hour() * 60 + t.minute()


class ConfigDeviceDialog(_ParentCenteredDialog):
    def __init__(self, target_label: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Config Device — {target_label}")
        self.setModal(True)
        layout = QFormLayout(self)
        self.new_lid = QLineEdit()
        self.new_lid.setMaxLength(11)
        layout.addRow("LID mới (1-11 ký tự):", self.new_lid)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)


class CardTypeDialog(_ParentCenteredDialog):
    """Picks the physical board variant to write down to node(s) — see Set
    Card Type in local_web.h. The node persists and echoes it back on later
    scans/Get Info, so the app can pick the right board photo (removable-
    card vs. onboard-card) without asking the operator again."""

    CARD_R = "R"
    CARD_OB = "OB"

    def __init__(self, target_label: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Set Card Type — {target_label}")
        self.setModal(True)
        layout = QFormLayout(self)
        self.card_type = QComboBox()
        self.card_type.addItem("Card rời (Card_R)", self.CARD_R)
        self.card_type.addItem("Card onboard (Card_OB)", self.CARD_OB)
        layout.addRow("Loại board:", self.card_type)
        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)

    def selected_card_type(self) -> str:
        return self.card_type.currentData()


class GroupEditDialog(_ParentCenteredDialog):
    """Pick group name + members (by MAC) from a candidate {mac: label} map."""

    def __init__(self, name: str, candidates: dict[str, str], selected_macs: set[str], parent=None):
        super().__init__(parent)
        self.setWindowTitle("Group")
        self.setModal(True)
        self.resize(420, 480)
        layout = QVBoxLayout(self)

        form = QFormLayout()
        self.name = QLineEdit(name)
        form.addRow("Tên group:", self.name)
        layout.addLayout(form)

        layout.addWidget(QLabel("Thành viên (tick chọn):"))
        self.list = QListWidget()
        for mac, label in sorted(candidates.items(), key=lambda kv: kv[1].lower()):
            item = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, mac)
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked if mac in selected_macs else Qt.CheckState.Unchecked)
            self.list.addItem(item)
        # Qt only toggles a checkable item's box when the click lands
        # precisely on the tiny indicator glyph; clicking the device name
        # next to it (the natural thing to click) does nothing by default.
        # Handle clicks on the rest of the row ourselves so "add a device"
        # just means clicking it anywhere. This needs the click's actual
        # position (from the mouse event itself, via an event filter on the
        # viewport) rather than itemClicked + QCursor.pos(): under some
        # platforms/timings QCursor.pos() doesn't reliably reflect where the
        # click happened, which made this toggle on the wrong clicks (and,
        # worse, double-toggle - cancelling back out - on a click that
        # landed right on the indicator, which Qt's own handling already
        # toggles by itself) - exactly the "tick lag, works sometimes" bug.
        self.list.viewport().installEventFilter(self)
        layout.addWidget(self.list)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def eventFilter(self, obj, event) -> bool:  # noqa: N802 (Qt override)
        if (
            obj is self.list.viewport()
            and event.type() == QEvent.Type.MouseButtonRelease
            and event.button() == Qt.MouseButton.LeftButton
        ):
            item = self.list.itemAt(event.pos())
            if item is not None:
                rect = self.list.visualItemRect(item)
                indicator_width = QApplication.style().pixelMetric(QStyle.PixelMetric.PM_IndicatorWidth)
                # A click within the indicator's own column is already
                # handled by Qt's built-in toggle - only step in past it.
                if event.pos().x() - rect.x() >= indicator_width + 10:
                    item.setCheckState(
                        Qt.CheckState.Unchecked if item.checkState() == Qt.CheckState.Checked
                        else Qt.CheckState.Checked
                    )
        return super().eventFilter(obj, event)

    def selected_macs(self) -> list[str]:
        macs = []
        for i in range(self.list.count()):
            item = self.list.item(i)
            if item.checkState() == Qt.CheckState.Checked:
                macs.append(item.data(Qt.ItemDataRole.UserRole))
        return macs


def mac_to_decimal(mac: str) -> str:
    """Render a colon-hex MAC ("50:78:7D:18:1B:D8") as per-byte decimal
    ("80.120.125.24.27.216") for display. Purely cosmetic — the hex form is
    still what identifies the device everywhere else (dict keys, what gets
    sent back to the hub in action payloads); only call this at the point
    text is shown to the user, never on a value used to address a device.
    """
    parts = mac.split(":")
    try:
        return ".".join(str(int(part, 16)) for part in parts)
    except ValueError:
        return mac  # not a well-formed "XX:XX:..." MAC — show as-is


def _stat_tile(label: str, value: str, *, variant: str = "orange", monospace: bool = False) -> QFrame:
    bg, accent = theme.TILE_COLORS.get(variant, theme.TILE_COLORS["orange"])
    tile = QFrame()
    tile.setStyleSheet(f"QFrame {{ background:{bg}; border:1px solid {bg}; border-radius:14px; }}")
    tile_layout = QVBoxLayout(tile)
    tile_layout.setContentsMargins(10, 6, 10, 6)
    tile_layout.setSpacing(1)
    label_widget = QLabel(label.upper())
    label_widget.setStyleSheet(f"font-size:10px; font-weight:800; letter-spacing:.04em; color:{accent};")
    tile_layout.addWidget(label_widget)
    value_widget = QLabel(value or "—")
    value_widget.setWordWrap(True)
    family = "font-family:'Consolas','Menlo',monospace; " if monospace else ""
    value_widget.setStyleSheet(f"{family}font-size:13px; font-weight:800; color:{theme.INK};")
    tile_layout.addWidget(value_widget)
    return tile


def _meter_tile(label: str, *, variant: str, level: int | None = None, color: str | None = None) -> QFrame:
    """Same footprint as _stat_tile, with a segmented gauge where the value
    would be — for accounts that may not see exact times."""
    tile = _stat_tile(label, "", variant=variant)
    value_widget = tile.layout().itemAt(1).widget()
    value_widget.hide()
    bar = MeterBar()
    bar.set_level(level or 0, color or theme.TILE_COLORS.get(variant, theme.TILE_COLORS["orange"])[1])
    bar.setFixedHeight(value_widget.sizeHint().height())  # keep the tile as tall as a numeric one
    tile.layout().addWidget(bar)
    return tile


def _stat_grid(node: dict, show_times: bool = True) -> QWidget:
    """Device ID / MAC / LID / Status / NOD / Last seen as small labeled,
    color-coded tiles instead of a plain list of numbers."""
    container = QWidget()
    grid = QGridLayout(container)
    grid.setContentsMargins(0, 0, 0, 0)
    grid.setSpacing(8)
    last_seen = round((node.get("lastSeenAgoMs") or 0) / 1000, 1)
    mac_tile = _stat_tile("MAC", mac_to_decimal(node.get("mac", "")), variant="orange", monospace=True)
    mac_tile.setToolTip(node.get("mac", ""))
    # 4 columns so the whole panel takes 3 rows instead of 5 - the detail
    # dialog has to fit on one screen without scrolling.
    grid.addWidget(mac_tile, 0, 0, 1, 4)
    grid.addWidget(_stat_tile("Device ID", str(node.get("deviceId", "")), variant="blue"), 1, 0)
    grid.addWidget(_stat_tile("LID", str(node.get("lid", "")), variant="purple"), 1, 1)
    grid.addWidget(_stat_tile("Status", str(node.get("protocolStatus", "")), variant="teal"), 1, 2)
    grid.addWidget(_stat_tile("NOD", str(node.get("numberOfDevices", "")), variant="pink"), 1, 3)
    if show_times:
        seen_tile = _stat_tile("Lần cuối", f"{last_seen}s trước", variant="amber")
    else:
        fresh = freshness_level(node.get("lastSeenAgoMs"))
        seen_tile = _meter_tile("Tín hiệu", variant="amber", level=fresh, color=freshness_color(fresh))
    grid.addWidget(seen_tile, 2, 0, 1, 2)
    card_type_labels = {"OB": "Card onboard", "R": "Card rời"}
    card_type_value = card_type_labels.get(node.get("cardType"), "Chưa cấu hình")
    grid.addWidget(
        _stat_tile("Loại board", card_type_value, variant="teal"), 2, 2, 1, 2
    )
    for col in range(4):
        grid.setColumnStretch(col, 1)
    return container


def _fmt_uptime_full(minutes) -> str:
    """"x ngày y giờ z phút" — spelled out in full for the detail dialog's
    "Uptime" tile, unlike device_card.py's _fmt_uptime (a compact "2n 3h"
    for the small on-card chip, where space is tight)."""
    try:
        m = int(minutes or 0)
    except (TypeError, ValueError):
        m = 0
    days, rem = divmod(m, 24 * 60)
    hours, mins = divmod(rem, 60)
    return f"{days} ngày {hours} giờ {mins} phút"


def _info_grid(node: dict, show_times: bool = True) -> QWidget:
    """Firmware/voltage/temperature/uptime telemetry from the last LIC_INFO
    reply (see local_web.h::webOnInfoResponse()/appendNodeInfo()). These
    fields are only present in `node` once a Get Info request has actually
    gotten a reply for this device — appendNodeInfo() omits them entirely
    otherwise, rather than sending misleading 0.0V/0.0°C placeholders.
    """
    container = QWidget()
    layout = QVBoxLayout(container)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(8)

    if node.get("infoAgoMs") is None:
        hint = QLabel('Chưa có dữ liệu Info — bấm "Get Info" để đọc firmware/điện áp/nhiệt độ/uptime.')
        hint.setObjectName("CardSubtitle")
        hint.setWordWrap(True)
        layout.addWidget(hint)
        return container

    title = QLabel("Thông tin thiết bị (LIC_INFO)")
    title.setStyleSheet("font-weight:800; font-size:13px;")
    layout.addWidget(title)

    grid_widget = QWidget()
    grid = QGridLayout(grid_widget)
    grid.setContentsMargins(0, 0, 0, 0)
    grid.setSpacing(8)
    fw = node.get("firmwareVersion") or "—"
    grid.addWidget(_stat_tile("Firmware", fw, variant="purple"), 0, 0)
    protocol = node.get("linkProtocol") or "—"
    grid.addWidget(_stat_tile("Giao thức", protocol, variant="orange"), 0, 1)
    grid.addWidget(
        _stat_tile("Điện áp", f"{node.get('voltageV', 0):.2f} V", variant="blue"), 0, 2
    )
    grid.addWidget(
        _stat_tile("Nhiệt độ", f"{node.get('temperatureC', 0):.1f} °C", variant="amber"), 1, 0
    )
    if show_times:
        grid.addWidget(
            _stat_tile("Uptime", _fmt_uptime_full(node.get("uptimeMinutes")), variant="teal"), 1, 1
        )
        info_ago_s = round((node.get("infoAgoMs") or 0) / 1000, 1)
        grid.addWidget(_stat_tile("Đọc lúc", f"{info_ago_s}s trước", variant="pink"), 1, 2)
    else:
        # Uptime is an exact reading for every account; only how stale the
        # data is stays a gauge.
        grid.addWidget(
            _stat_tile("Uptime", _fmt_uptime_full(node.get("uptimeMinutes")), variant="teal"), 1, 1
        )
        fresh = freshness_level(node.get("infoAgoMs"))
        grid.addWidget(
            _meter_tile("Độ mới dữ liệu", variant="pink", level=fresh, color=freshness_color(fresh)), 1, 2
        )
    for col in range(3):
        grid.setColumnStretch(col, 1)
    layout.addWidget(grid_widget)
    return container


def _license_time_bar(node: dict, total_minutes: int | None, show_times: bool = True) -> QWidget:
    """Remaining-vs-total license time as one horizontal bar, like a
    countdown/battery gauge, instead of a bare "Remain: N" number.

    `total_minutes` is the duration this app last *set* for this device (see
    MainWindow.license_totals) — the protocol only ever reports what's left,
    never the original duration, so without that record there's no "total"
    to measure against and the bar just shows full with the raw minutes.
    """
    container = QWidget()
    layout = QVBoxLayout(container)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(4)

    remain = node.get("remain") or 0
    header = QHBoxLayout()
    header.addWidget(QLabel("Thời hạn license" if show_times else "Status"))
    header.addStretch(1)
    total = total_minutes if total_minutes and total_minutes > 0 else remain
    percent = round(100 * remain / total) if total else 0
    if show_times:
        percent_label = QLabel(f"{percent}%")
        percent_label.setObjectName("CardSubtitle")
        header.addWidget(percent_label)
    layout.addLayout(header)

    if not show_times:
        # 5-step gauge, no minutes and no %.
        gauge = MeterBar()
        gauge.set_level(license_level(remain, total_minutes), theme.countdown_color(percent))
        layout.addWidget(gauge)
        return container

    bar = QProgressBar()
    bar.setRange(0, 100)
    bar.setValue(max(0, min(100, percent)))
    bar.setTextVisible(False)
    bar_color = theme.countdown_color(percent)
    bar.setStyleSheet(
        f"QProgressBar {{ border: none; border-radius: 6px; background: {theme.BAR_TRACK};"
        " min-height: 11px; max-height: 11px; }"
        f"QProgressBar::chunk {{ border-radius: 6px; background: {bar_color}; }}"
    )
    layout.addWidget(bar)

    if total_minutes:
        elapsed = max(0, total_minutes - remain)
        detail = f"Đã dùng {elapsed} phút · Còn lại {remain} phút / Tổng {total_minutes} phút"
    else:
        detail = f"Còn lại {remain} phút (set License qua app này để theo dõi % đã dùng)"
    detail_label = QLabel(detail)
    detail_label.setObjectName("CardSubtitle")
    detail_label.setWordWrap(True)
    layout.addWidget(detail_label)
    return container


class DeviceDetailDialog(_ParentCenteredDialog):
    """Full-detail view for one device, opened by clicking its card on the
    Devices dashboard. Get License / Get Info are read-only queries, so
    clicking them keeps this dialog open and fires the request; the caller
    (MainWindow._open_device_detail) feeds the resulting device.upsert back
    in via refresh() so the numbers update in place instead of the user
    having to close and reopen the dialog to see them. Set License / Config /
    Xoá are the opposite (they change something and the effect is better
    watched on the card/log), so those still close the dialog after firing.
    """

    def __init__(
        self, node: dict, *, on_get_license, on_get_info, on_set_license, on_config,
        on_alias_save, on_delete, on_set_card_type, total_minutes: int | None = None,
        show_times: bool = True, parent=None,
    ):
        super().__init__(parent)
        self._show_times = show_times
        self.setModal(True)
        self.resize(360, 620)
        self._total_minutes = total_minutes
        layout = QVBoxLayout(self)

        # Photo + alias + stat/info tiles add up to ~850px, and ~1020px once
        # LIC_INFO data is in (opened again later, or after Get Info) - taller
        # than what fits on one screen once the window manager's title bar and
        # top panel are counted, so it got pushed onto the other monitor.
        # Scroll that part instead of letting the dialog outgrow the screen;
        # the action buttons below stay put.
        self._scroll = QScrollArea()
        self._scroll.setWidgetResizable(True)
        self._scroll.setFrameShape(QFrame.Shape.NoFrame)
        self._scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        body = QWidget()
        self._scroll.setWidget(body)
        layout.addWidget(self._scroll, 1)
        content = QVBoxLayout(body)
        content.setContentsMargins(0, 0, 0, 0)

        # Updated per-node in refresh() below — which photo to show
        # (removable-card vs. onboard-card) depends on the device's cardType.
        self._image_label = QLabel()
        self._image_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        content.addWidget(self._image_label)

        header = QHBoxLayout()
        self._title_label = QLabel()
        self._title_label.setStyleSheet("font-size:18px; font-weight:800;")
        header.addWidget(self._title_label)
        header.addStretch(1)
        self._state_label = QLabel()
        header.addWidget(self._state_label, 0, Qt.AlignmentFlag.AlignVCenter)
        content.addLayout(header)

        alias_row = QHBoxLayout()
        alias_row.addWidget(QLabel("Alias:"))
        self.alias_edit = QLineEdit(node.get("alias", ""))
        alias_row.addWidget(self.alias_edit)
        content.addLayout(alias_row)

        # Rebuilt in place by refresh() on every live update instead of the
        # dialog needing to be closed and reopened to show fresh data.
        self._details = QWidget()
        self._details_layout = QVBoxLayout(self._details)
        self._details_layout.setContentsMargins(0, 0, 0, 0)
        self._details_layout.setSpacing(10)
        content.addWidget(self._details)

        save_alias_btn = QPushButton("Lưu Alias")
        save_alias_btn.clicked.connect(
            lambda: self._run_and_close(lambda: on_alias_save(self.alias_edit.text().strip()))
        )
        alias_row.addWidget(save_alias_btn)  # same row as the field, saves a row of height
        content.addStretch(1)  # spare height goes here, not into stretching the tiles/state pill

        actions = QHBoxLayout()
        get_btn = QPushButton("Get License")
        get_btn.clicked.connect(lambda: self._run_and_keep_open(on_get_license))
        actions.addWidget(get_btn)
        get_info_btn = QPushButton("Get Info")
        get_info_btn.setToolTip("Đọc firmware/điện áp/nhiệt độ/uptime (LIC_INFO)")
        get_info_btn.clicked.connect(lambda: self._run_and_keep_open(on_get_info))
        actions.addWidget(get_info_btn)
        set_btn = QPushButton("Set License")
        set_btn.setProperty("variant", "primary")
        set_btn.clicked.connect(lambda: self._run_and_close(on_set_license))
        actions.addWidget(set_btn)
        cfg_btn = QPushButton("Config")
        cfg_btn.clicked.connect(lambda: self._run_and_close(on_config))
        actions.addWidget(cfg_btn)
        card_type_btn = QPushButton("Set Card Type")
        card_type_btn.setToolTip("Ghi loại board (Card_R rời / Card_OB onboard) xuống node")
        card_type_btn.clicked.connect(lambda: self._run_and_keep_open(on_set_card_type))
        actions.addWidget(card_type_btn)
        del_btn = QPushButton("Xoá")
        del_btn.setProperty("variant", "danger")
        del_btn.clicked.connect(lambda: self._run_and_close(on_delete))
        actions.addWidget(del_btn)
        layout.addLayout(actions)

        close_btn = QPushButton("Đóng")
        close_btn.clicked.connect(self.reject)
        layout.addWidget(close_btn)

        self._actions_layout = actions
        self._close_btn = close_btn
        self.refresh(node)
        self._fit_height(initial=True)

    def refresh(self, node: dict, total_minutes: int | None = None) -> None:
        """Re-renders the title/state pill and the license/stat/info panels
        from a fresh `node` dict (a device.upsert payload) without closing
        the dialog. `total_minutes` is optional — pass it after a Set
        License call; omit it (e.g. on a plain Get Info/Get License refresh)
        to keep whatever total this dialog already knew about."""
        if total_minutes is not None:
            self._total_minutes = total_minutes

        self.setWindowTitle(node.get("uid") or "Device")
        self._title_label.setText(node.get("uid") or "—")
        pixmap = board_pixmap(node.get("cardType") or "R")
        if pixmap is not None:
            self._image_label.setPixmap(
                pixmap.scaledToWidth(200, Qt.TransformationMode.SmoothTransformation)
            )
        state = theme.effective_response_state(node)
        color, bg = theme.STATUS_COLORS.get(state, theme.STATUS_COLORS["UNKNOWN"])
        self._state_label.setText(state)
        self._state_label.setStyleSheet(
            f"color:{color}; background:{bg}; border-radius:9px; padding:3px 9px;"
            f"font-size:11px; font-weight:800;"
        )

        while self._details_layout.count():
            item = self._details_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                # setParent(None) detaches it from view immediately;
                # deleteLater() alone leaves it fully visible, overlapping
                # the freshly-added widgets below, until Qt gets around to
                # actually processing the deferred deletion.
                widget.setParent(None)
                widget.deleteLater()
        self._details_layout.addWidget(_license_time_bar(node, self._total_minutes, self._show_times))
        self._details_layout.addWidget(_stat_grid(node, self._show_times))
        self._details_layout.addWidget(_info_grid(node, self._show_times))
        # Measured on the next event-loop pass, not now: Qt reports the old,
        # shorter content height until it has re-laid-out the swapped tiles.
        QTimer.singleShot(0, self._fit_height)

    def _fit_height(self, initial: bool = False) -> None:
        """Size the dialog to its content but never taller than the parent's\n        screen can hold (the scroll area takes up the slack). Only grows after\n        the first sizing, and re-centers when it does so a taller dialog isn't\n        left hanging off the bottom of the screen."""
        if not hasattr(self, "_close_btn"):
            return  # refresh() called from __init__ before the footer exists
        parent = self.parentWidget()
        screen = (parent.window().screen() if parent is not None else None) or self.screen()
        if screen is None:
            return
        body = self._scroll.widget()
        outer = self.layout()
        margins = outer.contentsMargins()
        chrome = (
            self._actions_layout.sizeHint().height() + self._close_btn.sizeHint().height()
            + 2 * outer.spacing() + margins.top() + margins.bottom()
        )
        cap = screen.availableGeometry().height() - 160  # title bar + top panel + breathing room
        desired = min(body.sizeHint().height() + chrome, cap)
        # The footer button row's own minimum width counts too - without it the
        # dialog could shrink narrower than its buttons' labels.
        content_width = max(body.minimumSizeHint().width(), self._actions_layout.minimumSize().width())
        self.setMinimumWidth(content_width + margins.left() + margins.right() + 24)
        if initial or desired > self.height():
            grew = desired > self.height()
            self.resize(max(self.width(), self.minimumWidth(), 560 if initial else 0), desired)
            if grew and self.isVisible():
                self._center_on_parent_screen(force=True)

    def _run_and_close(self, callback) -> None:
        callback()
        self.accept()

    def _run_and_keep_open(self, callback) -> None:
        callback()
