# Lộ trình học & Project CV — MCAL kiểu AUTOSAR + DBC với SavvyCAN trên STM32F103

Người học: hi3rdt · Ngày: 2026-10-09 · Cập nhật: 2026-10-11 (phạm vi: MCAL + DBC với SavvyCAN)
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

### Giai đoạn 2 — Tự viết DBC & phép tính signal đằng sau nó (tuần 2–3, nhánh DBC)
- Tự viết DBC cho một log thật trong SavvyCAN (bài D1): cú pháp `BO_` / `SG_`, start bit và độ dài, `@1` hay `@0`, `+`/`-`, factor/offset, bảng `VAL_`, chú thích `CM_`, thuộc tính chu kỳ gửi. Kiểm DBC với log bằng cantools.
- Đánh số bit Intel (little-endian) và Motorola (big-endian) — nguồn lỗi decode số 1.
- Giá trị vật lý = raw × factor + offset; signal có dấu; min/max; signal multiplexed.
- Viết thư viện C chạy trên PC `CanSig_Unpack/Pack` + unit test (Unity hoặc CppUTest), đối chiếu kết quả với `cantools` (Python) trên cùng một file log.

### Giai đoạn 3 — MCAL mức thanh ghi trên STM32: Port/Dio, Mcu/Gpt, rồi bxCAN (tuần 3–5)
- GPIO trước (bài 1): CRL/CRH mỗi chân 4 bit CNF|MODE, IDR/ODR, BSRR ghi nguyên tử, bật clock qua RCC. Sau đó Mcu (HSE → PLL 72 MHz, wait state của Flash) và Gpt (TIM2 làm tick 1 ms).
- Cây clock (HSE 8 MHz → 72 MHz, APB1 = 36 MHz), bit timing cho 500 kbit/s: prescaler 4, BS1 = 15, BS2 = 2, SJW = 1 → 36 MHz / 4 / 18 TQ = 500 kbit/s, sample point 16/18 = 88.9%.
- Chuyển chế độ init/sleep/normal (INRQ/INAK, SLEEP/SLAK), chế độ loopback để tự kiểm tra.
- 3 TX mailbox, 2 RX FIFO (mỗi FIFO sâu 3 frame), 14 filter bank: chế độ ID-list và mask.
- Nhận frame bằng ngắt (IRQ USB_LP_CAN1_RX0), ngắt TX-complete, xử lý lỗi/bus-off (ABOM).
- Driver Uart đẩy mọi frame theo **giao thức text LAWICEL/SLCAN**, SavvyCAN đọc trực tiếp qua kết nối LAWICEL serial.
- Sản phẩm: STM32 ở chế độ loopback nhận lại chính frame nó gửi; SavvyCAN hiển thị trực tiếp các frame đó và decode bằng DBC của bạn.

### Giai đoạn 4 — Ánh xạ khái niệm AUTOSAR vào code của bạn (tuần 4–6, chạy song song)
- Kiến trúc phân lớp: MCAL → ECU Abstraction → Services → RTE → SWC. Cần hiểu tất cả các lớp, nhưng **chỉ implement MCAL** (mục 2). Các lớp phía trên chỉ học lý thuyết: driver Can dừng ở các callback trong `CanIf_Cbk.h`, do một test harness nhỏ trong `app/` viết phần thân.
- Đường đi của communication stack trong ECU thật: `Can` → `CanIf` → `PduR` → `Com` → RTE → SWC. Trong project này, ranh giới là các callback của driver Can (`CanIf_Cbk.h`).
- Các khái niệm cần hiểu để trả lời phỏng vấn (không implement): PDU, I-PDU và L-PDU, bảng cấu hình signal/PDU, các chế độ TX của Com (periodic / direct / mixed), deadline monitoring (timeout → giá trị thay thế), update bit, PDU gateway của PduR, signal gateway của Com. Mốc 2 đã làm một phiên bản đơn giản của các phần này; code vẫn còn trong lịch sử git (đã xóa 2026-10-11).
- Chỉ đọc lý thuyết (không cần implement cho CV): RTE và SWC/runnable, EcuM/BswM, NvM/MemIf, cơ bản Dcm/UDS, các profile bảo vệ E2E (CRC + alive counter).
- Tài liệu tham khảo: các tài liệu SWS AUTOSAR Classic cho Can, CanIf, PduR, Com (miễn phí trên autosar.org) — chỉ cần đọc phần API và cấu hình.

### Giai đoạn 5 — Xây dựng project CV (tuần 6–10)
Xem mục 2. Tuần cuối: README, sơ đồ kiến trúc, video/GIF SavvyCAN decode frame của Blue Pill bằng DBC của bạn, báo cáo MISRA.

---

## 2. Project CV

