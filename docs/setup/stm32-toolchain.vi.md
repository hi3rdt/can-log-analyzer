# Cài môi trường lập trình STM32F103 (VS Code + Arm GCC + OpenOCD + ST-Link)

Người học: hi3rdt · Ngày: 2026-10-18 · Áp dụng cho: repo `can-log-analyzer`, Blue Pill + ST-Link V2

> Code, lệnh, tên công cụ giữ nguyên tiếng Anh. Hướng dẫn viết cho **Windows 10/11**; phần Linux ở
> mục 8. ESP32 không cần cài thêm vì bạn đã có ESP-IDF.

---

## 0. Vì sao không dùng STM32CubeIDE

Project này viết MCAL ở mức thanh ghi, không dùng HAL/CubeMX, và build bằng **CMake** (cùng một
`CMakeLists.txt` cho firmware lẫn host test, CI trên GitHub chạy đúng lệnh đó). STM32CubeIDE dùng
Eclipse managed build và mặc định kéo HAL vào, nên phải duy trì hai hệ build song song. Với VS Code bạn
có: build bằng CMake preset, flash/debug bằng OpenOCD + ST-Link, **xem thanh ghi ngoại vi trực tiếp
khi debug** (rất cần khi viết MCAL), và dùng chung editor với ESP-IDF. STM32CubeProgrammer vẫn nên cài
(mục 1.4) vì nó kèm driver ST-Link và cứu chip khi bị khoá.

## 1. Cài công cụ

| # | Công cụ | Lấy ở đâu | Kiểm tra sau khi cài |
|---|---|---|---|
| 1.1 | **Arm GNU Toolchain** (arm-none-eabi), bản 13.x hoặc 14.x, gói `mingw-w64-i686-arm-none-eabi.exe` hoặc `x86_64` | developer.arm.com → *Arm GNU Toolchain Downloads* | `arm-none-eabi-gcc --version` |
| 1.2 | **CMake** ≥ 3.21 | `winget install Kitware.CMake` | `cmake --version` |
| 1.3 | **Ninja** | `winget install Ninja-build.Ninja` | `ninja --version` |
| 1.4 | **STM32CubeProgrammer** (kèm driver ST-Link) | st.com → STM32CubeProg | mở app, cắm ST-Link, bấm *Connect* |
| 1.5 | **xPack OpenOCD** 0.12.x | github.com/xpack-dev-tools/openocd-xpack/releases (`win32-x64.zip`) | `openocd --version` |
| 1.6 | **Git** | `winget install Git.Git` | `git --version` |
| 1.7 | **VS Code** | code.visualstudio.com | — |

Ghi chú khi cài:

- 1.1: ở trang cuối của installer tick **"Add path to environment variable"**.
- 1.5: giải nén vào ví dụ `C:\tools\xpack-openocd-0.12.0-x`, rồi thêm `...\bin` vào `PATH`
  (*Edit the system environment variables → Environment Variables → Path → New*).
- Sau khi sửa `PATH`, **đóng hết terminal và VS Code** rồi mở lại.

> **Bẫy với ESP-IDF:** terminal "ESP-IDF" (hoặc sau khi chạy `export.bat`) đặt `openocd` của
> Espressif (`openocd-esp32`) lên đầu `PATH`. Khi flash/debug STM32 hãy dùng terminal thường, hoặc chỉ
> định rõ đường dẫn trong *User Settings* của VS Code:
> `"cortex-debug.openocdPath": "C:/tools/xpack-openocd-0.12.0-x/bin/openocd.exe"`.

## 2. Extension VS Code

Mở thư mục repo trong VS Code, VS Code sẽ gợi ý cài các extension trong `.vscode/extensions.json`:

| Extension | Dùng để |
|---|---|
| C/C++ (`ms-vscode.cpptools`) | IntelliSense, nhảy tới định nghĩa |
| CMake Tools (`ms-vscode.cmake-tools`) | chọn preset, build |
| Cortex-Debug (`marus25.cortex-debug`) | debug qua OpenOCD + GDB |
| Cortex-Debug device support STM32F1 (`marus25.cortex-debug-dp-stm32f1`) | file SVD: cửa sổ *XPERIPHERALS* hiện thanh ghi GPIO/CAN/RCC theo tên bit |
| ESP-IDF (`espressif.esp-idf-extension`) | cho phần ESP32 (bạn đã cài thì bỏ qua) |

## 3. Nối ST-Link với Blue Pill

| ST-Link V2 | Blue Pill (header 4 chân SWD ở cạnh ngắn) |
|---|---|
| SWDIO | DIO (PA13) |
| SWCLK | DCLK (PA14) |
| GND | GND |
| 3.3V | 3.3 |

- Cả hai jumper BOOT0/BOOT1 ở vị trí **0**.
- Chỉ cấp nguồn **một** đường: hoặc 3.3 V từ ST-Link, hoặc USB của Blue Pill (khi đó bỏ dây 3.3V).

## 4. Build firmware lần đầu

```bash
git clone https://github.com/hi3rdt/can-log-analyzer.git
cd can-log-analyzer
cmake --preset fw          # configure: Ninja + cmake/arm-none-eabi.cmake -> build-fw/
cmake --build --preset fw  # build: firmware.elf/.hex/.bin
```

Trong VS Code: thanh trạng thái dưới cùng chọn *Configure Preset* = **fw**, rồi `Ctrl+Shift+B`
(task `fw: build`). Kết quả mong đợi (số có thể lệch vài byte):

