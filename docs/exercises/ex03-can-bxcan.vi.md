# Bài tập MCAL 3 — Driver `Can` (bxCAN)

> Thứ tự bài MCAL: [M1 Port/Dio](ex01-port-dio.vi.md) → M2 Mcu + Gpt (sắp có) → **M3 Can** → M4 Uart + SLCAN (sắp có) → M5 Adc (tùy chọn). Phần A của bài này là hàm thuần nên làm song song với bài 1 được; phần B và C dùng `Port` (bài 1) để cấu hình chân PB8/PB9 và `Mcu` (bài 2) để biết tần số PCLK1.

Người học: hi3rdt · Bạn tự viết code, mình review · Tài liệu chính: **RM0008** (STM32F10x Reference Manual) chương 24 "Controller area network (bxCAN)"

Bài tập chia làm 3 phần, mỗi phần có tiêu chí "xong" rõ ràng. Phần A chạy hoàn toàn trên PC nên làm được ngay hôm nay, chưa cần Blue Pill.

| Phần | Bạn viết | Kiểm chứng bằng | Xong khi |
|---|---|---|---|
| A | `bsw/mcal/can/Can_Hw.c`: các hàm thuần tính toán giá trị thanh ghi | `test/host/test_can_hw.c` (đã viết sẵn, 14 test, ~2 175 check, chạy dưới ASan/UBSan) | `test_can_hw` báo `0 failures` và bạn đổi option CMake sang `ON` |
| B | `bsw/mcal/can/Can.c`: `Can_Init`, `Can_SetControllerMode`, `Can_Write` (truy cập thanh ghi) | Blue Pill ở chế độ **loopback**, debugger hoặc LED | Gửi một frame, nhận lại đúng ID/DLC/data |
| C | 3 ISR: TX, RX FIFO0, SCE (lỗi/bus-off) + gọi callback của lớp trên (`CanIf_Cbk.h`) | Loopback; bạn tự viết hai callback trong `app/` | Frame gửi bằng `Can_Write` quay về và tới `CanIf_RxIndication` đúng ID, DLC, data |

Sau khi `Can` chạy được, bài tiếp theo là M4: driver `Uart` đẩy frame lên PC theo giao thức LAWICEL/SLCAN để SavvyCAN xem trực tiếp, rồi decode bằng DBC bạn viết ở [bài D1](dbc01-gvret-log.vi.md).

## Vì sao tách `Can_Hw.c` khỏi `Can.c`

Toàn bộ "logic bit" của driver (tính bit timing, ghép ID vào `TIxR`, pack data vào `TDLxR/TDHxR`, chọn mailbox trống, dựng filter) là hàm thuần: nhận số vào, trả số ra, không đụng thanh ghi. Tách chúng ra `Can_Hw.c` thì test được trên PC với ASan, và `Can.c` chỉ còn là trình tự ghi thanh ghi đơn giản, dễ review theo RM0008. Đây cũng là cách các MCAL thương mại tổ chức code (lớp "LLD" thuần + lớp "IPW" truy cập phần cứng), và là câu trả lời tốt khi phỏng vấn hỏi "bạn test driver thế nào khi không có phần cứng".

## Phần A — Hàm thuần (`Can_Hw.c`)

Hợp đồng của từng hàm nằm trong `bsw/mcal/can/Can_Hw.h`; đọc kỹ vì test kiểm tra đúng hợp đồng đó. `Can_Hw.c` hiện là stub trả về giá trị sai, bạn thay thân hàm bằng code của mình.

```bash
cmake -S . -B build-host -DCAN_HW_EXERCISE_TESTS=ON
cmake --build build-host -j
ctest --test-dir build-host -R test_can_hw --output-on-failure
```

Gợi ý theo thứ tự nên làm (dễ → khó):

