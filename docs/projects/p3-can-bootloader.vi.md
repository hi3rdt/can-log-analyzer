# Project 3 — CAN Flash Bootloader + mini-FOTA (STM32 ECU, ESP32 OTA master)

Người học: hi3rdt · Ngày: 2026-10-18 · Trạng thái: thiết kế / brainstorm, chưa bắt đầu
Phần cứng: STM32F103C8 (Blue Pill, bxCAN) · ESP32 (TWAI, WiFi) · 2 transceiver CAN · TFT 2.4"

> Code, tên module, thuật ngữ kỹ thuật giữ nguyên tiếng Anh. Làm sau
> [Project 1](p1-instrument-cluster.vi.md): P1 cho bạn bus CAN thật và MCAL ổn định, P3 dùng lại toàn bộ.

---

## 1. Bài toán thực tế

Mỗi xe hiện đại có 30–100 ECU, và phần mềm của chúng phải được cập nhật trong suốt vòng đời xe:
ở cuối dây chuyền (EOL flashing), ở xưởng dịch vụ (tester nối cổng OBD), và ngày càng nhiều là
**qua mạng (FOTA)**: TCU/gateway tải image từ server rồi tự flash các ECU phía sau qua CAN. Cả ba
trường hợp dùng cùng một cơ chế: **Flash Bootloader (FBL)** trên ECU + **UDS (ISO 14229) trên ISO-TP
(ISO 15765-2)**. Vector bán FBL như một sản phẩm riêng; OEM nào cũng có "flash sequence" của mình.

Vấn đề cốt lõi mà một FBL phải giải quyết, và cũng là thứ nhà tuyển dụng muốn nghe:

1. **Không bao giờ "brick" ECU**: mất điện, rút dây, image hỏng giữa chừng thì ECU vẫn phải vào
   bootloader và chờ flash lại.
2. **Chỉ chấp nhận image đúng**: đúng phần cứng, nguyên vẹn (CRC), và (nâng cao) có chữ ký hợp lệ.
3. **Chỉ người có quyền mới flash được**: Security Access (seed/key).
4. **Nhanh và đúng timing**: tuân thủ P2/P2*/S3 của UDS, tận dụng băng thông CAN.

Trong project này: **STM32 = ECU đích** (bootloader + application), **ESP32 = TCU/OTA master**
(nhận image qua WiFi, đóng vai UDS tester), **TFT = màn hình "Software update"** như trên head unit.

## 2. Kiến trúc hệ thống

```
  Laptop / phone                ESP32 "TCU / OTA master"                 STM32 "Target ECU"
 ┌────────────┐  WiFi/HTTP   ┌──────────────────────────────┐   CAN    ┌──────────────────────────┐
 │ upload .bin│─────────────►│ HTTP server + manifest check │ 500 kbit │ 0x08000000  Bootloader   │
 │ + manifest │              │ LittleFS: new + last-good    │◄────────►│  Can, CanTp, Dcm(subset) │
 └────────────┘              │ UDS client (flash sequencer) │ 0x7E0 ►  │  Fls, Crc, Wdg           │
                             │ ISO-TP client  │ TWAI        │ ◄ 0x7E8  │ 0x08004000  Application  │
                             │ TFT UI: consent, progress    │          │  (Project 1 cluster app) │
                             └──────────────────────────────┘          │ 0x0800FC00  Metadata     │
                                                                       └──────────────────────────┘
```

Điểm hay khi nối với P1: **application được flash chính là firmware cluster của P1**. Demo cuối là:
cluster đang chạy, upload v1.1 lên ESP32, TFT hỏi "Install update?", flash xong cluster khởi động lại
với version mới hiện trên màn hình.

## 3. Bố trí Flash của STM32F103C8 (64 KB, page 1 KB)

| Vùng | Địa chỉ | Kích thước | Nội dung |
|---|---|---|---|
| Bootloader | `0x0800_0000`–`0x0800_3FFF` | 16 KB | FBL, khoá ghi bằng option byte WRP |
| Application | `0x0800_4000`–`0x0800_FBFF` | 47 KB | vector table riêng ở đầu vùng |
| Metadata | `0x0800_FC00`–`0x0800_FFFF` | 1 KB | app valid flag, CRC, version, đếm số lần flash |

