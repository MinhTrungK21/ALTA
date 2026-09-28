# HUB66S Control (Linux desktop client)

App Linux (PySide6) thay thế cho giao diện web `http://192.168.4.1`. Hỗ trợ
2 đường truyền tới hub, cùng một giao thức JSON `{"action":...,"data":{...}}`
mà `local_web.h` xử lý cho trình duyệt:

- **WiFi** — WebSocket cổng 81 (như trang web).
- **USB** — cổng serial ảo của ESP32-S3 (native USB CDC), ổn định hơn WiFi vì
  không qua sóng radio, không giới hạn số client, không phải lo rớt AP. Xem
  local_web.h phần "USB serial transport" bên firmware.

## Cài đặt

```bash
cd linux_client
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Chạy — qua WiFi

1. Kết nối máy Linux vào WiFi AP của hub: SSID `HUB66S-xxxxxx`, mật khẩu
   `hub66s66`.
2. Chạy app: `python3 main.py`.
3. Chọn transport **WiFi** (mặc định), giữ nguyên ô "Hub" = `192.168.4.1`,
   bấm **Kết nối**.
4. Đăng nhập bằng tài khoản trên hub (mặc định `admin` / `admin123` nếu
   chưa từng tạo tài khoản nào). Nếu tài khoản bật 2FA, app sẽ hỏi thêm mã 6 số.

## Chạy — qua USB

1. Cắm cáp USB-C từ hub vào máy Linux (không cần bật WiFi/AP).
2. Chạy app: `python3 main.py`.
3. Chọn transport **USB**, bấm nút ⟳ để dò cổng nếu danh sách trống, chọn
   cổng (thường là `/dev/ttyACM0`), bấm **Kết nối**.
4. Sau vài giây chờ board khởi động lại (cắm/mở cổng USB làm ESP32-S3 tự
   reset), app sẽ hỏi đăng nhập **giống hệt WiFi** — dùng đúng tài khoản đã
   tạo trên hub (mặc định `admin` / `admin123` nếu chưa tạo tài khoản nào).

Nếu không thấy quyền truy cập cổng serial (`Permission denied`), thêm user
vào group `dialout`: `sudo usermod -aG dialout $USER` rồi đăng xuất/đăng nhập lại.

## Tính năng

- Đăng nhập / đăng xuất, quản lý tài khoản (tạo, đổi mật khẩu, xoá, bật/tắt 2FA với QR).
- Tab **Thiết bị**: Scan discovery, Refresh All, tìm theo UID, xem danh sách
  Node theo thời gian thực, sửa/xoá alias (double-click cột Alias), Get/Set
  License, Config Device, xoá khỏi danh sách.
- Tab **Group**: tạo/sửa/xoá group, chọn thành viên, Get/Set License cho cả group.
- Thanh trạng thái dưới cùng hiển thị tiến trình job (scan/refresh/set license…)
  và log các sự kiện.

## Ghi chú giao thức

Toàn bộ hành vi tương ứng 1:1 với các action trong `local_web.h`
(`auth.login`, `state.get`, `scan.start`, `license.get`, `license.set`,
`device.config`, `group.save`, `alias.save`, `account.*`, …). Nếu firmware
thêm action mới, chỉ cần thêm xử lý tương ứng trong `main_window.py`.
