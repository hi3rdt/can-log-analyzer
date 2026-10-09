# Lộ trình học & Project CV — Communication stack CAN phần BSW kiểu AUTOSAR trên STM32F103

Người học: hi3rdt · Ngày: 2026-10-09
Phần cứng: STM32F103C8T6 (Blue Pill, bxCAN) · Công cụ: SavvyCAN, file log CAN (offline)

> Bản tiếng Việt của [roadmap.md](roadmap.md). Code, tên module và thuật ngữ kỹ thuật giữ nguyên tiếng Anh.

---

## 0. Các đặc điểm phần cứng quyết định toàn bộ kế hoạch

| Ràng buộc | Ảnh hưởng | Quyết định |
|---|---|---|
| Trên F103, **USB và bxCAN dùng chung vùng packet SRAM 512 byte** (RM0008) | Không thể chạy USB-CDC và CAN cùng lúc | Kết nối với PC qua **UART** bằng mạch USB-UART (CH340/CP2102), không dùng USB native |
| Chân CAN mặc định PA11/PA12 trùng với chân USB | Xung đột với điện trở kéo lên D+ (R10 1.5k trên PA12) của Blue Pill | Remap CAN sang **PB8 (RX) / PB9 (TX)** (`AFIO_MAPR CAN_REMAP = 0b10`) |
| bxCAN cần transceiver ngoài | Không có transceiver thì không có bus | Ưu tiên **SN65HVD230** (3.3 V); TJA1050 (5 V) vẫn dùng được vì PB8/PB9 chịu được 5 V, nhưng cần nguồn 5 V |
| Chỉ có 1 node + file log | Không có node khác trên bus | Dùng chế độ **Loopback / Silent-Loopback** của bxCAN ở giai đoạn 1–3; thêm node thứ 2 sau (tùy chọn) |
| 20 KB RAM / 64 KB Flash | Chỉ cấp phát tĩnh | Không dùng `malloc`, buffer PDU kích thước cố định, bảng cấu hình đặt `const` (nằm trong Flash) |

---

## 1. Lộ trình (≈10 tuần, ~8–10 giờ/tuần)

### Giai đoạn 1 — Nền tảng CAN với SavvyCAN (tuần 1–2)
- Cấu trúc frame: SOF, ID 11/29-bit, RTR, DLC, data, CRC, ACK, EOF; bit stuffing; cơ chế arbitration; error frame và cơ chế fault confinement theo TEC/REC (Error Active → Error Passive → Bus-Off).
- Bit timing: TQ, SYNC/PROP/PHASE1/PHASE2, sample point (~87.5% cho 500 kbit/s trong automotive).
- Thực hành SavvyCAN với log công khai (chưa cần phần cứng):
  - Mở file log (GVRET CSV / candump / ASC), dùng các công cụ **Frame Info**, **Graphing**, **Flow View**, **Sniffer**.
  - Nạp file **DBC**, decode signal, vẽ đồ thị ví dụ tốc độ xe theo thời gian.
  - Reverse-engineer một signal chưa biết từ log (tìm counter, checksum, byte thay đổi).
- Sản phẩm: ghi chú ngắn + ảnh chụp màn hình 3 signal đã decode (sẽ đưa vào README của project sau).
- Nguồn log/DBC: `commaai/opendbc` (file DBC của xe thật), dữ liệu test của `linklayer/cantools`, log mẫu của SavvyCAN. Chọn một hãng xe có cả DBC lẫn log.

### Giai đoạn 2 — DBC & tính toán signal bằng C (tuần 2–3)
- Đánh số bit Intel (little-endian) và Motorola (big-endian) — nguồn lỗi decode số 1.
- Giá trị vật lý = raw × factor + offset; signal có dấu; min/max; signal multiplexed.
- Viết thư viện C chạy trên PC `CanSig_Unpack/Pack` + unit test (Unity hoặc CppUTest), đối chiếu kết quả với `cantools` (Python) trên cùng một file log.