```c
/* Written by the bootloader ONLY after the whole image has been verified.
 * Erased first when programming starts -> an interrupted update leaves no valid flag. */
typedef struct
{
    uint32 magic;          /* 0xA5A5C3C3 when valid, 0xFFFFFFFF when erased        */
    uint32 app_size;       /* bytes actually programmed                            */
    uint32 app_crc32;      /* STM32 HW CRC unit, poly 0x04C11DB7                    */
    uint32 sw_version;     /* e.g. 0x00010100 = 1.1.0, readable via DID 0xF189     */
    uint32 hw_id;          /* must match the board, checked before erase           */
    uint32 program_count;  /* informational, readable via DID                      */
    uint32 meta_crc32;     /* CRC over the fields above                            */
} Fbl_AppMetaType;
```

**Brainstorm: A/B hay single-slot?** Xe hiện đại (ECU lớn) dùng A/B: ghi vào bank không chạy, lỗi thì
chạy lại bank cũ. Với 64 KB thì A/B chỉ còn ~23 KB mỗi slot, không đủ cho app có TFT. Lựa chọn hợp lý
cho phần cứng này: **single-slot trên ECU, rollback ở cấp hệ thống**: ESP32 luôn giữ "last-good image"
trong LittleFS; nếu flash bản mới thất bại hoặc app mới không báo "boot OK" thì ESP32 flash lại bản cũ.
Đây cũng là cách nhiều kiến trúc OTA thật làm với ECU nhỏ (gateway giữ bản dự phòng). Ghi trade-off
này vào README; nó là câu trả lời phỏng vấn tốt. (Nhiều chip "C8" thực tế có 128 KB nhưng không được
bảo đảm, không nên dựa vào.)

## 4. Luồng khởi động (boot decision)

```
Reset
 ├─ Đọc cờ "programming request" (thanh ghi BKP_DR1, app ghi trước khi reset)  ── có ──► ở lại FBL
 ├─ Nút "force boot" (Dio) đang nhấn?                                          ── có ──► ở lại FBL
 ├─ Metadata hợp lệ? (magic, meta_crc32)                                        ── không ► ở lại FBL
 ├─ CRC32 vùng app == app_crc32?                                                ── không ► ở lại FBL
 ├─ Vector table app hợp lệ? (SP trong RAM, Reset_Handler trong vùng app, bit Thumb)── không ► FBL
 └─ Nhảy vào app: tắt IRQ, deinit Can/SysTick/Gpt, xoá NVIC pending,
                  SCB->VTOR = 0x08004000, set MSP, gọi Reset_Handler của app
```

Chi tiết dễ sai (sẽ có trong checklist review):

- App phải có linker script riêng (`ORIGIN = 0x08004000`) và `SystemInit` của app phải set lại `VTOR`.
- Cờ programming request đặt trong **backup register** (cần bật clock PWR + BKP và bit `DBP`) hoặc một
  vùng RAM `.noinit` có magic; không dùng biến `.bss` vì startup code xoá nó.
- CRC toàn bộ 47 KB bằng CRC hardware mất vài ms; nếu bật IWDG thì phải kick trong vòng lặp.
- Đọc `RCC_CSR` để biết lý do reset (watchdog, software, pin) và đưa vào DID chẩn đoán.

## 5. Ngăn xếp giao thức

### 5.1 CAN + ISO-TP (module `CanTp` tự viết)

- Physical addressing: request `0x7E0`, response `0x7E8`; functional `0x7DF` (như OBD). Trong FBL, filter
  bxCAN **chỉ nhận** 3 ID này.
- Khung: Single Frame, First Frame, Consecutive Frame, Flow Control; padding `0xCC`.
- Tham số FC của ECU: `BS = 0` (gửi liên tục), `STmin` = 0–1 ms. Cần thử: F103 có kịp nhận CF liên tục
  không, khi FIFO bxCAN chỉ sâu 3? Đây là thí nghiệm hay để ghi vào README.
- Timeout `N_Bs`, `N_Cr` = 1000 ms; độ dài tối đa của một message ISO-TP chuẩn là 4095 byte.

### 5.2 UDS (module `Dcm` subset tự viết)

