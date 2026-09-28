import sys

from PySide6.QtWidgets import QApplication

import theme
from main_window import MainWindow


def main() -> None:
    app = QApplication(sys.argv)
    app.setStyleSheet(theme.STYLESHEET)
    window = MainWindow()
    # Maximized by default: the Devices grid enforces an exact column count
    # (fixed-width DeviceListView, see device_card.py), so a small starting
    # window forces horizontal scrolling to see a full row of cards.
    window.showMaximized()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
