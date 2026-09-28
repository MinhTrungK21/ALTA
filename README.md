# Sender ESP-NOW

Firmware điều khiển ESP-NOW qua Web Local tại `http://192.168.4.1`.

- Web Local cung cấp giao diện cấu hình và quản lý thiết bị.
- Cùng giao thức JSON đó cũng có thể điều khiển qua cáp USB (native USB CDC
  của ESP32-S3) thay cho WiFi — ổn định hơn, xem `local_web.h` phần "USB
  serial transport" và app tham khảo tại `linux_client/`.
- PCF8575 tiếp tục quản lý cụm phím và LED qua I2C.
- Cấu hình và driver phần cứng LCD JC4827W543 vẫn nằm trong `hardware/`, nhưng firmware không khởi tạo UI hoặc runtime đồ họa.