```
Memory region         Used Size  Region Size  %age Used
           FLASH:        1056 B        64 KB      1.61%
             RAM:        2064 B        20 KB     10.08%
```

RAM 2 KB đó là stack MSP dành sẵn trong linker script, không phải biến.

## 5. Flash

```bash
openocd -f tools/openocd/bluepill.cfg -c "program build-fw/firmware.elf verify reset exit"
```

Hoặc trong VS Code: `Ctrl+Shift+P` → *Tasks: Run Task* → **fw: flash (OpenOCD + ST-Link)**.
Thấy `** Verified OK **` là xong; **LED PC13 nháy 1 Hz** (heartbeat trong `app/main.c`).

`tools/openocd/bluepill.cfg` đã xử lý sẵn hai vấn đề hay gặp với Blue Pill:

- chip clone (CKS32/CS32/APM32) có IDCODE khác chip ST thật → `set CPUTAPID 0`;
- ST-Link clone không có dây NRST → `reset_config none` (reset bằng SYSRESETREQ).

## 6. Debug

Nhấn `F5` → chọn **STM32: debug (OpenOCD + ST-Link)**: build, nạp, dừng ở `main`.

Những thứ nên tập ngay vì sẽ dùng suốt khi viết MCAL:

- Cửa sổ **XPERIPHERALS** → `GPIOC` → `CRH`, `ODR`: đặt breakpoint trong `Led_Toggle()` và xem bit 13
  của `ODR` đổi. Sau này bài Port/Dio, bạn kiểm tra `CRL/CRH` ở đây thay vì đoán.
- `RCC` → `CFGR`: xác nhận `SWS = PLL` (đang chạy 72 MHz). Nếu là HSI thì thạch anh 8 MHz có vấn đề.
- Biến `System_TickMs` trong *WATCH*: tăng 1 mỗi ms.
- Cấu hình thứ hai **STM32: attach** gắn vào chip đang chạy mà không reset, dùng khi muốn xem trạng
  thái bxCAN (ví dụ `CAN_ESR`: TEC/REC, LEC) đúng lúc đang có lỗi bus.

## 7. Host unit test (chạy trên PC)

Host test dùng AddressSanitizer/UBSan, MinGW trên Windows không hỗ trợ, nên chạy trong **WSL**:

```bash
# Trong Ubuntu (WSL), một lần:
sudo apt install build-essential cmake ninja-build python3-pip
# Mỗi lần test:
cmake --preset host && cmake --build --preset host && ctest --preset host
```

Hoặc task VS Code **host: build + test** khi mở repo bằng *WSL: Open Folder*. CI trên GitHub chạy đúng
các lệnh này cho mỗi PR, nên nếu chưa cài WSL bạn vẫn thấy kết quả test trên PR.

## 8. Linux (Ubuntu/Debian)

```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi gdb-multiarch openocd cmake ninja-build
# Gói openocd cài sẵn udev rules cho ST-Link: rút ra cắm lại ST-Link là dùng được không cần sudo.
```

Trên Linux, thêm vào *User Settings*: `"cortex-debug.gdbPath": "gdb-multiarch"` (gói apt không có
`arm-none-eabi-gdb`).

## 9. Lỗi hay gặp

| Triệu chứng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| `UNEXPECTED idcode` | chip clone | đã xử lý bằng `CPUTAPID 0`; nếu vẫn lỗi, kiểm tra bạn đang dùng đúng `bluepill.cfg` |
| `init mode failed (unable to connect to the target)` | dây SWD lỏng, chưa cấp nguồn, hoặc firmware cũ đã **tắt SWD** (ghi sai `AFIO_MAPR.SWJ_CFG`) | kiểm tra dây; nếu nghi SWD bị tắt: đặt **BOOT0 = 1**, reset, flash lại, rồi trả BOOT0 = 0 |
| `flash write failed` / chip bị khoá đọc | Option byte RDP hoặc WRP đang bật (hay gặp ở board mới mua) | STM32CubeProgrammer → *Option bytes* → RDP = AA, bỏ WRP; hoặc `openocd -f tools/openocd/bluepill.cfg -c "init; reset halt; stm32f1x unlock 0; reset halt; stm32f1x mass_erase 0; exit"` |
| `LIBUSB_ERROR_NOT_SUPPORTED` / không thấy ST-Link | thiếu driver | cài STM32CubeProgrammer (kèm driver), cắm lại ST-Link |
| `cortex-debug: gdb not found` | GDB không nằm trong `PATH` | User Settings: `"cortex-debug.armToolchainPath": "C:/.../Arm GNU Toolchain .../bin"` |
| `arm-none-eabi-gdb` báo thiếu Python | một số bản GDB Windows cần Python đúng phiên bản | cài bản Python mà thông báo yêu cầu, hoặc dùng bản toolchain khác (13.3.rel1) |
| LED không nháy dù flash OK | đang debug và CPU bị halt, hoặc board có LED ở chân khác | `continue` trong debugger; kiểm tra LED nằm trên PC13 |

> **Chú ý cho bài CAN sau này:** khi remap CAN sang PB8/PB9 bạn sẽ ghi `AFIO_MAPR`. Các bit
> `SWJ_CFG[26:24]` là write-only và giá trị đọc về **không xác định** (RM0008 §9.4.2), nên khi
> read-modify-write phải **xoá 3 bit đó** trước khi ghi lại (`0b000` = giữ JTAG + SWD). Nếu vô tình
> ghi `0b100`, SWD tắt và bạn phải dùng mẹo BOOT0 = 1 ở bảng trên.
