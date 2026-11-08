# LOS201 - Embedded Linux System Development

Repository cá nhân tổng hợp toàn bộ bài thực hành Lab và đồ án Assignment môn Hệ thống Nhúng (LOS201) - Trường Đại học FPT.

- **Sinh viên thực hiện:** Trần Lê Anh Duy  
- **Chuyên ngành:** Thiết kế Vi mạch IC & Hệ thống Nhúng  

---

## 🛠️ Repository Architecture

- `LAB1/` — Cross-compilation toolchain (`arm-linux-gnueabihf`), build Linux Kernel 5.15 LTS, U-Boot 2022.04, tích hợp BusyBox & boot hệ thống nhúng trên QEMU ARM (Vexpress-A9).
- `LAB2/` — Phát triển Character Device Driver (`lab2_driver`), xây dựng các giao diện giao tiếp `/proc` và `/sys` (procfs/sysfs), cấu hình MTD subsystem mô phỏng NAND Flash chạy JFFS2 Filesystem.
- `LAB3/` — Kỹ thuật Debugging & Performance Profiling trên hệ thống nhúng:
  * Debug lỗi ứng dụng ARM bằng `gdb-multiarch` kết nối QEMU gdbserver.
  * Phân tích bộ nhớ & phát hiện Memory Leak với `Valgrind`.
  * Trích xuất System Calls (`strace`) và xác định CPU Hotspot (`perf`).
  * Đo kiểm Real-Time Latency (`cyclictest`) & trực quan hóa histogram bằng `gnuplot`.
- `ASM1_SOURCE/` & `ASM1.tar.gz` — Assignment 1: Mở rộng Character Driver hỗ trợ các lệnh `ioctl`, phát triển `miscdevice` Counter Driver điều khiển từ User-space.
- `ASM2/` — Assignment 2 (Capstone Project): Hệ thống Sensor Monitoring tích hợp Linux Kernel Driver, xử lý đa luồng (Multi-threading), giao tiếp tiến trình (IPC) và đáp ứng thời gian thực (Real-Time processing).


---

