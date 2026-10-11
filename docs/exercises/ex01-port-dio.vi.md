# Bài tập MCAL 1 — `Port` và `Dio` (GPIO)

Người học: hi3rdt · Bạn tự viết code, mình review · Tài liệu chính: **RM0008** chương 9 "General-purpose and alternate-function I/Os" và mục 7.3.7 "APB2 peripheral clock enable register"

> Thứ tự bài MCAL: **1 Port/Dio** → 2 Mcu + Gpt (sắp có) → [3 Can](ex03-can-bxcan.vi.md) → 4 Adc (tùy chọn). Xem [roadmap](../roadmap.vi.md#cập-nhật-phạm-vi-2026-10-11-mcal--can-stack) để biết vì sao project làm MCAL.

| Phần | Bạn viết | Kiểm chứng bằng | Xong khi |
|---|---|---|---|
| A | `bsw/mcal/port/Port.c`, `bsw/mcal/dio/Dio.c` | `test/host/test_port_dio.c` (đã viết sẵn, 16 test, 112 check, chạy dưới ASan/UBSan) | `test_port_dio` báo `0 failures` và bạn đổi option CMake sang `ON` |
| B | Bảng cấu hình chân của board + sửa `app/main.c` | Blue Pill | LED PC13 nháy 1 Hz qua `Port_Init` + `Dio_FlipChannel`, không còn dòng nào trong `main.c` chạm thanh ghi GPIO |

## 1. Port và Dio khác nhau thế nào

Trong AUTOSAR, GPIO được chia cho hai module với trách nhiệm tách bạch: **Port** cấu hình chân (input hay output, pull-up hay pull-down, push-pull hay open-drain, tốc độ, alternate function) và chạy một lần lúc khởi động, còn **Dio** chỉ đọc và ghi mức logic của những chân đã được Port cấu hình, được gọi liên tục lúc chạy, kể cả từ ISR. Dio không bao giờ được đổi mode của chân. Tách như vậy để một SWC hay CDD bật tắt LED không thể vô tình biến chân CAN TX thành input, và để cấu hình chân của cả ECU nằm ở một bảng `const` duy nhất mà người tích hợp phần cứng review được. Trên chip thật, MCAL Port và Dio do hãng chip cung cấp (ví dụ NXP RTD cho S32K); ở đây bạn tự viết theo cùng API.

## 2. Thanh ghi GPIO của STM32F103

Mỗi port (A, B, C trên Blue Pill) có các thanh ghi 32 bit sau. Struct `Mcal_GpioRegType` trong `bsw/mcal/reg/Mcal_Reg.h` có đúng thứ tự này; bạn tự đối chiếu offset với RM0008 mục 9.5.

| Thanh ghi | Offset | Ý nghĩa |
|---|---|---|
| `CRL` | `0x00` | Cấu hình chân 0..7, **4 bit mỗi chân** |
| `CRH` | `0x04` | Cấu hình chân 8..15, 4 bit mỗi chân |
| `IDR` | `0x08` | Mức thật trên chân (chỉ đọc), bit 15..0 |
| `ODR` | `0x0C` | Chốt output (output latch). Với input pull, bit này chọn pull-up (1) hay pull-down (0) |
| `BSRR` | `0x10` | Ghi 1 vào bit *n* (15..0) thì set chân *n*; ghi 1 vào bit *n*+16 thì reset chân *n*. Bit ghi 0 không làm gì. **Nguyên tử** |
| `BRR` | `0x14` | Chỉ reset (bit 15..0). Project này không dùng, BSRR đủ |
| `LCKR` | `0x18` | Khóa cấu hình. Không dùng |

4 bit của một chân là `CNF[1:0] MODE[1:0]` (RM0008 bảng 20 và 21). Nibble của chân *p* nằm ở `CRL` nếu *p* < 8, ngược lại ở `CRH`, và dịch trái `(p % 8) * 4` bit. Giá trị reset là `0x4` (input floating) cho mọi chân, nên `CRL = CRH = 0x44444444` sau reset.

| Mode trong `Port_PinModeType` | CNF | MODE | Nibble |
|---|---|---|---|
| `ANALOG` | 00 | 00 | `0x0` |
| `INPUT_FLOATING` | 01 | 00 | `0x4` |
| `INPUT_PULLUP` / `INPUT_PULLDOWN` | 10 | 00 | `0x8` (pull chọn bằng ODR) |
| `OUTPUT_PP` | 00 | speed | `0x0 \| speed` |
| `OUTPUT_OD` | 01 | speed | `0x4 \| speed` |
| `ALT_PP` | 10 | speed | `0x8 \| speed` |
| `ALT_OD` | 11 | speed | `0xC \| speed` |

`speed` là `MODE`: `01` = 10 MHz, `10` = 2 MHz, `11` = 50 MHz. Enum `Port_PinSpeedType` đã gán đúng các giá trị này nên bạn OR thẳng vào được. Clock của GPIO mặc định **tắt**: phải set `IOPAEN` (bit 2), `IOPBEN` (bit 3), `IOPCEN` (bit 4) trong `RCC_APB2ENR` trước khi ghi bất kỳ thanh ghi GPIO nào, nếu không mọi lệnh ghi bị bỏ qua mà không báo lỗi gì. Đây là bug số 1 khi bring-up một board mới.

## 3. Lớp thanh ghi: vì sao không ghi thẳng địa chỉ

`Port.c` và `Dio.c` chỉ chạm phần cứng qua `Mcal_GpioBase[port]` và `Mcal_RccBase`, khai báo `extern` trong `Mcal_Reg.h`. Có hai nơi định nghĩa chúng: `platform/stm32f103/Mcal_RegMap.c` trỏ vào địa chỉ thật (`GPIOA = 0x40010800` …) và chỉ được build vào firmware, còn `test/host/stubs/Mcal_FakeRegs.c` trỏ vào các biến trong RAM và chỉ được build vào host test. Nhờ vậy cùng một file `Port.c` chạy trên chip và trên PC, và phép ép số nguyên sang con trỏ (MISRA C:2012 Rule 11.4) chỉ nằm ở một file trong `platform/`. Khi viết, **không** `#include "stm32f103_regs.h"` trong `Port.c`/`Dio.c`.

Thanh ghi fake là RAM thường: ghi `BSRR` **không** làm `ODR` đổi, và `IDR` chỉ đổi khi test ghi vào. Vì thế test kiểm tra *bạn đã ghi gì* (ví dụ giá trị `BSRR`) và *chuẩn bị sẵn thứ bạn sẽ đọc* (ví dụ `IDR`).

## 4. Phần A — `Port.c` và `Dio.c`

Hợp đồng chi tiết của từng hàm nằm trong comment của `Port.h` và `Dio.h`; test kiểm tra đúng hợp đồng đó. Thứ tự nên làm:

1. **`Port_Hw_ModeNibble`**: hàm thuần, chỉ là bảng ở mục 2. Viết bằng `switch` có `default` (MISRA Rule 16.4).
2. **`Port_Init`**, theo thứ tự trong `Port.h`:
   - Kiểm tra con trỏ, rồi kiểm tra **toàn bộ** pin id trước khi ghi bất kỳ thanh ghi nào. Test `test_init_invalid_pin_writes_nothing` đặt pin sai ở cuối mảng để bắt driver vừa kiểm tra vừa ghi. Lý do: cấu hình dở dang nửa chừng nguy hiểm hơn không cấu hình gì.
   - Với mỗi pin: bật clock bằng `|=` (không được xóa bit của ngoại vi khác như USART1), đặt `ODR` **trước**, rồi mới ghi nibble vào `CRL/CRH`. Nếu đổi mode trước, chân thành output ngay với giá trị `ODR` cũ, và LED hay chân enable của transceiver có thể chớp một xung sai.
   - Ghi nibble bằng read-modify-write: xóa đúng 4 bit của chân, OR giá trị mới, giữ nguyên 7 nibble còn lại.
   - Lưu `ConfigPtr` vào biến `static` để `Port_SetPinDirection` dùng. Init thất bại thì vẫn là "chưa init".
3. **`Port_SetPinDirection`**: tìm pin trong cấu hình đã lưu, chỉ cho đổi khi `directionChangeable == TRUE`.
4. **`Dio_ReadChannel` / `Dio_ReadPort`**: đọc `IDR`, không đọc `ODR`.
5. **`Dio_WriteChannel` / `Dio_WritePort`**: **một** lệnh ghi `BSRR`, không bao giờ đọc-sửa-ghi `ODR`.
6. **`Dio_FlipChannel`**: đọc `ODR` (không phải `IDR`) để biết mức hiện tại, rồi một lệnh ghi `BSRR`.

Thao tác bit cần dùng nhiều lần, ghi ra đây để bạn khỏi đoán (không phải lời giải):

```c
/* Channel/pin id -> port index and bit position (id = port * 16 + pin) */
uint8  port  = (uint8)(id / 16U);
uint8  pin   = (uint8)(id % 16U);
uint32 shift = ((uint32)pin % 8U) * 4U;     /* nibble position inside CRL or CRH */
```

### Vì sao Dio phải dùng BSRR

Xét đoạn code "tự nhiên" này, chạy trong task, trong khi một ISR cũng điều khiển chân khác của cùng port:

```c
/* WRONG: read-modify-write on ODR is not atomic */
GPIOB->ODR |= (1UL << 5);   /* compiles to: LDR r1,[ODR]; ORR r1,#0x20; STR r1,[ODR] */
```

Nếu ngắt xảy ra giữa `LDR` và `STR`, và ISR set chân PB6, thì `STR` của task ghi lại giá trị `ODR` cũ (PB6 = 0) và xóa mất thay đổi của ISR. Lỗi này chỉ xuất hiện khi ngắt rơi đúng vào cửa sổ 2–3 lệnh, nên test trên bàn hiếm khi thấy mà xe chạy lâu thì gặp. `BSRR` giải quyết bằng phần cứng: một lệnh `STR` duy nhất, bit 0 không ảnh hưởng chân khác, nên không cần khóa ngắt. `Port_Init` thì được phép read-modify-write `ODR` vì nó chạy một lần lúc khởi động, trước khi bật ngắt.

### Vì sao Flip đọc ODR chứ không đọc IDR

Với chân open-drain đang được thiết bị ngoài kéo xuống, `ODR = 1` (đang nhả) nhưng `IDR = 0`. Flip theo `IDR` sẽ ghi 1, tức là không đổi gì, và lần flip sau lại ghi 1. `ODR` mới là thứ phần mềm đã ra lệnh. Test `test_flip_channel_uses_odr` dựng đúng tình huống này.

### Chạy test

```bash
cmake -S . -B build-host -DMCAL_PORT_DIO_EXERCISE_TESTS=ON
cmake --build build-host -j
ctest --test-dir build-host -R test_port_dio --output-on-failure
```

Stub hiện tại cố tình sai, nên lần chạy đầu sẽ có khoảng 80 check fail. Mỗi dòng `FAIL` in ra file:dòng, giá trị mong đợi và giá trị bạn ghi, ví dụ `GPIOB->CRH (expected 0x444444B8, got 0x44444444)`. Đọc giá trị hex theo từng nibble là cách nhanh nhất để thấy sai ở chân nào.

Tự kiểm tra trước khi chạy test: PB9 ở `ALT_PP` 50 MHz là nibble `0xB` ở vị trí `(9 % 8) * 4 = 4` của `CRH`, nên từ giá trị reset `CRH` thành `0x444444B4`. Thêm PB8 `INPUT_PULLUP` thì thành `0x444444B8` và `ODR` bit 8 = 1.

**Xong phần A:** đổi `option(MCAL_PORT_DIO_EXERCISE_TESTS ... OFF)` thành `ON` trong `CMakeLists.txt` để CI chạy test này từ nay về sau.

## 5. Phần B — Chạy trên Blue Pill

Hiện `app/main.c` có `Led_Init()` và `Led_Toggle()` ghi thẳng `GPIO_CRH`/`GPIO_BSRR`. Thay chúng bằng một bảng cấu hình chân của board và hai lời gọi MCAL:

| Chân | Mode | Speed | Mức đầu | Ghi chú |
|---|---|---|---|---|
| PC13 | `OUTPUT_PP` | 2 MHz | `STD_HIGH` | LED tích cực mức thấp, nên HIGH = tắt |
| PB8 | `INPUT_PULLUP` | (bỏ qua) | | CAN RX sau remap, chuẩn bị cho bài 3 |
| PB9 | `ALT_PP` | 50 MHz | `STD_HIGH` | CAN TX sau remap; mức lặn của CAN là 1 |

Đặt bảng ở một file cấu hình riêng (ví dụ `app/Port_Cfg.c` với `const Port_ConfigType Port_Config`), gọi `Port_Init(&Port_Config)` sau `System_ClockInit()`, và trong vòng lặp gọi `(void)Dio_FlipChannel(DIO_CHANNEL(PORT_C, 13U))`. `Mcal_RegMap.c` đã được link vào firmware; bạn chỉ cần thêm file cấu hình mới vào danh sách nguồn của `firmware.elf` trong `CMakeLists.txt`.

Bẫy phần cứng cần biết trước khi tự thêm chân:

- **PA13, PA14** là SWDIO/SWCLK. Cấu hình lại chúng là mất kết nối debugger; muốn nạp lại phải giữ nút RESET khi bấm "connect under reset" trong ST-Link.
- **PA15, PB3, PB4** sau reset là chân JTAG, không phải GPIO. Muốn dùng phải tắt JTAG bằng `AFIO_MAPR.SWJ_CFG`, việc sẽ gặp lại ở bài 3.
- **PC13, PC14, PC15** nằm trong miền backup: chỉ chịu dòng khoảng 3 mA, tối đa 2 MHz, và không được dùng làm nguồn dòng (datasheet STM32F103x8, ghi chú của bảng pin definitions). Vì thế LED trên Blue Pill nối kiểu tích cực mức thấp.
- `ALT_PP` chỉ chọn "ngoại vi điều khiển chân"; ngoại vi nào thì do remap trong AFIO quyết định, và clock `AFIOEN` phải bật. Port không làm việc này; driver `Can` làm ở bài 3.

## 6. Checklist review (mình sẽ dùng khi bạn push)

- **Đúng lớp:** `Dio` không ghi `CRL/CRH`; `Port` không ghi `BSRR`; không file nào trong `bsw/mcal/` chứa địa chỉ cứng hay include `stm32f103_regs.h`.
- **Nguyên tử:** `Dio` chỉ ghi `BSRR` bằng một lệnh; không có `|=` hay `&=` trên `ODR` ngoài `Port_Init`.
- **Read-modify-write đúng:** `APB2ENR` và `CRL/CRH` chỉ đổi đúng bit của chân đang cấu hình.
- **Kiểm tra trước, ghi sau:** mọi lỗi tham số được phát hiện trước lệnh ghi thanh ghi đầu tiên, và đi qua `Det_ReportError` với module ID, service ID, error ID đúng như header.
- **MISRA C:2012:** hằng số có hậu tố `U`/`UL`; phép dịch thực hiện trên `uint32` (dịch `1U << 31` trên kiểu 16 bit là undefined behavior, UBSan sẽ bắt); ép kiểu thu hẹp có `(uint8)` tường minh vì project build với `-Wconversion -Werror`; `switch` có `default`; một điểm `return` mỗi hàm (Rule 15.5, advisory, project đang theo).
- **Bộ nhớ:** không `malloc`; cấu hình là `const`; trạng thái duy nhất của Port là một con trỏ `static`.

## 7. Cách nộp

1. Tạo nhánh, ví dụ `ex01-port-dio`, commit phần A (kèm đổi option CMake sang `ON`), push và mở PR vào `master` (sau khi các PR đang mở đã được merge).
2. Nhắn trong thread "review ex01" kèm link PR. Mình review theo checklist trên, comment trực tiếp vào dòng code, và không sửa code thay bạn trừ khi bạn yêu cầu.
3. Phần B có thể nộp chung PR hoặc PR riêng sau khi LED đã nháy trên Blue Pill.
