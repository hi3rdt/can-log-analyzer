# Nhập môn thanh ghi STM32F103: đọc RM0008, hex, bit, `UL`

Người học: hi3rdt · Ngày: 2026-10-18 · Dùng trước khi làm [ex01 Port/Dio](../exercises/ex01-port-dio.vi.md)

> Code, tên thanh ghi, tên bit giữ nguyên tiếng Anh vì đó là tên bạn sẽ gõ và tra trong RM0008.

---

## 0. Một câu để nhớ

Lập trình STM32 mức thanh ghi **không có syntax riêng**, chỉ là C. Mỗi "thanh ghi" là một ô nhớ 32 bit
nằm ở một địa chỉ cố định; ghi một số vào đó thì phần cứng làm việc (bật clock, đổi chân thành output,
kéo chân lên HIGH), đọc từ đó thì biết trạng thái phần cứng (chân đang HIGH hay LOW). Mọi thứ bạn cần
biết là: **thanh ghi nằm ở địa chỉ nào**, **bit nào trong đó làm gì**, và **cách dùng C để bật/tắt đúng
bit đó**. Hai câu hỏi đầu tra trong RM0008, câu thứ ba là mục 4 và 5 bên dưới.

## 1. Địa chỉ: base + offset

Toàn bộ chip nhìn từ CPU là một dải địa chỉ 32 bit (memory map, RM0008 mục 3.3):

| Vùng | Địa chỉ bắt đầu | Chứa gì |
|---|---|---|
| Flash | `0x0800 0000` | code của bạn (64 KB trên C8) |
| SRAM | `0x2000 0000` | biến (20 KB) |
| Peripheral | `0x4000 0000` | **thanh ghi** của GPIO, RCC, CAN, USART… |
| Cortex-M3 core | `0xE000 0000` | SysTick, NVIC (tài liệu PM0056, không phải RM0008) |

Mỗi ngoại vi có một **base address** (RM0008 mục 3.3, bảng "Register boundary addresses"), và mỗi thanh
ghi của nó có một **offset** (ghi ngay đầu mục mô tả thanh ghi đó). Địa chỉ thật = base + offset:

```
GPIOC base   = 0x4001 1000      (table "Register boundary addresses")
CRH offset   =        0x04      (section 9.2.2 "Address offset: 0x04")
GPIOC->CRH   = 0x4001 1004
```

Trong repo, phép cộng này được viết sẵn thành macro trong `platform/stm32f103/stm32f103_regs.h`:

```c
#define REG32(addr)      (*(volatile uint32 *)(addr))   /* "the 32-bit cell at address addr" */
#define GPIOC_BASE       0x40011000UL
#define GPIO_CRH(base)   REG32((base) + 0x04UL)

GPIO_CRH(GPIOC_BASE) = 0x44244444UL;   /* expands to: *(volatile uint32 *)0x40011004UL = ... */
```

`REG32(addr)` đọc là: ép số `addr` thành con trỏ tới `uint32`, rồi lấy giá trị tại con trỏ đó (`*`).
`volatile` bảo compiler "ô nhớ này có thể tự đổi, và mỗi lần ghi đều có tác dụng thật", nên compiler
không được bỏ qua hay gộp các lệnh đọc/ghi. Thiếu `volatile` là bug kinh điển: build `-O0` chạy, `-Os`
thì không.

## 2. Tên thanh ghi đọc như thế nào

Tên luôn có dạng `NGOẠIVI_THANHGHI`, bit có dạng tên viết tắt. Đọc to ra là hiểu:

| Tên | Viết đầy đủ | Việc |
|---|---|---|
| `RCC` | Reset and Clock Control | cấp clock cho từng ngoại vi |
| `RCC_APB2ENR` | APB2 peripheral clock **EN**able **R**egister | mỗi bit bật clock cho 1 ngoại vi trên bus APB2 |
| `IOPCEN` (bit 4) | **I/O P**ort **C** **EN**able | bật clock GPIOC |
| `GPIOx_CRL` / `CRH` | **C**onfiguration **R**egister **L**ow / **H**igh | cấu hình chân 0–7 / 8–15, 4 bit mỗi chân |
| `MODE[1:0]`, `CNF[1:0]` | Mode, Configuration | 4 bit đó: input/output, tốc độ, kiểu output |
| `GPIOx_IDR` | **I**nput **D**ata **R**egister | đọc mức thật trên chân |
| `GPIOx_ODR` | **O**utput **D**ata **R**egister | mức đang xuất ra |
| `GPIOx_BSRR` | **B**it **S**et/**R**eset **R**egister | set/reset chân bằng một lệnh ghi |
| `AFIO_MAPR` | Alternate Function I/O, **MAP**ping **R**egister | remap CAN sang PB8/PB9 |

`x` là A, B, C… tức cùng một bộ thanh ghi lặp lại cho mỗi port, chỉ khác base address.

## 3. Tra một thanh ghi trong RM0008 (ví dụ `GPIOx_CRH`)

1. Mở PDF RM0008, `Ctrl+F` gõ `GPIOx_CRH` (hoặc vào mục lục: chương 9 GPIO and AFIO → 9.2 GPIO
   registers → 9.2.2).
2. Đọc 3 dòng đầu: **Address offset: 0x04**, **Reset value: 0x4444 4444** (giá trị sau khi cấp nguồn).
3. Nhìn **hình ô bit**: hàng trên là số bit 31…0, hàng giữa là tên field (`CNF15[1:0] MODE15[1:0] …
   CNF8[1:0] MODE8[1:0]`), hàng dưới là quyền truy cập: `rw` đọc ghi, `r` chỉ đọc, `w` chỉ ghi,
   `rc_w1` ghi 1 để xoá.
4. Đọc **bảng giải thích field** bên dưới: `MODEy = 00` input, `01` output 10 MHz, `10` output 2 MHz,
   `11` output 50 MHz; `CNFy` nghĩa khác nhau tuỳ input hay output.
5. Cuối chương luôn có **register map** (mục 9.5): một bảng gom offset + reset value của mọi thanh ghi
   trong chương. Đây là trang nên in ra dán cạnh bàn.

Mẹo: trong VS Code lúc debug, panel **XPERIPHERALS** hiện đúng các tên này kèm giá trị từng field, nên
bạn có thể đối chiếu PDF với giá trị thật trên chip.

## 4. Hex và binary: vì sao toàn thấy `0x`

`0x` nghĩa là số viết hệ 16 (hex). Lý do dùng hex: **1 chữ số hex = đúng 4 bit**, nên đọc hex là đọc
được từng nhóm 4 bit mà không phải tính.

| Hex | Binary | | Hex | Binary |
|---|---|---|---|---|
| `0` | `0000` | | `8` | `1000` |
| `1` | `0001` | | `9` | `1001` |
| `2` | `0010` | | `A` | `1010` |
| `3` | `0011` | | `B` | `1011` |
| `4` | `0100` | | `C` | `1100` |
| `5` | `0101` | | `D` | `1101` |
| `6` | `0110` | | `E` | `1110` |
| `7` | `0111` | | `F` | `1111` |

GPIO của F103 dùng **4 bit mỗi chân**, nên mỗi chữ số hex của `CRH` là cấu hình của đúng một chân.
Ví dụ `GPIOB->CRH = 0x444444B8` (đích của ex01), đọc từ **phải sang trái**:

```
digit:   4    4    4    4    4    4    B    8
pin:    15   14   13   12   11   10    9    8
         |                             |    +-- 8 = 1000: CNF=10 MODE=00 -> input pull-up/down
         |                             +------- B = 1011: CNF=10 MODE=11 -> alternate push-pull 50 MHz (CAN_TX)
         +------------------------------------- 4 = 0100: CNF=01 MODE=00 -> input floating (reset value)
```

Các số hay gặp: `0xF` = `1111` (mặt nạ 4 bit), `0xFFFF` = 16 bit 1, `0x2000` = bit 13, `0x20000000` =
bit 29. Calculator của Windows có chế độ **Programmer** đổi hex/binary/dec và hiện từng bit, rất tiện.

## 5. `U`, `UL` và các phép bit

**Hậu tố** cho biết kiểu của hằng số: `1` là `int` (có dấu), `1U` là `unsigned int`, `1UL` là
`unsigned long` (32 bit trên ARM). Thanh ghi là `uint32` không dấu, nên hằng số đi cùng cũng phải không
dấu. Lý do cụ thể: `1 << 31` dịch vào bit dấu của `int` là **undefined behavior** trong C, còn
`1UL << 31` thì hợp lệ. MISRA C (chuẩn code ô tô) bắt mọi phép dịch bit làm trên kiểu unsigned, nên
trong repo này bạn sẽ thấy `U`/`UL` ở mọi hằng số.

**Năm thao tác bit** dùng cho 95% code thanh ghi:

```c
uint32 r;   /* imagine r is any register */

r |=  (1UL << n);                 /* SET bit n, keep all others            */
r &= ~(1UL << n);                 /* CLEAR bit n, keep all others          */
r ^=  (1UL << n);                 /* TOGGLE bit n                          */
if ((r & (1UL << n)) != 0UL) {}   /* TEST: is bit n equal to 1?            */

