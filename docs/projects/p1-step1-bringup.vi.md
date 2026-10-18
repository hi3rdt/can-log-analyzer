# Project 1 — Bước 1: dựng bàn test (đấu dây, ESP32 rest-bus, STM32 toolchain)

Người học: hi3rdt · Ngày: 2026-10-18 · Thiết kế tổng: [p1-instrument-cluster.vi.md](p1-instrument-cluster.vi.md)

> Code, lệnh, tên chân giữ nguyên tiếng Anh.

---

## 0. Ai làm phần nào

| Phần | Trạng thái | Ai viết |
|---|---|---|
| Toolchain STM32, CMake preset, cấu hình VS Code/OpenOCD | có sẵn trong repo | Claude ([hướng dẫn cài](../setup/stm32-toolchain.vi.md)) |
| ESP32 rest-bus simulator + fault injection | có sẵn: `esp32/restbus_sim/` | Claude (công cụ test, không phải phần CV chính) |
| MCAL STM32: Port/Dio, Can, Spi; lớp `SigRx`; `ClusterHmi` | TODO | **bạn**, mình ra skeleton + test và review |

Lý do chia như vậy: trên CV, phần bạn phải bảo vệ từng dòng là ECU cluster trên STM32. ESP32 đóng
vai "xe" (như CANoe ở công ty), nên mình viết sẵn để bạn có bus thật ngay từ đầu. Nếu bạn muốn tự viết
cả phía ESP32 thì cứ nói, mình chuyển nó thành bài tập.

## 1. Sơ đồ đấu dây

```
 ESP32 DevKit                 Transceiver A          Transceiver B            Blue Pill
 GPIO5 (TWAI TX) ───────────► TXD/CTX                 TXD/CTX ◄────────────── PB9 (CAN_TX, remap)
 GPIO4 (TWAI RX) ◄───[*]───── RXD/CRX                 RXD/CRX ──────────────► PB8 (CAN_RX, remap)
 3V3 / 5V ──────────────────► VCC                     VCC ◄────────────────── 3.3 / 5V
 GND ───────────────────────► GND ───────┬─────────── GND ◄────────────────── GND
                                CANH ────┼──────────── CANH
                                CANL ────┼──────────── CANL
                              120 Ω      │           120 Ω   (một điện trở ở mỗi đầu bus)
                                         └── GND chung giữa hai board là bắt buộc
```

`[*]` Nếu transceiver là **TJA1050 / MCP2551 (nuôi 5 V)**: chân RXD ra mức 5 V, phải qua cầu chia
(ví dụ 2.2 kΩ nối tiếp, 3.3 kΩ xuống GND) trước khi vào GPIO4 của ESP32. Phía STM32 không cần vì
PB8 chịu được 5 V (chân FT). Với **SN65HVD230 (3.3 V)** thì nối thẳng cả hai phía.

Kiểm tra trước khi cấp nguồn:

1. Đo điện trở giữa CANH và CANL khi **tắt nguồn**: phải ra khoảng **60 Ω** (hai 120 Ω song song).
   Ra 120 Ω là thiếu một điện trở; ra 40 Ω là thừa (nhiều module đã hàn sẵn 120 Ω, jumper hoặc R tên
   `R120`/`J1`).
2. Dây CANH/CANL nên xoắn vào nhau, dài dưới 1 m là đủ cho bàn test.

### Màn hình (dùng ở bước P1.4, chưa cần bây giờ)

Kiểm tra mặt sau module TFT 2.4" của bạn:

- Có chân **SCK, SDI/MOSI, DC/RS, CS, RESET** → màn **SPI** (thường ILI9341). Đấu vào SPI1:
  `PA5 = SCK`, `PA7 = MOSI`, `PA6 = MISO` (tuỳ chọn), `PA4 = CS`, `PB0 = DC`, `PB1 = RESET`,
  `LED` → 3.3 V.
- Có chân **LCD_D0..LCD_D7, LCD_RS, LCD_WR, LCD_RD** (dạng shield cắm Arduino UNO) → màn
  **song song 8-bit**. Loại này vẫn dùng được nhưng cần 13 GPIO và thiết kế driver khác. Báo mình loại
  bạn có để mình chỉnh bài P1.4.

## 2. Bước A — ESP32 tự kiểm tra (không cần STM32)

```bash
cd esp32/restbus_sim
idf.py set-target esp32
idf.py -p COM5 flash monitor
```

Trong console:

```
restbus> mode noack          # ESP32 khởi động lại ở chế độ self-test
restbus> rxlog on            # thấy các dòng "RX STD 00000100 [8] ..." chạy liên tục
restbus> status              # tx_ok và rx tăng, TEC = 0
restbus> rxlog off
```

Nếu chưa có transceiver, nối thẳng **GPIO5 → GPIO4** bằng một dây là đủ cho bước này.

Sau đó `mode normal`. Ở chế độ normal mà STM32 chưa nhận frame, `status` sẽ báo TEC tăng đến 128
(error passive) và log `error passive (no ACK? ...)`: **đó là đúng**, vì CAN cần ít nhất một node
khác ACK. Khi driver Can của bạn chạy, TEC sẽ về 0.

## 3. Bước B — STM32 toolchain

Làm theo [stm32-toolchain.vi.md](../setup/stm32-toolchain.vi.md) đến mục 6: build, flash, LED PC13
nháy 1 Hz, debug được và xem được `GPIOC->ODR` trong *XPERIPHERALS*.

## 4. Bước C — bài MCAL theo thứ tự

| Thứ tự | Bài | Kết quả trên bàn test |
|---|---|---|
| 1 | [Port/Dio](../exercises/ex01-port-dio.vi.md) | `main.c` dùng `Port_Init` + `Dio_FlipChannel` thay cho `Led_Init`/`Led_Toggle` viết tay |
| 2 | [Can (bxCAN)](../exercises/ex03-can-bxcan.vi.md), chạy **loopback** trước | STM32 nhận lại frame của chính nó |
| 3 | Can ở **normal mode** trên bus thật | ESP32 `status`: TEC = 0; breakpoint trong `CanIf_RxIndication` thấy `0x100` mỗi 10 ms |

Tiêu chí xong Bước 1 (P1.1 trong thiết kế): LED trên Blue Pill nháy theo frame `0x100` nhận được
(ví dụ đảo LED mỗi 50 frame = 2 Hz), và tắt khi bạn gõ `fault 100 drop` trên ESP32. Đó là phiên bản
đầu tiên của deadline monitoring, trước khi viết `SigRx` thật ở P1.3.

## 5. Bảng lệnh fault injection (dùng từ P1.3 trở đi)

| Lệnh ESP32 | Lỗi mô phỏng | `SigRx` phải ra trạng thái |
|---|---|---|
| `fault 100 drop` | ABS không gửi nữa | `SIG_STATE_TIMEOUT` sau `timeout_ms` |
| `fault 100 corrupt` | nhiễu làm hỏng checksum | `SIG_STATE_INVALID` sau `max_err` frame |
| `fault 100 freeze` | ABS treo, vẫn gửi frame cũ | `SIG_STATE_INVALID` (counter đứng yên) |
| `fault 100 repeat` | frame bị gửi lặp | phát hiện counter bước 0 |
| `silence on` | toàn bộ bus im | mọi signal `TIMEOUT`, mọi đèn cảnh báo bật |
| `pause` rồi `play` | mất bus rồi có lại | về `VALID` sau K frame tốt |

Khi log quay vòng (30 s), counter của `0x100` nhảy một lần; cluster đếm một lỗi counter mỗi vòng
và không được báo INVALID chỉ vì một lần nhảy đó (debounce).
