# Embedded Linux & System Programming (LOS201)

Repository lưu trữ bài thực hành Lab và Assignment môn LOS201 tại FPT University.

**Họ và tên:** Trần Lê Anh Duy  
**Chuyên ngành:** Thiết kế Vi mạch IC & Hệ thống Nhúng  

---

## 📁 Cấu trúc Thư mục Repository

- **LAB1/**: Cross-Compilation, Kernel Building & Booting QEMU
- **LAB2/**: Custom RootFS & BusyBox Init System
- **LAB3/**: Debugging, Memory Leak, Profiling & RT Latency
- **ASM1_SOURCE/** / **ASM2/**: Mã nguồn các bài Assignment
- **MSSV_TranLeAnhDuy_Lab03.tar.gz**: File nén nộp bài Lab 3 chuẩn

---

## 🛠️ Tóm tắt Nội dung Thực hành LAB-03

- **Phần A (Remote Debugging):** GDB Multiarch kết nối QEMU/gdbserver xử lý bug buggy_app.
- **Phần B (Memory Analysis):** Phân tích rò rỉ bộ nhớ với Valgrind.
- **Phần C (System Profiling):** Phân tích strace & perf (CPU hotspot tại __ieee754_log_fma).
- **Phần D (RT Latency):** Cyclictest 4 threads & vẽ biểu đồ latency_histogram.png bằng gnuplot.