### Giai đoạn 3 — Driver bxCAN bare-metal trên STM32 (tuần 3–5)
- Cây clock (HSE 8 MHz → 72 MHz, APB1 = 36 MHz), bit timing cho 500 kbit/s: prescaler 4, BS1 = 15, BS2 = 2, SJW = 1 → 36 MHz / 4 / 18 TQ = 500 kbit/s, sample point 16/18 = 88.9%.
- Chuyển chế độ init/sleep/normal (INRQ/INAK, SLEEP/SLAK), chế độ loopback để tự kiểm tra.
- 3 TX mailbox, 2 RX FIFO (mỗi FIFO sâu 3 frame), 14 filter bank: chế độ ID-list và mask.
- Nhận frame bằng ngắt (IRQ USB_LP_CAN1_RX0), ngắt TX-complete, xử lý lỗi/bus-off (ABOM).
- Ghi log mọi frame qua UART theo định dạng **giống GVRET / text SLCAN** để SavvyCAN hoặc script Python thu được.
- Sản phẩm: STM32 ở chế độ loopback nhận lại chính frame nó gửi; SavvyCAN hiển thị được các frame đó.

### Giai đoạn 4 — Ánh xạ khái niệm AUTOSAR vào code của bạn (tuần 4–6, chạy song song)
- Kiến trúc phân lớp: MCAL → ECU Abstraction → Services → RTE → SWC. Cần hiểu tất cả các lớp, nhưng **chỉ implement phần BSW** (MCAL, ECU Abstraction, Services).
- Đường đi của communication stack: `Can` → `CanIf` → `PduR` → `Com` → (RTE → SWC trong ECU thật). Trong project này, API public của Com (`Com_ReceiveSignal` / `Com_SendSignal`) là ranh giới trên cùng.
- Các khái niệm sẽ thực sự implement (dạng đơn giản hóa): PDU, I-PDU và L-PDU, bảng cấu hình signal/PDU, các chế độ TX của Com (periodic / direct / mixed), deadline monitoring (timeout → giá trị thay thế), update bit, PDU gateway của PduR, signal gateway của Com.
- Chỉ đọc lý thuyết (không cần implement cho CV): RTE và SWC/runnable, EcuM/BswM, NvM/MemIf, cơ bản Dcm/UDS, các profile bảo vệ E2E (CRC + alive counter).
- Tài liệu tham khảo: các tài liệu SWS AUTOSAR Classic cho Can, CanIf, PduR, Com (miễn phí trên autosar.org) — chỉ cần đọc phần API và cấu hình.

### Giai đoạn 5 — Xây dựng project CV (tuần 6–10)
Xem mục 2. Tuần cuối: README, sơ đồ kiến trúc, video/GIF demo từ SavvyCAN, báo cáo MISRA.

---

## 2. Project CV

### Quyết định phạm vi (2026-10-09): chỉ làm BSW, bỏ RTE/ASW
Project chỉ implement **communication stack phần BSW**. RTE và các SWC ứng dụng được bỏ có chủ đích: trong công nghiệp, chúng được sinh ra bởi bộ tool (Vector DaVinci, EB tresos, ETAS ISOLAR) mà project cá nhân không có, nên một "RTE" tự viết tay dễ bị hỏi "không có tool thì làm thế nào?". Một tập con API theo SWS của Can/CanIf/PduR/Com, viết tay và được cấu hình bằng một script nhỏ sinh code từ DBC, thì trung thực, giải thích được từng dòng, và đúng là thứ mà các vị trí BSW integration/development làm hằng ngày.

### Option A (đã chọn): "Communication stack CAN phần BSW kiểu AUTOSAR trên STM32 (gateway ECU), cấu hình từ DBC, kiểm chứng bằng phát lại log"

Câu giới thiệu để ghi vào CV (giữ tiếng Anh khi đưa vào CV):
> Implemented an AUTOSAR Classic–inspired BSW CAN communication stack on STM32F103 without commercial tooling: register-level bxCAN driver, CanIf, PduR (PDU gateway), Com (signal pack/unpack, signal gateway, deadline monitoring, periodic TX) and Det; configuration generated from DBC by a Python tool; verified by replaying real-vehicle CAN logs and decoding the output in SavvyCAN; MISRA C:2012 oriented, unit-tested on host, CI on GitHub Actions.

