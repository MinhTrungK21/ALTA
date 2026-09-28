HUB66S RECEIVER ESP-NOW
======================

Firmware dành cho ESP32-S3, dùng làm bộ nhận và quản lý license của HUB66S qua
giao thức ESP-NOW.

Chức năng chính
---------------

- Nhận và phản hồi gói JSON qua ESP-NOW trên Wi-Fi channel 1.
- Xác thực dữ liệu bằng mã MD5 và khóa dùng chung.
- Device UID cố định được sinh từ MAC của ESP32; không còn cấu hình Device ID.
- Opcode 0x07 chỉ dùng để cấu hình LID dạng chuỗi.
- Cấp, đọc, xóa và theo dõi thời hạn license.
- Lưu runtime và license vào NVS, không mất khi tắt nguồn.
- Tự gửi lại tối đa 3 lần khi truyền thất bại.
- Báo trạng thái bằng LED và tự khởi động lại khi bị treo bằng watchdog.

Các lệnh hỗ trợ
---------------

- 0x01: Lấy thời gian hoạt động.
- 0x02: Cấp hoặc cập nhật license.
- 0x03: Đọc thông tin license.
- 0x04: Xóa một license.
- 0x05: Xóa toàn bộ license.
- 0x06: Lấy thông tin thiết bị.
- 0x07: Cấu hình LID.
- 0x08: Nhấp nháy nhanh LED 46 cùng các chân 1-12 (HIGH/LOW, 100 ms / 100 ms) để nhận diện board đang chọn trong app.
  data {"on":true|false,"sec":giây}; tự tắt sau `sec` giây; node không phản hồi.

Phần cứng
---------

- Board: ESP32-S3.
- LED trạng thái: GPIO 46.
- Ngõ ra LED hiển thị: GPIO 1 đến 12.
- Serial: 115200 baud.

Tệp chính
---------

- receiver.ino: khởi tạo và vòng lặp chính.
- protocol_handler.h: xử lý lệnh, license và NVS.
- espnow_handler.h: giao tiếp ESP-NOW.
- config.h: cấu hình, opcode và xác thực MD5.
- led_status.h, led_display.h: điều khiển LED.
- serial.h: tệp tương thích; LID được cấu hình qua ESP-NOW opcode 0x07.
- watch_dog.h: giám sát treo chương trình.

Biên dịch bằng Arduino IDE với board ESP32S3 Dev Module. Phía gửi phải sử dụng
cùng Wi-Fi channel, định dạng JSON, khóa xác thực và cách tính MD5.