Chuỗi flash, mô phỏng flash sequence kiểu OEM:

| Pha | Request | Ý nghĩa / chỗ cần cẩn thận |
|---|---|---|
| Pre | `10 03` | Extended session (app) |
| Pre | `31 01 02 03` | Routine "check preconditions": app trả về OK chỉ khi speed = 0 (từ P1) và điện áp ổn (Adc, tùy chọn) |
| Pre | `85 02` (functional) | Tắt ghi DTC trên các ECU khác |
| Pre | `28 03 01` (functional) | Tắt truyền thông thường, giải phóng bus |
| Prog | `10 02` | App ghi cờ BKP rồi reset; **FBL** gửi response `50 02` sau reset (bẫy kinh điển) |
| Prog | `27 01` / `27 02` | Seed/key; sai 3 lần → NRC `0x36`, khoá 10 s → NRC `0x37` |
| Prog | `2E F1 5A` | Ghi fingerprint (ai flash, ngày nào) |
| Prog | `31 01 FF 00` + addr/size | Erase: 47 page × ~20–40 ms ≈ 1–2 s, vượt P2 = 50 ms → **phải trả NRC `0x78` (response pending)** rồi mới trả kết quả trong P2* = 5000 ms |
| Prog | `34 00 44` addr size | Request download; ECU trả `maxNumberOfBlockLength = 0x0402` (1024 data + 2 byte header = 1 page) |
| Prog | `36 <seq>` data… | Transfer data; seq bắt đầu `01`, wrap `FF → 00`; seq lặp lại (tester gửi lại) phải được chấp nhận lần nữa, seq sai → NRC `0x73` |
| Prog | `37` | Request transfer exit |
| Prog | `31 01 02 02` | Check memory: CRC32 so với manifest; OK mới ghi metadata valid |
| Prog | `31 01 FF 01` | Check programming dependencies (hw_id, version không được hạ cấp nếu cấm rollback) |
| Post | `11 01` | Hard reset → app mới chạy |
| Post | `10 01`, `28 00 01`, `85 01` (functional) | Trả mạng về bình thường |

Timing: `P2 = 50 ms`, `P2* = 5000 ms`, `S3 = 5000 ms` (tester gửi `3E 80` mỗi 2 s). Hết S3 thì FBL về
default session nhưng **vẫn ở lại FBL** nếu app chưa valid.

### 5.3 Ghi Flash (module `Fls`, MCAL mới)

- Unlock bằng `FLASH_KEYR` (`0x45670123`, `0xCDEF89AB`), erase page bằng `PER` + `AR` + `STRT`, ghi
  bằng `PG` **theo half-word (16 bit)**, chờ `BSY`, kiểm tra `EOP`/`PGERR`/`WRPRTERR`, đọc lại so sánh.
- F103 chỉ có 1 bank: trong lúc erase/program, CPU đọc Flash sẽ bị stall (~20–40 ms mỗi page), kể cả
  ISR CAN. Không sao trong FBL vì tester đang chờ response, nhưng phải phân tích và ghi lại. Cách nâng
  cao: chạy routine ghi từ RAM (`.ramfunc`).
- API theo kiểu AUTOSAR `Fls`: `Fls_Erase()`, `Fls_Write()` trả về ngay, `Fls_MainFunction()` làm từng
  page một, `Fls_GetJobResult()`. Kiểu bất đồng bộ này chính là thứ cho phép Dcm gửi `0x78` đúng hạn.
- Host test với thanh ghi giả (`Mcal_FakeRegs.c`) như các driver hiện có.

### 5.4 Toàn vẹn và bảo mật

| Mức | Cơ chế | Chỗ thực hiện |
|---|---|---|
| Bắt buộc | CRC32 cả image, dùng **CRC unit hardware** của F103 (thêm driver `Crc`) | STM32 |
| Bắt buộc | `hw_id` + version trong manifest, kiểm trước khi erase | ESP32 và STM32 |
| Bắt buộc | Seed/key, giới hạn số lần thử, delay timer | STM32 |
| Nâng cao | Chữ ký ECDSA P-256 trên image | ESP32 kiểm (mbedTLS có sẵn, có tăng tốc HW) |
| Nâng cao | Kiểm chữ ký ngay trong FBL (micro-ecc, ~5–8 KB code) | STM32, nếu còn chỗ trong 16 KB |
| Nâng cao | Option byte RDP level 1 + WRP vùng FBL | STM32 |