1. `Can_Hw_FindFreeMailbox`: bit `TME0..2` là bit 26..28 của `CAN_TSR`.
2. `Can_Hw_EncodeId` / `Can_Hw_DecodeId`: chuẩn `STID` ở bit 31:21; extended `EXID` ở bit 31:3 và `IDE` = bit 2. Cờ extended của project là bit 31 của `Can_IdType` (xem `Can_GeneralTypes.h`), giống quy ước DBC.
3. `Can_Hw_PackData` / `Can_Hw_UnpackData`: byte 0 nằm ở bit 7:0 của `TDLxR`. Test cấp **đúng `length` byte** trên heap, đọc lố một byte là ASan dừng ngay.
4. `Can_Hw_BuildBtr`: mọi field lưu "giá trị − 1". `LBKM` = bit 30, `SILM` = bit 31.
5. `Can_Hw_BuildMaskFilter32`: `FiR1` và `FiR2` cùng layout với `RIxR`, nên dùng lại `Can_Hw_EncodeId`.
6. `Can_Hw_CalcBitTiming`: khó nhất. Bit time = `(1 + BS1 + BS2)` TQ, `TQ = prescaler / PCLK1`. Sample point = `(1 + BS1) / (1 + BS1 + BS2)`. Một test cố tình đưa bitrate làm phép nhân `bitrate * tq` tràn `uint32`; nếu test đó fail, bạn biết phải làm gì (CERT INT30-C và MISRA C:2012 Dir 4.1 nói về đúng loại lỗi này).

Tự kiểm tra trước khi chạy test: 36 MHz, 500 kbit/s, 87,5 % phải ra prescaler 4, BS1 15, BS2 2, tức `CAN_BTR = 0x001E0003` (và `0x401E0003` khi loopback).

**Xong phần A:** đổi `option(CAN_HW_EXERCISE_TESTS ... OFF)` thành `ON` trong `CMakeLists.txt` để CI chạy test này từ nay về sau, commit và mở PR.

## Phần B — Trình tự thanh ghi (`Can.c`)

API đã khai báo sẵn trong `bsw/mcal/can/Can.h` (`Can_ConfigType`, `Can_Init`, `Can_SetControllerMode`, `Can_Write`, `Can_MainFunction_BusOff`). Theo cùng cách đã làm ở bài 1, bạn tự thêm `Mcal_CanRegType` (và `Mcal_AfioRegType`) vào `bsw/mcal/reg/Mcal_Reg.h`, một con trỏ base `Mcal_CanBase` với địa chỉ thật trong `platform/stm32f103/Mcal_RegMap.c` và bản fake trong `test/host/stubs/Mcal_FakeRegs.c`, để `Can.c` cũng test được trên PC. Đối chiếu từng offset với RM0008 mục 24.9.5 "bxCAN register map" thay vì tin bảng dưới đây.

| Thanh ghi | Offset từ `CAN1` = `0x4000 6400` | Bit cần dùng |
|---|---|---|
| `CAN_MCR` | `0x000` | `INRQ` 0, `SLEEP` 1, `TXFP` 2, `ABOM` 6, `RESET` 15 |
| `CAN_MSR` | `0x004` | `INAK` 0, `SLAK` 1, `ERRI` 2 (rc_w1) |
| `CAN_TSR` | `0x008` | `RQCPx`/`TXOKx` (mailbox 0 ở bit 0/1, mailbox 1 ở 8/9, mailbox 2 ở 16/17), `TMEx` 26..28 |
| `CAN_RF0R` | `0x00C` | `FMP0` 1:0, `FULL0` 3, `FOVR0` 4, `RFOM0` 5 |
| `CAN_IER` | `0x014` | `TMEIE` 0, `FMPIE0` 1, `BOFIE` 10, `ERRIE` 15 |
| `CAN_ESR` | `0x018` | `BOFF` 2, `LEC` 6:4, `TEC` 23:16, `REC` 31:24 |
| `CAN_BTR` | `0x01C` | từ `Can_Hw_BuildBtr` (chỉ ghi được khi đang ở init mode) |
| TX mailbox *n* | `0x180 + 0x10·n` | `TIxR` +0, `TDTxR` +4 (DLC 3:0), `TDLxR` +8, `TDHxR` +C |
| RX FIFO 0 | `0x1B0` | `RI0R` +0, `RDT0R` +4 (DLC 3:0), `RDL0R` +8, `RDH0R` +C |
| Filter | `0x200`… | `FMR` 0x200 (`FINIT` 0), `FM1R` 0x204, `FS1R` 0x20C, `FFA1R` 0x214, `FA1R` 0x21C, bank *i*: `FiR1` = `0x240 + 8·i`, `FiR2` = `+4` |

`Can_Init` nên làm theo thứ tự:

1. Bật clock: `RCC_APB1ENR.CANEN` và `RCC_APB2ENR.AFIOEN`. Clock `IOPBEN` đã do `Port_Init` bật.
2. Remap CAN sang PB8/PB9: `AFIO_MAPR` (`0x4001 0004`) bit 14:13 = `0b10`. Bẫy: bit `SWJ_CFG` 26:24 là write-only và giá trị đọc về không xác định (RM0008 §9.4.2), nên read-modify-write có thể ghi đè cấu hình SWD; hỏi bản thân xem có ảnh hưởng không.
3. GPIO **không** cấu hình trong `Can.c`: trong AUTOSAR, chân là việc của `Port`. Bảng `Port_ConfigType` của board phải có PB8 = `PORT_MODE_INPUT_PULLUP` và PB9 = `PORT_MODE_ALT_PP` 50 MHz (đúng như test của bài 1), và `Port_Init` chạy trước `Can_Init`.
4. Thoát sleep, vào init: xóa `SLEEP`, set `INRQ`, chờ `INAK = 1` và `SLAK = 0`. **Mọi vòng chờ phải có timeout** (dùng `System_GetTickMs()`), hết hạn thì báo `Det_ReportError(DET_MODULE_ID_CAN, ...)` và trả lỗi.
5. Cấu hình `MCR`: `ABOM = 1` (tự phục hồi bus-off), `TXFP` tùy bạn chọn và **ghi lý do vào comment** (gợi ý: ảnh hưởng thế nào đến thứ tự frame khi 3 mailbox đều đầy và `Can_Hw_FindFreeMailbox` luôn chọn mailbox thấp nhất?).
6. `CAN_BTR = Can_Hw_BuildBtr(...)` từ `Can_Hw_CalcBitTiming(PCLK1_HZ, Config->bitrate, Config->samplePointPermille, ...)`. Sau bài 2, `PCLK1_HZ` lấy từ cấu hình `Mcu` thay vì hằng số.
7. Filter: `FMR.FINIT = 1` → với mỗi filter: mask mode, 32-bit, gán FIFO0, ghi `FiR1/FiR2`, active → `FINIT = 0`. `filters == NULL_PTR` thì cấu hình bank 0 nhận mọi frame (id 0, mask 0, cả chuẩn lẫn extended cần 2 bank vì bit `IDE` luôn được so).
8. Ngắt: `IER = TMEIE | FMPIE0 | ERRIE | BOFIE`, enable IRQ 19, 20, 22 trong `NVIC_ISER0` (`0xE000 E100`).
9. Để controller ở `CAN_CS_STOPPED`. `Can_SetControllerMode(STARTED)` xóa `INRQ` và chờ `INAK = 0`.

`Can_Write`:

- Kiểm tra `Hth == CAN_HTH_TX`, `PduInfo != NULL_PTR`, `length <= 8`; sai thì `Det_ReportError` và trả `E_NOT_OK`.
- Vào `SchM_Enter_Exclusive()`, vì ISR TX dùng chung mảng `swPduHandle` theo mailbox.
- `mb = Can_Hw_FindFreeMailbox(CAN_TSR)`; không còn → `CAN_BUSY`.
- Lưu `PduInfo->swPduHandle` vào `txHandle[mb]`, ghi `TDTxR` (DLC), `TDLxR`, `TDHxR`, **cuối cùng** ghi `TIxR = Can_Hw_EncodeId(id) | TXRQ` bằng một lệnh ghi duy nhất.

Kiểm chứng trên Blue Pill: cấu hình `testMode = CAN_HW_MODE_LOOPBACK` (không cần transceiver, vì ở loopback bxCAN không đọc chân RX), gửi `0x100` với 8 byte bất kỳ, rồi trong debugger xem `RF0R.FMP0 = 1` và `RI0R/RDL0R/RDH0R` đúng như đã gửi. Câu hỏi để bạn tự trả lời: vì sao ở **normal mode** không có transceiver và node khác thì frame không bao giờ gửi xong (gợi ý: ACK slot, rồi xem `TEC` tăng dần).

## Phần C — Ngắt và callback lên lớp trên