### Phạm vi hiện tại (2026-10-11): MCAL + DBC với SavvyCAN
Project chỉ có hai nhánh:

1. **Nhánh MCAL.** Một **MCAL kiểu AUTOSAR viết tay** trên thanh ghi STM32F103: Port + Dio (GPIO), Mcu (cây clock) + Gpt (timer phần cứng), Can (bxCAN), một driver Uart đẩy frame lên SavvyCAN theo giao thức text LAWICEL/SLCAN, và Adc nếu còn thời gian. Mọi driver dùng tên API theo SWS (`Port_Init`, `Dio_WriteChannel`, `Mcu_InitClock`, `Gpt_StartTimer`, `Can_Write`, ...), nhận một struct cấu hình `const`, báo lỗi phát triển qua Det với module ID chính thức, và chỉ chạm thanh ghi qua con trỏ base (`bsw/mcal/reg/Mcal_Reg.h`). Firmware gán các con trỏ đó vào địa chỉ thật (`platform/stm32f103/Mcal_RegMap.c`), còn host test gán vào RAM (`test/host/stubs/Mcal_FakeRegs.c`), nên driver nào cũng unit test được trên PC.
2. **Nhánh DBC.** Reverse-engineer log CAN thật trong SavvyCAN rồi ghi kết quả thành file DBC: chu kỳ message, alive counter, CRC-8 SAE J1850 và checksum kiểu tổng, giá trị vật lý 16 bit, byte order và dấu. DBC được kiểm với log bằng cantools, sau đó dùng trong SavvyCAN để decode các frame mà Blue Pill gửi qua driver Can và Uart.

**Ngoài phạm vi:** các BSW service mới (CanIf, PduR, Com, CanTp, Dcm, NvM, ...), RTE và ASW/SWC. Trong công nghiệp, các lớp này được cấu hình và sinh code bằng tool (Vector DaVinci, EB tresos, ETAS ISOLAR) mà project cá nhân không có. MCAL thì khác: hãng chip giao nó dưới dạng driver C thường, và không hãng nào làm MCAL cho F103, nên tự viết vừa đáng tin vừa là cách tốt nhất để thể hiện kỹ năng thanh ghi.

**Code CanIf/PduR/Com của mốc 2 đã bị xóa (2026-10-11)**, cùng với cấu hình sinh ra và tool `dbc2c.py`; code vẫn còn trong lịch sử git. Phụ thuộc lên trên duy nhất của driver Can là `bsw/include/CanIf_Cbk.h` (`CanIf_RxIndication`, `CanIf_TxConfirmation`), do test harness trong `app/` viết phần thân.

Ghi trong CV là "AUTOSAR-style MCAL, hand-written at register level", không bao giờ ghi "AUTOSAR-compliant": project chỉ làm một tập con API, không có cấu hình ARXML và không qua conformance test.

Câu giới thiệu để ghi vào CV (giữ tiếng Anh khi đưa vào CV):
> Hand-wrote an AUTOSAR-style MCAL for STM32F103 at register level (Mcu, Port, Dio, Gpt, Can/bxCAN, Uart) with SWS API names, const configuration and Det error reporting, unit-tested on host against fake registers with CI on GitHub Actions; reverse-engineered real-vehicle CAN logs in SavvyCAN into a DBC (alive counters, CRC-8 SAE J1850 and sum checksums, verified with cantools) and used it to decode the driver's live traffic streamed to SavvyCAN; MISRA C:2012 oriented.