Seed: F103 không có RNG; lấy từ nhiễu LSB của ADC (cảm biến nhiệt độ nội) trộn với SysTick. Ghi rõ đây
là giải pháp học tập, không phải bảo mật thật.

## 6. Phía ESP32 (ESP-IDF, C)

- **Task CAN**: TWAI driver, ISO-TP client (cùng thiết kế với `CanTp` phía STM32, có thể chia sẻ code
  logic thuần C giữa hai bên và host-test một lần).
- **Task UDS sequencer**: state machine theo bảng 5.2; mỗi bước có timeout, retry có giới hạn, và hành
  động khi thất bại (thử lại bước, hoặc flash lại last-good).
- **Storage**: LittleFS chứa `new.bin`, `last_good.bin`, `manifest.json` (`version`, `size`, `crc32`,
  `hw_id`, `signature`).
- **HTTP**: trang upload (bước đầu) → sau này có thể "pull" từ GitHub Releases, giống một OTA campaign.
- **TFT UI**: Idle → "Update available v1.1" → kiểm điều kiện (đọc speed qua `22`/routine `31`) → xin
  xác nhận người dùng (nút hoặc touch XPT2046 nếu màn có) → thanh tiến trình % và tốc độ KB/s →
  Verifying → Success / Failed + Rolled back.

## 7. Ma trận kịch bản lỗi (phần "giải quyết vấn đề", nên làm thành bảng kết quả trong README)

| # | Kịch bản | Cách tạo | Kết quả mong muốn |
|---|---|---|---|
| F1 | Mất điện khi đang erase | rút nguồn STM32 | khởi động lại vào FBL (metadata đã bị xoá), flash lại được |
| F2 | Mất điện khi đang transfer | rút nguồn ở 50% | như F1 |
| F3 | Rút dây CAN giữa chừng | ngắt CAN_H | ESP32 timeout, báo lỗi; ECU hết S3 vẫn ở FBL; nối lại flash tiếp |
| F4 | Image hỏng | ESP32 cố ý đảo 1 bit | `31 01 02 02` trả lỗi, không ghi valid, ESP32 rollback |
| F5 | Sai phần cứng | manifest `hw_id` khác | bị từ chối **trước** khi erase, app cũ vẫn chạy |
| F6 | Sai key 3 lần | ESP32 gửi key sai | NRC `0x36`, rồi `0x37` trong thời gian khoá |
| F7 | Mất một block | ESP32 bỏ qua seq N | NRC `0x73`, tester gửi lại đúng seq |
| F8 | Gửi lặp block | ESP32 gửi seq N hai lần | chấp nhận, không ghi đè sai |
| F9 | App mới bị lỗi (treo) | flash app cố ý treo trước khi báo boot OK | IWDG reset, FBL đếm số lần, sau N lần ở lại FBL; ESP32 rollback |
| F10 | Bus-off trong lúc flash | chập CAN_H/CAN_L ngắn | phục hồi, tiếp tục hoặc báo lỗi rõ ràng |

F9 cần thêm một cơ chế nhỏ: app ghi "boot OK" (ví dụ qua BKP register hoặc một DID) sau khi chạy ổn
N giây; FBL đếm số lần reset liên tiếp chưa có boot OK.

## 8. Hiệu năng ước tính (để so với số đo thật)

- Frame CAN 8 byte ở 500 kbit/s ≈ 110–130 bit (có bit stuffing) → tối đa ~4000 frame/s.
- Mỗi CF chở 7 byte → lý thuyết ~25–28 KB/s với `STmin = 0`, `BS = 0`.
- Trừ thời gian ghi Flash (1 page/block) và overhead UDS, kỳ vọng thực tế **8–15 KB/s** → app 47 KB
  mất ~4–6 s. Đo và ghi số thật vào README (bảng theo `STmin` và block size).

## 9. Liên hệ với phạm vi hiện tại (cần bạn quyết)