Kiến trúc:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐   UART 1M   ┌─────────────────────────────────────────┐
 │ replay.py             │────────────▶│ CddUartGw  (log frames in / out)        │
 │  reads .csv/.asc/.log │◀────────────│        │                                │
 │  real timestamps      │             │        ▼                                │
 └───────────────────────┘             │ Can    (MCAL, bxCAN, loopback/normal)   │
 ┌───────────────────────┐             │ CanIf  (HRH/HTH mapping, RX indication) │
 │ SavvyCAN              │◀── capture ─│ PduR   (routing table, PDU gateway)     │
 │  DBC decode / graphs  │   (UART or  │ Com    (signal pack/unpack, signal      │
 │  verify ECU outputs   │   CANable)  │         gateway, deadline monitoring,   │
 └───────────────────────┘             │         periodic TX)                    │
 ┌───────────────────────┐             │ ComTest (thin hook calling the public   │
 │ dbc2c.py (codegen)    │── *.c/h ──▶ │          Com_ReceiveSignal/SendSignal)  │
 └───────────────────────┘             │ Os     (1 ms SysTick cooperative tasks) │
                                       │ Det    (dev error tracer → UART)        │
                                       └─────────────────────────────────────────┘
```

Luồng dữ liệu: một file log của xe thật được phát lại với đúng timing gốc → frame đi vào STM32 → `Can` → `CanIf` → `PduR`. PduR hoặc chuyển nguyên PDU sang một PDU TX (PDU gateway, ví dụ đổi ID), hoặc đưa lên `Com`. `Com` unpack signal, thực hiện **signal gateway** (chép các signal đầu vào được chọn sang các PDU output định nghĩa trong **file DBC output do chính bạn viết**) và gửi các PDU output theo chu kỳ → SavvyCAN thu lại và decode bằng DBC output đó. Khi dừng replay → deadline monitoring của Com kích hoạt → giá trị thay thế + báo cáo Det xuất hiện trong log. Phần demo timeout này là điểm rất mạnh để nói khi phỏng vấn. Không cần logic ứng dụng: gateway ECU là một sản phẩm hợp lệ chỉ gồm BSW.

Vì sao chọn phạm vi này:
- Phù hợp ràng buộc (dùng file log, không có xe) mà vẫn là công việc embedded thật trên phần cứng của bạn.
- Mọi dòng code đều do bạn viết và giải thích được; không có "RTE giả" đóng vai output của tool.
- Script sinh code từ DBC mô phỏng việc các tool cấu hình làm (`Com_Cfg.c`, `PduR_Cfg.c`), nên bạn có thể giải thích code sinh ra khi phỏng vấn.
- Mở rộng được: các phần thêm tùy chọn gồm E2E (CRC8 SAE-J1850 + alive counter) trong Com, UDS ReadDataByIdentifier (0x22) qua CanTp/Dcm tối giản, thêm một Blue Pill thứ hai làm node thật trên bus.

Repository: `github.com/hi3rdt/can-log-analyzer`. Cấu trúc:

```
can-log-analyzer/
├── bsw/
│   ├── include/         Std_Types.h, ComStack_Types.h, Platform_Types.h
│   ├── mcal/can/        Can.c, Can.h, Can_Cfg.h           (bxCAN register-level)
│   ├── ecual/canif/     CanIf.c, CanIf_Cfg.c
│   ├── services/pdur/   PduR.c, PduR_Cfg.c
│   ├── services/com/    Com.c, Com_Cfg.c (generated)
│   ├── services/det/    Det.c
│   └── os/              Os.c (SysTick scheduler)
├── cdd/uartgw/          CddUartGw.c (frame bridge to PC)
├── platform/stm32f103/  startup, linker script, register definitions
├── app/                 main.c + ComTest hook (no RTE, no SWC)
├── config/              input.dbc, output.dbc, gateway.yaml
├── tools/               dbc2c.py, replay.py, verify.py (cantools)
├── test/host/           host unit tests for Com, PduR, CanIf, timeouts
├── docs/                roadmap, architecture.md, SavvyCAN screenshots
└── CMakeLists.txt       arm-none-eabi firmware + host test targets
```

Các mốc (mỗi mốc một PR):
1. Khung repo: CMake (host test + firmware arm-none-eabi), startup/linker script cho STM32F103, `Std_Types.h`/`ComStack_Types.h`, Det, CI.
2. Đường Com trên PC: `CanIf` → `PduR` → `Com`, `dbc2c.py` sinh `Com_Cfg.c` / `PduR_Cfg.c` / `CanIf_Cfg.c`, kèm host unit test.
3. Driver `Can` cho bxCAN (loopback trước) + `CddUartGw`; thấy được frame trên PC.
4. Os scheduler, Com periodic TX + deadline monitoring, signal gateway; phát lại log thật, decode output trong SavvyCAN.
5. Phục hồi bus-off, kiểm tra MISRA (cppcheck `--addon=misra`), README kèm sơ đồ, kết quả, GIF demo.

### Option B (không chọn làm project chính): "Công cụ phân tích log CAN & reverse-engineer signal"
Công cụ C++/Python đọc log SavvyCAN/candump + DBC, decode signal, phát hiện bất thường (thiếu frame định kỳ, counter nhảy cóc, checksum sai, bus load) và xuất báo cáo. Chỉ giữ lại làm công cụ phụ bên trong Option A (`tools/verify.py`).

---

## 3. Code tham khảo — Com unpack signal (byte order Intel)

```c
/* Com_Signal.c - generic Intel (little-endian) signal extraction.
 * MISRA C:2012 notes: fixed-width types, no dynamic memory, single exit point. */
