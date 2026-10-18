# Project 1 — Mini Instrument Cluster trên CAN thật (STM32 + ESP32 + TFT 2.4")

Người học: hi3rdt · Ngày: 2026-10-18 · Trạng thái: đề xuất, chưa bắt đầu
Phần cứng: STM32F103C8 (Blue Pill, bxCAN) · ESP32 (TWAI) · 2 transceiver CAN · TFT 2.4" (ILI9341, SPI)

> Code, tên module, thuật ngữ kỹ thuật giữ nguyên tiếng Anh. Project này là **bước 1** của chuỗi
> P1 → P3 (xem [p3-can-bootloader.vi.md](p3-can-bootloader.vi.md)): toàn bộ phần CAN/MCAL làm ở đây
> được P3 dùng lại nguyên vẹn.

---

## 1. Bài toán thực tế

Cụm đồng hồ (instrument cluster) là ECU "nghe" nhiều nhất trên xe: nó nhận speed, RPM, nhiệt độ, trạng
thái cửa, đèn cảnh báo từ hàng chục ECU khác và phải **hiển thị đúng cả khi bus có vấn đề**. Ba câu hỏi
mà kỹ sư cluster thật phải trả lời, và project này trả lời:

1. **Signal không còn đến thì hiển thị gì?** (deadline monitoring / timeout → giá trị thay thế, đèn lỗi)
2. **Signal đến nhưng sai thì sao?** (alive counter đứng yên hoặc nhảy cóc, checksum sai → bỏ frame)
3. **Làm sao test cluster mà không cần xe?** (một "rest-bus simulator" phát lại log thật lên bus)

Trong công nghiệp, câu 3 là công việc hằng ngày của HIL/validation (CANoe + rest-bus simulation). Ở đây
ESP32 đóng vai rest-bus simulator, STM32 đóng vai ECU cluster.

## 2. Kiến trúc

```
   PC (SavvyCAN)                                         
      │  USB-UART (SLCAN, MCAL Uart - M4, optional)      
      │                                                   
 ┌────┴──────────────┐      CAN 500 kbit/s       ┌───────────────────────┐
 │ STM32F103 "Cluster│◄─────────────────────────►│ ESP32 "Rest-bus sim"  │
 │ ECU"              │  CAN_H/CAN_L, 120Ω x2     │  TWAI + replay engine │
 │  Mcu/Port/Dio/Gpt │                           │  log stored in        │
 │  Can (bxCAN)      │                           │  LittleFS (CSV)       │
 │  Spi → TFT        │                           │  web UI: play/pause,  │
 │  Signal layer     │                           │  drop ID, corrupt CRC │
 └───────────────────┘                           └───────────────────────┘
```

**Hướng mặc định: TFT gắn vào STM32.** Lý do: cluster thật là một ECU tự vẽ màn hình, và việc này cho
bạn thêm một driver MCAL nữa (**Spi**) cùng một bài toán real-time thật: vẽ màn hình không được làm
trễ việc xử lý CAN RX. Nếu sau này muốn UI đẹp (LVGL), có thể chuyển TFT sang ESP32 và STM32 chỉ gửi
"display PDU" qua CAN, nhưng khi đó phần "cluster" nằm trên ESP32 và giá trị CV giảm.

Ràng buộc RAM: F103C8 có 20 KB, framebuffer 320×240×2 = 150 KB nên **không có framebuffer**. Vẽ trực
tiếp bằng lệnh vùng (`CASET`/`PASET` + `RAMWR`), chỉ vẽ lại phần thay đổi (dirty region), font bitmap
đặt `const` trong Flash.

## 3. Các module (phần bạn tự viết)

| Lớp | Module | Việc | Liên quan track hiện tại |
|---|---|---|---|
| MCAL | Mcu, Port, Dio, Gpt | clock 72 MHz, tick 1 ms, LED cảnh báo | M1, M2 |
| MCAL | Can | bxCAN 500 k, filter chỉ nhận ID cluster cần, RX IRQ, bus-off | M3 |
| MCAL | **Spi (mới)** | SPI1 master 18 MHz, có thể thêm DMA1 ch3 | mới |
| MCAL | Uart (tùy chọn) | SLCAN để SavvyCAN thấy bus | M4 |
| Driver | `Ili9341` | init sequence, fill rect, draw glyph | mới, nằm trên Spi |
| App | `SigRx` | unpack signal theo bảng sinh từ DBC, kiểm counter/checksum, timeout | dùng DBC track D1 |
| App | `ClusterHmi` | layout màn hình, state đèn cảnh báo, chuỗi "---" khi invalid | mới |

Upward interface vẫn là `CanIf_Cbk.h` như hiện tại: `CanIf_RxIndication()` trong `app/` đẩy frame vào
một ring buffer, `SigRx_MainFunction()` (10 ms) xử lý. Không dựng lại CanIf/PduR/Com, đúng phạm vi đã
chốt ngày 2026-10-11.

## 4. Thiết kế lớp signal (phần "giải quyết vấn đề")

Mỗi signal cluster dùng có một bản ghi trạng thái:

```c
/* Per-signal runtime state, one entry per displayed signal (static, no malloc). */
typedef enum
{
    SIG_STATE_NEVER_RECEIVED = 0u,  /* show "---" since power-up               */
    SIG_STATE_VALID,                /* fresh, counter/checksum OK               */
    SIG_STATE_TIMEOUT,              /* no frame for > timeout_ms -> substitute  */
    SIG_STATE_INVALID               /* frame arrives but E2E check fails N times */
} SigRx_StateType;

typedef struct
{
    uint16  can_id;          /* e.g. 0x116                                        */
    uint16  cycle_ms;        /* nominal period from DBC GenMsgCycleTime           */
    uint16  timeout_ms;      /* usually 3..5 x cycle_ms                           */
    uint8   e2e_kind;        /* NONE / SUM_WITH_ID / CRC8_J1850                    */
    uint8   max_err;         /* consecutive E2E failures before INVALID           */
} SigRx_MsgCfgType;          /* const table in Flash, generated from the DBC      */
```