Phạm vi chốt ngày 2026-10-11 là MCAL + DBC, không viết BSW service. Project 3 **bắt buộc** cần hai
module thuộc lớp service: ISO-TP (`CanTp`) và UDS (`Dcm` subset). Đề xuất: mở rộng phạm vi cho đúng
hai module này, viết tay ở mức tối thiểu, và gọi trên CV là "ISO-TP and UDS (ISO 14229) subset, hand
written", không nói "AUTOSAR Dcm". Các MCAL mới cần: `Fls`, `Crc`, `Wdg`, (P1 đã thêm `Spi`).

## 10. Lộ trình (mỗi bước một PR; bạn viết code, mình ra skeleton/test và review)

| Bước | Nội dung | Tiêu chí xong |
|---|---|---|
| P3.0 | Xong P1 (bus thật, Can ổn định) | — |
| P3.1 | Tách linker script FBL/app, FBL chỉ kiểm vector table rồi nhảy | app nháy LED khi chạy từ `0x08004000` |
| P3.2 | MCAL `Fls` + `Crc` + host test | erase/write/verify 1 page trên board; CRC khớp CRC-32/MPEG-2 tính trên PC (lưu ý: khác `zlib.crc32`, CRC unit F103 không reflect và không XOR đầu ra) |
| P3.3 | `CanTp` host test, rồi trên board | gửi/nhận message 4095 byte giữa ESP32 và STM32; SavvyCAN decode ISO-TP |
| P3.4 | `Dcm` subset: `10`, `3E`, `27`, `22`, `11`, NRC, `0x78`, S3 | script Python test được mọi NRC |
| P3.5 | Lập trình: `31`, `34`, `36`, `37`, metadata, boot decision đầy đủ | flash app thành công từ PC |
| P3.6 | ESP32 OTA master + TFT UI + last-good rollback | flash qua WiFi không cần PC |
| P3.7 | Chạy ma trận F1–F10, đo throughput, video demo | bảng kết quả trong README |

**Mẹo phát triển nhanh:** trước khi viết UDS client trên ESP32, cho ESP32 chạy firmware ESP32RET
(GVRET) hoặc SLCAN để PC thành tester; dùng Python `python-can` + `can-isotp` + `udsoncan` viết test
cho FBL. Bộ script đó vừa là test HIL vừa là "reference tester" để so với client ESP32 sau này.

## 11. Câu viết vào CV (mẫu)

> **CAN Flash Bootloader with OTA update (STM32F103 + ESP32)** — UDS (ISO 14229) flash sequence over
> ISO-TP (ISO 15765-2) with security access, response-pending handling and block sequence recovery;
> fail-safe boot decision (metadata + hardware CRC32, never-brick on power loss); asynchronous Flash
> driver in AUTOSAR MCAL style; ESP32 OTA master with manifest/version checks and last-good rollback;
> validated against 10 fault-injection scenarios, measured ~X KB/s at 500 kbit/s.

## 12. Câu hỏi phỏng vấn project này chuẩn bị cho bạn

- Tại sao response của `10 02` lại do bootloader gửi, và tester phân biệt thế nào?
- Khi nào ECU phải gửi `7F xx 78`, và tester phải đổi timeout ra sao?
- Làm sao đảm bảo mất điện ở bất kỳ thời điểm nào cũng không brick ECU? (thứ tự: xoá valid flag →
  erase → ghi → verify → ghi valid flag cuối cùng)
- Seed/key chống được gì và không chống được gì? Khác gì với secure boot dùng chữ ký?
- Vì sao block size chọn bằng page Flash?
- Khi nhảy từ bootloader sang app cần dọn dẹp những gì?

## 13. Các hướng mở rộng (brainstorm, chọn sau)

- **Flash nhiều ECU**: thêm ECU ảo thứ hai (ví dụ ESP32 tự đóng vai) để làm "campaign" có thứ tự và
  phụ thuộc version giữa các ECU.
- **Delta update**: chỉ gửi các page thay đổi (so sánh CRC từng page qua một routine) → giảm thời gian.
- **Nén image** (heatshrink, LZ4) để tăng throughput hiệu dụng, giải nén trong FBL.
- **Log toàn bộ phiên flash bằng SavvyCAN** và đưa ảnh decode UDS vào README.
- **Đo và báo cáo MISRA C:2012** cho FBL (cppcheck addon misra).
