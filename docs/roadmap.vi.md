# Lộ trình học & Project CV — Embedded SW kiểu AUTOSAR (BSW/ASW) + CAN trên STM32F103

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
- Kiến trúc phân lớp: MCAL → ECU Abstraction → Services → RTE → SWC.
- Đường đi của communication stack: `Can` → `CanIf` → `PduR` → `Com` → `RTE` → SWC (và chiều ngược lại khi TX).
- Các khái niệm sẽ thực sự implement (dạng đơn giản hóa): PDU, I-PDU và L-PDU, bảng cấu hình signal/PDU, các chế độ TX của Com (periodic / direct / mixed), deadline monitoring (timeout → giá trị thay thế), update bit.
- Chỉ đọc lý thuyết (không cần implement cho CV): EcuM/BswM, Det, NvM/MemIf, cơ bản Dcm/UDS, các profile bảo vệ E2E (CRC + alive counter).
- Tài liệu tham khảo: các tài liệu SWS AUTOSAR Classic cho Can, CanIf, PduR, Com (miễn phí trên autosar.org) — chỉ cần đọc phần API và cấu hình.

### Giai đoạn 5 — Xây dựng project CV (tuần 6–10)
Xem mục 2. Tuần cuối: README, sơ đồ kiến trúc, video/GIF demo từ SavvyCAN, báo cáo MISRA.

---

## 2. Các lựa chọn project CV

### Option A (đề xuất): "ECU CAN kiểu AUTOSAR thu nhỏ trên STM32, stack Com sinh từ DBC và phát lại log"

Câu giới thiệu để ghi vào CV (giữ tiếng Anh khi đưa vào CV):
> Designed a layered (AUTOSAR Classic–inspired) CAN ECU firmware on STM32F103: bare-metal bxCAN driver, CanIf/PduR/Com/RTE layers generated from a DBC, application SWCs, log-replay HIL setup verified in SavvyCAN; MISRA C:2012 checked, unit-tested on host.

Kiến trúc:

```
 PC side                                   STM32F103 (Blue Pill)
 ┌───────────────────────┐   UART 1M   ┌─────────────────────────────────────────┐
 │ replay.py             │────────────▶│ CddUartGw  (log frames in / out)        │
 │  reads .csv/.asc/.log │◀────────────│        │                                │
 │  real timestamps      │             │        ▼                                │
 └───────────────────────┘             │ Can (MCAL, bxCAN, loopback or normal)   │
 ┌───────────────────────┐             │ CanIf  (HRH/HTH mapping, RX indication) │
 │ SavvyCAN              │◀── capture ─│ PduR   (routing table)                  │
 │  DBC decode / graphs  │   (UART or  │ Com    (signal pack/unpack, timeouts,   │
 │  verify ECU outputs   │   CANable)  │         periodic TX)                    │
 └───────────────────────┘             │ Rte    (generated Rte_Read/Rte_Write)   │
 ┌───────────────────────┐             │ SWC: SpeedMonitor, LightCtrl, DoorLogic │
 │ dbc2c.py (codegen)    │── *.c/h ──▶ │ OS: 1 ms SysTick cooperative scheduler  │
 └───────────────────────┘             │      (1/10/100 ms tasks)                │
                                       │ Det (dev error tracer → UART)           │
                                       └─────────────────────────────────────────┘
```

Luồng dữ liệu: một file log của xe thật được phát lại với đúng timing gốc → frame đi vào STM32 → đi qua toàn bộ stack → các SWC tính toán output (ví dụ "cảnh báo quá tốc độ", "bật đèn khi trời tối và xe đang chạy", "mở khóa cửa khi va chạm/dừng xe") → Com gửi các PDU output theo chu kỳ → SavvyCAN thu lại và decode bằng **file DBC output do chính bạn viết**. Khi dừng replay → deadline monitoring của Com kích hoạt → giá trị thay thế + báo cáo Det xuất hiện trong log. Phần demo timeout này là điểm rất mạnh để nói khi phỏng vấn.

Vì sao chọn option này:
- Phù hợp ràng buộc (dùng file log, không có xe) mà vẫn là công việc embedded thật trên phần cứng của bạn.
- Thể hiện đúng sự phân tách BSW và ASW mà nhà tuyển dụng như Bosch/Continental/FPT/LG VS tìm kiếm.
- Sinh code từ DBC mô phỏng cách các tool cấu hình AUTOSAR thật (DaVinci, EB tresos) sinh ra `Com_Cfg.c`.
- Mở rộng được: các phần thêm tùy chọn gồm E2E (CRC8 SAE-J1850 + alive counter), UDS ReadDataByIdentifier (0x22) trên một ID chẩn đoán, thêm một Blue Pill thứ hai làm node thật trên bus.

Cấu trúc repository đề xuất:

```
mini-autosar-can-ecu/
├── bsw/
│   ├── mcal/can/        Can.c, Can.h, Can_Cfg.h           (bxCAN register-level)
│   ├── ecual/canif/     CanIf.c, CanIf_Cfg.c
│   ├── services/pdur/   PduR.c, PduR_Cfg.c
│   ├── services/com/    Com.c, Com_Cfg.c (generated)
│   ├── services/det/    Det.c
│   └── os/              Os.c (SysTick scheduler)
├── rte/                 Rte.c, Rte_<Swc>.h (generated)
├── asw/                 SpeedMonitor.c, LightCtrl.c, DoorLogic.c
├── cdd/uartgw/          CddUartGw.c (frame bridge to PC)
├── config/              input.dbc, output.dbc, ecu_config.yaml
├── tools/               dbc2c.py, replay.py, verify.py (cantools)
├── test/host/           Unity tests for Com pack/unpack, PduR, timeouts
├── docs/                architecture.md, diagrams, SavvyCAN screenshots
└── CMakeLists.txt       arm-none-eabi + host test targets
```

Các mốc:
1. Module `Com` pack/unpack đã test trên PC + bộ sinh code `dbc2c.py` (tái sử dụng kết quả giai đoạn 2).
2. Driver `Can` ở chế độ loopback + `CddUartGw`; thấy được frame trên PC.
3. Đường đi `CanIf` → `PduR` → `Com` → `Rte`; một SWC đọc được một signal.
4. Phát lại một log thật, chạy 3 SWC, các PDU output được decode trong SavvyCAN.
5. Deadline monitoring, Det, phục hồi bus-off; kiểm tra MISRA (cppcheck `--addon=misra`), CI trên GitHub Actions (host test + cross build).
6. README kèm sơ đồ, kết quả, GIF demo.

### Option B (nhẹ hơn, chỉ chạy trên PC): "Công cụ phân tích log CAN & reverse-engineer signal"
Công cụ C++/Python đọc log SavvyCAN/candump + DBC, decode signal, phát hiện bất thường (thiếu frame định kỳ, counter nhảy cóc, checksum sai, bus load) và xuất báo cáo. Làm nhanh hơn nhưng thể hiện rất ít kỹ năng embedded/BSW — chỉ nên dùng làm công cụ phụ bên trong Option A (`tools/verify.py`), không nên là project chính trong CV.

**Đề xuất: Option A**, gộp phần analyzer của Option B vào làm công cụ kiểm tra.

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

/* Returns the raw value; caller applies factor/offset in the RTE/SWC layer. */
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