Kiến trúc:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐  UART (SLCAN) ┌─────────────────────────────────────────┐
 │ SavvyCAN              │◀──────────────│ app/main.c  (test harness, no RTE/SWC)  │
 │  LAWICEL connection   │               │        │                                │
 │  your DBC decodes the │               │        ▼                                │
 │  frames live          │               │ MCAL                                    │
 └───────────────────────┘               │  Can   (bxCAN, loopback/normal)         │
 ┌───────────────────────┐               │  Uart  (SLCAN frame stream)             │
 │ dbc/*.dbc (yours)     │               │  Port / Dio  (pins, LED)                │
 │  checked by cantools  │               │  Mcu / Gpt   (72 MHz clock, 1 ms tick)  │
 │  against real logs    │               │  Adc   (optional)                       │
 └───────────────────────┘               │ Det    (development error tracer)       │
                                         └─────────────────────────────────────────┘
```

Cấu trúc repo (đích đến):

```
can-log-analyzer/
├── bsw/
│   ├── include/         Std_Types.h, Can_GeneralTypes.h, CanIf_Cbk.h (callbacks, implemented in app/)
│   ├── mcal/reg/        Mcal_Reg.h (register structs + base pointers)
│   ├── mcal/port, dio/  Port.c, Dio.c                     (GPIO)
│   ├── mcal/mcu, gpt/   Mcu.c, Gpt.c                      (clock tree, timer)
│   ├── mcal/can/        Can.c, Can_Hw.c, Can.h            (bxCAN register-level)
│   ├── mcal/uart/       Uart.c                            (USART1, SLCAN stream)
│   └── services/det/    Det.c
├── dbc/                 DBC files you write from the logs (DBC track)
├── platform/stm32f103/  startup, linker script, Mcal_RegMap.c (real addresses)
├── app/                 main.c test harness (no RTE, no SWC)
├── samples/             demo drive log (GVRET CSV + candump)
├── tools/               check_dbc.py, demo log generator (cantools)
├── test/host/           host unit tests, fake MCAL registers
└── docs/                roadmap, SavvyCAN guides, exercises
```

Các mốc:
1. Khung repo: CMake (host test + firmware arm-none-eabi), startup/linker script, Det, CI. Đã xong.
2. CanIf → PduR → Com với tool sinh code từ DBC. Đã xong, rồi bị xóa khi thu hẹp phạm vi (còn trong lịch sử git).
3. Nền MCAL: lớp thanh ghi, Port + Dio (bài M1), Mcu + Gpt (bài M2); `main.c` nháy LED bằng `Port_Init` / `Dio_FlipChannel` theo tick của Gpt.
4. Driver Can cho bxCAN (bài M3, loopback trước) + Uart đẩy frame dạng SLCAN (bài M4); SavvyCAN thấy frame của Blue Pill trực tiếp qua kết nối LAWICEL.
5. Nhánh DBC: DBC cho `GVRET_Log.csv` (bài D1) và cho `0x2F0` trong log demo (bài 4 của hướng dẫn SavvyCAN), mỗi file kiểm bằng cantools; Blue Pill gửi các frame mô tả trong DBC của bạn và SavvyCAN decode chúng.
6. Xử lý bus-off, Adc (tùy chọn), kiểm tra MISRA (cppcheck `--addon=misra`), README kèm sơ đồ, GIF demo.

Các bài tập có hướng dẫn (bạn tự viết code và DBC, Claude review):

| # | Nhánh | Nội dung | Hướng dẫn | Kiểm tra |
|---|---|---|---|---|
| M1 | MCAL | Port, Dio | [ex01-port-dio.vi.md](exercises/ex01-port-dio.vi.md) | `test_port_dio` (`MCAL_PORT_DIO_EXERCISE_TESTS`) |
| M2 | MCAL | Mcu, Gpt | sắp có | sắp có |
| M3 | MCAL | Can (bxCAN) | [ex03-can-bxcan.vi.md](exercises/ex03-can-bxcan.vi.md) | `test_can_hw` (`CAN_HW_EXERCISE_TESTS`) |
| M4 | MCAL | Uart + SLCAN lên SavvyCAN | sắp có | sắp có |
| M5 | MCAL | Adc (tùy chọn) | sắp có | sắp có |
| D1 | DBC | DBC cho `GVRET_Log.csv` | [dbc01-gvret-log.vi.md](exercises/dbc01-gvret-log.vi.md) | cantools + SavvyCAN |

### Lịch sử phạm vi
- 2026-10-09: chọn Option A (ECU gateway CAN trên STM32, kiểm chứng bằng phát lại log), thu hẹp còn BSW (Can, CanIf, PduR, Com, Det), không RTE/ASW. Mốc 1 và 2 được làm theo phạm vi này.
- 2026-10-11: thêm MCAL bên dưới stack ("MCAL + CAN stack").
- 2026-10-11: thu hẹp còn MCAL + DBC với SavvyCAN như mô tả ở trên. CanIf/PduR/Com, `gen/` và `dbc2c.py` bị xóa khỏi repo theo yêu cầu của hi3rdt.
- Option B (một tool phân tích log độc lập) chưa bao giờ là project chính; phần hữu ích của nó nay là nhánh DBC.

---

## 3. Code tham khảo — unpack signal (byte order Intel), phép tính đằng sau một dòng `SG_` trong DBC

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
- Mạch USB-UART (CP2102/CH340) — cần cho đường Uart SLCAN lên SavvyCAN.
- Một Blue Pill thứ hai — làm "gateway" hoặc ECU thứ hai; giúp bạn thoát khỏi chế độ loopback.
- USB-CAN loại CANable / tương thích candleLight (~15–25 USD) — giúp SavvyCAN nghe trực tiếp bus thật (kiểm tra thiết bị có được hỗ trợ trên hệ điều hành của bạn trong hộp thoại kết nối của SavvyCAN trước khi mua).
- Điện trở termination 120 Ω ở cả hai đầu bus.