/* WRITE A FIELD: put value v (4 bits wide) at bit position s */
r = (r & ~(0xFUL << s)) | ((uint32)v << s);
/*       ^ clear the 4 old bits   ^ OR in the new 4 bits */
```

Đọc thử một dòng trong `app/main.c` bằng bảng trên:

```c
GPIO_CRH(GPIOC_BASE) = (GPIO_CRH(GPIOC_BASE) & ~(0xFUL << 20)) | (0x2UL << 20);
```

PC13 nằm trong `CRH` (vì 13 ≥ 8), là chân thứ `13 − 8 = 5` trong thanh ghi đó, nên field 4 bit của nó
bắt đầu ở bit `5 × 4 = 20`. Dòng này là thao tác "write a field" với `v = 0x2` (`0010`: CNF=00
push-pull, MODE=10 output 2 MHz) và `s = 20`.

## 6. Lộ trình học (khoảng 1 tuần, xen với ex01)

| Ngày | Học | Tự kiểm tra |
|---|---|---|
| 1 | Mục 0–2 của tài liệu này; mở RM0008 tìm 3 thanh ghi `RCC_APB2ENR`, `GPIOx_CRH`, `GPIOx_BSRR` | nói được địa chỉ thật của `GPIOB->ODR` |
| 2 | Mục 4–5; làm bài tập mục 7 | giải mã được `0x44444444`, `0x44344444` |
| 3 | Debug blinky, step từng dòng `Led_Init`, xem XPERIPHERALS đổi | giải thích từng dòng `Led_Init` |
| 4 | Bước 3 khởi động: PB8 nối GND thì LED sáng (xem thread) | chạy trên board |
| 5+ | Bắt đầu ex01 Phần A (`Dio` trước, `Port` sau) | số check fail giảm |

### Video và bài đọc

Chọn video **viết code thanh ghi trực tiếp, không dùng HAL/CubeMX**. Cẩn thận: rất nhiều tutorial
"register" là cho STM32**F4**, ở đó GPIO dùng `MODER/OTYPER/PUPDR`, khác hẳn `CRL/CRH` của F103. Ý tưởng
giống nhau nhưng tên và bit khác, đừng chép code F4 sang.

- [Bare Metal Programming for ARM Cortex Processor Based STM32 MCU (playlist, Gettobyte)](https://www.youtube.com/playlist?list=PLb_Q-Ps0nJos2ZEHoDxO-mmlWlz1t7trR): 40 video, nhắm Blue Pill STM32F103, đi qua RCC, GPIO, timer, UART, SPI, I2C. Nguồn chính nên xem.
- [01 Led ON and OFF Bare Metal programming with BluePill (void loop Robotech)](https://www.youtube.com/watch?v=ES1vNmTjzBc): bật tắt LED trên F103 chỉ bằng địa chỉ thanh ghi, đúng mức bài ex01.
- [STM32 Bare Metal Videos (STM32World, series 0–9)](https://stm32world.com/wiki/STM32_Bare_Metal_Videos): hay ở phần debug thanh ghi (video #6), nhưng chạy trên chip F4 nên tên thanh ghi GPIO khác.
- [STM32F103 Clock Setup using Registers (ControllersTech)](https://controllerstech.com/stm32f103-clock-setup-using-registers/): bài đọc cho M2 Mcu sau này. Bài GPIO của trang này là F4, bỏ qua.

Sách nếu muốn đào sâu: *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors* (Joseph Yiu)
cho phần core, và chính RM0008 cho phần ngoại vi. Trong nghề ô tô, đọc reference manual là kỹ năng
dùng hằng ngày (với S32K, TC3xx, RH850 cũng y hệt), nên tập đọc RM0008 bây giờ là đáng công.

## 7. Bài tập tự kiểm tra (đáp án ở cuối)

1. Địa chỉ thật của `GPIOB->BSRR` là bao nhiêu? (GPIOB base `0x40010C00`, offset BSRR `0x10`)
2. `GPIOC->CRH = 0x44244444`: chân nào đang là output, tốc độ bao nhiêu?
3. Viết giá trị cần ghi vào `GPIOC->BSRR` để kéo PC13 xuống LOW.
4. `1UL << 4` bằng bao nhiêu ở dạng hex? Đó là bit nào trong `RCC_APB2ENR`?
5. Viết dòng C đặt PB9 thành alternate push-pull 50 MHz (nibble `0xB`) mà không đụng các chân khác.

<details><summary>Đáp án</summary>

1. `0x40010C00 + 0x10 = 0x40010C10`.
2. Chữ số thứ 6 từ phải (vị trí chân 13) là `2` = `0010`: PC13 output push-pull 2 MHz. Còn lại input floating.
3. `1UL << (13 + 16)` = `0x20000000`.
4. `0x10`, là bit 4 = `IOPCEN` (clock GPIOC).
5. `GPIO_CRH(GPIOB_BASE) = (GPIO_CRH(GPIOB_BASE) & ~(0xFUL << 4)) | (0xBUL << 4);` vì PB9 là chân `9 − 8 = 1` trong CRH, `1 × 4 = 4`.

</details>