Trong AUTOSAR, driver `Can` không biết ai dùng frame: nó chỉ gọi các callback của CanIf (`CanIf_RxIndication`, `CanIf_TxConfirmation`). Project này không có CanIf, nên các callback đó được khai báo trong `bsw/include/CanIf_Cbk.h` và **bạn tự viết phần thân trong `app/`** (ví dụ `app/CanIf_Shim.c`): lưu frame vào một ring buffer `static` để `main` đọc, và sau bài M4 thì đẩy frame đó ra Uart dạng SLCAN. `Can.c` chỉ `#include "CanIf_Cbk.h"` và không phụ thuộc vào gì khác. Đó cũng chính là ranh giới giữa MCAL và lớp ECU Abstraction.

Tên handler đã có trong `platform/stm32f103/startup_stm32f103.c`: `USB_HP_CAN1_TX_IRQHandler` (19), `USB_LP_CAN1_RX0_IRQHandler` (20), `CAN1_SCE_IRQHandler` (22). Trên F103 chúng dùng chung vector với USB, lý do project không dùng USB (roadmap mục 0).

- **TX ISR:** với mỗi mailbox có `RQCPx = 1`: đọc `TXOKx`, xóa bằng cách **ghi 1 chỉ vào `RQCPx`** (thanh ghi rc_w1; read-modify-write `TSR` sẽ xóa luôn cờ của mailbox khác), nếu `TXOK` thì `CanIf_TxConfirmation(txHandle[mb])`.
- **RX0 ISR:** lặp tối đa 3 lần (FIFO sâu 3) khi `FMP0 != 0`: đọc `RI0R`, `RDT0R`, `RDL0R`, `RDH0R` **trước** khi nhả FIFO bằng `RF0R = RFOM0`, rồi gọi `CanIf_RxIndication` với `Hoh = CAN_HRH_FIFO0`. DLC trong `RDT0R` là 4 bit, có thể > 8, nên kẹp lại. Kiểm tra `FOVR0` (mất frame) và báo Det.
- **SCE ISR:** đọc `ESR`; `BOFF = 1` → ghi nhận bus-off (với `ABOM = 1` phần cứng tự phục hồi sau 128 × 11 bit lặn); xóa `MSR.ERRI` bằng cách ghi 1.

Tiêu chí xong: trong `main.c`, gọi `Can_Write` với một frame `0x100`, frame quay lại qua loopback và `CanIf_RxIndication` trong `app/` nhận đúng ID, DLC và data (đặt breakpoint, hoặc so sánh rồi nháy LED bằng `Dio`). Callback chạy trong ISR, nên chỉ copy frame rồi thoát; việc so sánh làm ở `main`.

## Checklist review (mình sẽ dùng khi bạn push)

- **MISRA C:2012:** hằng số có hậu tố `U`/`UL`; không trộn kiểu có dấu và không dấu; một điểm `return` mỗi hàm (Rule 15.5, advisory, project đang theo); mọi `if/else if` có `else`; ép số nguyên sang con trỏ thanh ghi chỉ nằm trong `platform/stm32f103/` (Rule 11.4, đã có ghi chú deviation).
- **ISR:** ngắn, không vòng chờ, không gọi hàm có thể block; biến dùng chung giữa ISR và task là `volatile` và được bảo vệ bằng exclusive area ở phía task.
- **Thanh ghi:** không read-modify-write trên thanh ghi rc_w1 (`TSR`, `RF0R`, `MSR`); `BTR` chỉ ghi trong init mode; `TXRQ` là lệnh ghi cuối cùng vào mailbox.
- **Lỗi:** mọi vòng chờ có timeout; mọi lỗi phát triển đi qua `Det_ReportError(DET_MODULE_ID_CAN, ...)`.
- **Bộ nhớ:** không `malloc`, mảng trạng thái `static`, bảng cấu hình `const`.

## Cách nộp

1. Tạo nhánh, ví dụ `ex03-can-hw`, commit phần A (kèm đổi option CMake sang `ON`), push và mở PR vào `master` (sau khi các PR đang mở đã được merge).
2. Nhắn trong thread "review ex03" kèm link PR. Mình review theo checklist trên, comment trực tiếp vào dòng code, và không sửa code thay bạn trừ khi bạn yêu cầu.
3. Phần B và C có thể nộp chung một PR sau khi đã chạy được trên Blue Pill.