Các quyết định cần thiết kế (đây là phần nên viết vào README vì phỏng vấn sẽ hỏi):

- **Timeout = 3–5 × chu kỳ**, không phải 1×: log `GVRET_Log.csv` có `0x168` mất frame (khoảng cách 40,
  60 ms thay vì 20 ms, xem walkthrough mục 7). Timeout 1× sẽ làm đồng hồ nhấp nháy "---" liên tục.
- **Debounce theo hai chiều**: vào INVALID sau `max_err` lỗi liên tiếp, ra khỏi INVALID sau K frame tốt
  liên tiếp, tránh đèn cảnh báo chập chờn.
- **Alive counter**: chấp nhận bước tăng 1 (và wrap), báo lỗi khi counter đứng yên (ECU gửi bị treo
  nhưng vẫn phát frame) hoặc nhảy > 1 (mất frame).
- **Checksum**: dùng đúng công thức bạn đã reverse-engineer: `0x116` là tổng data + 2 byte ID,
  `0x228` là CRC-8 J1850.
- **Giá trị thay thế khi timeout**: speed về 0 và bật đèn, nhiệt độ giữ giá trị cuối và bật đèn. Ghi rõ
  lựa chọn cho từng signal, vì đây là quyết định an toàn chứ không phải chi tiết code.
- **Real-time**: CAN RX IRQ chỉ copy frame vào ring buffer (ISR < 5 µs). Vẽ TFT chạy ở background với
  ngân sách mỗi lần gọi (ví dụ tối đa 1 dirty region / 20 ms) để `SigRx_MainFunction` 10 ms không bao
  giờ trễ. Đo bằng Dio toggle + logic analyzer và ghi số đo vào README.

## 5. Phía ESP32: rest-bus simulator

- **Đã có sẵn:** `esp32/restbus_sim/` phát lại `samples/demo_drive.csv` (nhúng vào firmware) đúng timestamp qua TWAI;
  fault injection điều khiển qua console serial. Bước đặt bàn test: [p1-step1-bringup.vi.md](p1-step1-bringup.vi.md).
  Mở rộng sau: nạp `GVRET_Log.csv` từ LittleFS, web UI thay cho console.
- Điều khiển (hiện là console, sau có thể là web UI): Play/Pause, tốc độ phát, và **fault injection**:
  - bỏ hẳn một ID (test timeout),
  - làm hỏng checksum của một ID (test INVALID),
  - đóng băng alive counter (test "ECU treo"),
  - dừng toàn bộ bus (test mọi đèn cảnh báo).
- Khuyến nghị **ESP-IDF (C)** thay vì Arduino: gần môi trường automotive hơn, dùng FreeRTOS task thật.
- Lưu ý phần cứng: nếu transceiver là TJA1050/MCP2551 (5 V), chân RX về ESP32 phải qua cầu chia áp
  hoặc dùng SN65HVD230; ESP32 GPIO không chịu 5 V.

## 6. Lộ trình (mỗi bước là một PR, bạn viết code, mình review)

| Bước | Nội dung | Tiêu chí xong |
|---|---|---|
| P1.0 | Hoàn thành M2 Mcu/Gpt, M3 Can (đang làm) | loopback bxCAN chạy, tick 1 ms chính xác |
| P1.1 | Nối bus thật STM32 ↔ ESP32, ESP32 gửi 1 frame 100 ms | LED trên STM32 đổi theo data; SavvyCAN thấy cả 2 node (qua SLCAN hoặc ESP32RET) |
| P1.2 | DBC D1 xong → bảng `SigRx_MsgCfgType` (viết tay hoặc script nhỏ) | host test unpack khớp `cantools` trên cùng log |
| P1.3 | `SigRx`: timeout, counter, checksum + host test | test cho 4 trạng thái và chuyển trạng thái |
| P1.4 | Spi MCAL + `Ili9341`, vẽ số tĩnh | màn hình hiện "0 km/h" |
| P1.5 | `ClusterHmi` + replay engine trên ESP32 | đồng hồ chạy theo log thật |
| P1.6 | Fault injection + đo timing | bảng kết quả 4 kịch bản lỗi, số đo ISR/main loop |

## 7. Câu viết vào CV (mẫu)

> **CAN Instrument Cluster ECU (STM32F103, bare-metal C)** — Hand-written AUTOSAR-style MCAL
> (Mcu, Port, Dio, Gpt, Can, Spi) at register level; signal layer generated from a self-built DBC with
> deadline monitoring, alive-counter and checksum (CRC-8 J1850) validation; ESP32 rest-bus simulator
> replaying real vehicle logs with fault injection; 2.4" TFT HMI without framebuffer in 20 KB RAM.

## 8. Câu hỏi phỏng vấn project này chuẩn bị cho bạn

- Tại sao timeout không đặt bằng đúng chu kỳ gửi?
- Alive counter và CRC bảo vệ chống loại lỗi nào (theo AUTOSAR E2E: repetition, loss, delay, corruption,
  masquerade)?
- Bus-off xảy ra thì cluster làm gì, khi nào tự phục hồi (ABOM, 128 × 11 recessive bit)?
- Làm sao đảm bảo vẽ màn hình không làm mất frame CAN (FIFO bxCAN chỉ sâu 3)?