#include <stdint.h>

typedef struct
{
    uint8_t  startBit;   /* DBC start bit (LSB position for Intel) */
    uint8_t  bitLength;  /* 1..32 */
    uint8_t  isSigned;   /* 0 = unsigned, 1 = two's complement */
} Com_SignalCfgType;

/* Returns the raw value; caller (Com user) applies factor/offset. */
uint32_t Com_UnpackIntel(const uint8_t *pdu, const Com_SignalCfgType *cfg)
{
    uint64_t frame = 0U;
    uint32_t raw;
    uint8_t  i;

    /* Assemble the 8-byte payload into a 64-bit word, byte 0 = LSB */
    for (i = 0U; i < 8U; i++)
    {
        frame |= ((uint64_t)pdu[i]) << (8U * i);
    }

    raw = (uint32_t)((frame >> cfg->startBit) &
                     ((cfg->bitLength == 32U) ? 0xFFFFFFFFULL
                                              : ((1ULL << cfg->bitLength) - 1ULL)));

    /* Sign-extend if required (stored as uint32_t, reinterpret at the RTE) */
    if ((cfg->isSigned != 0U) && (cfg->bitLength < 32U) &&
        ((raw & (1UL << (cfg->bitLength - 1U))) != 0U))
    {
        raw |= ~((1UL << cfg->bitLength) - 1UL);
    }

    return raw;
}
```

Các edge case cần test: `bitLength` = 1 và 32, signal nằm vắt qua ranh giới byte, DLC < 8 (bên gọi phải đệm 0), giá trị nhỏ nhất của signal có dấu, và signal Motorola (cần hàm riêng — start bit trong DBC là MSB theo kiểu đánh số "răng cưa").

---

## 4. Danh sách mua sắm (tùy chọn, giá rẻ)
- Module transceiver SN65HVD230 ×2 (~1 USD/cái) — cần cho bất kỳ bus thật nào.
- Mạch USB-UART (CP2102/CH340) — cần cho Option A.
- Một Blue Pill thứ hai — làm "gateway" hoặc ECU thứ hai; giúp bạn thoát khỏi chế độ loopback.
- USB-CAN loại CANable / tương thích candleLight (~15–25 USD) — giúp SavvyCAN nghe trực tiếp bus thật (kiểm tra thiết bị có được hỗ trợ trên hệ điều hành của bạn trong hộp thoại kết nối của SavvyCAN trước khi mua).
- Điện trở termination 120 Ω ở cả hai đầu bus.
