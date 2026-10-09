# Bài 1 chi tiết — Đọc `GVRET_Log.csv` từng cột, từng con số

Bổ sung cho [savvycan-guide.vi.md](savvycan-guide.vi.md), Bài 1. File: [`SavvyCAN/examples/GVRET_Log.csv`](https://github.com/collin80/SavvyCAN/blob/master/examples/GVRET_Log.csv). Mọi con số dưới đây được đếm trực tiếp từ file bằng script, không phải ước lượng.

## 1. Mở file trong SavvyCAN, từng bước

1. Mở SavvyCAN. Không cần kết nối thiết bị, bỏ qua cửa sổ Connection nếu nó hiện ra.
2. **File → Load Log File**. Ở góc dưới hộp thoại, ô loại file chọn **GVRET Logs (\*.csv \*.CSV)**, rồi chọn `GVRET_Log.csv`. Thanh trạng thái hiện *Total Frames Captured: 12974*.
3. Bảng chính giờ có các cột Timestamp, ID, Ext, Dir, Bus, Len, Data. Kéo xuống để thấy các frame xếp theo thời gian.
4. Tick **Overwrite Mode**. Bảng co lại còn **180 dòng**, mỗi dòng là một ID, và giá trị trong dòng nhảy theo frame mới nhất. Đây là cách nhanh nhất để xem "bus có bao nhiêu ID". Bỏ tick để quay lại xem theo thời gian.
5. Ở khung **Frame Filtering** bên trái, bấm **None** để bỏ chọn hết, rồi tick riêng `0x0E`. Bảng chỉ còn frame của ID đó.
6. **RE Tools → Frame Data Analysis**. Chọn `0x0E` trong danh sách *Frame IDs* bên trái. Đọc phần *Details* (số frame, khoảng cách giữa các frame, bit nào đã từng thay đổi) và tab *Interval Histogram*.
7. **RE Tools → Sniffer**. Tick **Never Expire IDs**. Sniffer chạy theo dữ liệu đang có; byte vừa đổi được tô màu. Bấm **Notch** để "đóng băng" trạng thái hiện tại, từ đó chỉ bit nào thay đổi *thêm* mới sáng.

## 2. Ý nghĩa từng cột của file CSV

Dòng đầu tiên của file (header) và hai dòng dữ liệu đầu:

```
Time Stamp,ID,Extended,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8
166064000,0000021A,false,0,8,FE,36,12,FE,69,05,07,AD,
166064000,00000228,false,0,4,40,C0,30,F8,00,00,00,00,
```

| Cột | Ví dụ | Ý nghĩa |
|---|---|---|
| `Time Stamp` | `166064000` | Thời điểm thiết bị ghi nhận frame, đơn vị **micro giây** tính từ lúc bộ ghi (GVRET) bật nguồn, không phải giờ thực. `166064000` µs = 166,064 s. Log kéo dài từ 166,064 s đến 174,624 s, tức **8,56 s**. |
| `ID` | `0000021A` | CAN identifier dạng **hex**, đệm 8 chữ số. `0000021A` = `0x21A` = 538. Với frame chuẩn 11-bit, ID chỉ đi từ `0x000` đến `0x7FF`. ID càng nhỏ thì ưu tiên càng cao khi arbitration. |
| `Extended` | `false` | `false` = frame chuẩn (11-bit ID), `true` = frame mở rộng (29-bit, thường gặp ở J1939/xe tải, UDS 29-bit). File này **100 % là `false`**. |
| `Bus` | `0` | Số kênh CAN của bộ ghi (GVRET có thể ghi 2 bus). File này **toàn bộ là bus 0**. |
| `LEN` | `8`, `4` | **DLC**: số byte dữ liệu thật, 0–8 với CAN classic. Phân bố trong file: 8 byte (8319 frame), 6 (1927), 4 (1455), 7 (487), 1 (336), 3 (195), 5 (169), 2 (86). |
| `D1`…`D8` | `FE`,`36`… | Byte dữ liệu 0…7, dạng hex. **Chỉ `LEN` byte đầu có nghĩa**: dòng `0x228` có `LEN = 4` nên `00,00,00,00` ở D5–D8 chỉ là phần đệm của file, không có trên bus. |
| dấu phẩy cuối | | Thừa, không có cột nào. SavvyCAN bỏ qua. |

So với file `demo_drive.csv` của project, header ở đây thiếu cột `Dir` (Rx/Tx). Đó là định dạng GVRET **phiên bản 1**; SavvyCAN tự nhận ra phiên bản bằng cách kiểm tra header.

## 3. Đọc một frame bằng mắt

Lấy dòng `166064000,00000228,false,0,4,40,C0,30,F8`:

- Lúc 166,064 s, ECU nào đó gửi ID `0x228`, frame chuẩn, trên bus 0, 4 byte dữ liệu: byte0 = `0x40`, byte1 = `0xC0`, byte2 = `0x30`, byte3 = `0xF8`.
- Không có DBC nên ta **chưa biết** byte nào là signal gì. Việc của bài này là đoán từ cách các byte thay đổi theo thời gian.

Mười frame `0x228` đầu tiên:

```
byte0 byte1 byte2 byte3
 40    C0    30    F8
 40    C0    40    F1
 40    C0    50    6C
 40    C0    60    26
 40    C0    70    EB
 40    C0    80    94
 40    C0    90    59
 40    C0    A0    13
 40    C0    B0    DE
 40    C0    C0    87
```

- byte0 = `0x40` **không bao giờ đổi** trong cả log: hằng số, có thể là trạng thái hoặc bit cấu hình.
- byte1 chỉ có 3 giá trị (`0xC0`…`0xC8`): một trạng thái ít khi đổi.
- **Nibble cao của byte2 tăng 3, 4, 5 … F, 0, 1 …**: đây là **alive counter** (rolling counter) 4 bit. ECU nhận dùng nó để phát hiện frame bị mất, bị lặp, hoặc ECU gửi bị treo.
- byte3 nhảy loạn, không có quy luật với mắt thường, và đổi mỗi khi counter đổi: đó là **CRC**. Đã kiểm chứng: byte3 = CRC-8 SAE J1850 trên byte 0–2, khớp 871/880 frame (xem [can-frame-basics.vi.md](can-frame-basics.vi.md)). Đó đúng là cặp "counter + CRC" của cơ chế **E2E protection** trong AUTOSAR.

`0x0E` có cùng kiểu: byte0 (`0x20`) và byte2 (`0x3F`) cố định, byte1 dao động chậm trong khoảng `0x5D`–`0x73`, **nibble cao byte6 là counter** (`20, 30, 50, 60…`), byte7 là checksum.

## 4. Chu kỳ gửi: con số nào đáng tin

| ID | Số frame | Frame trùng lặp | Chu kỳ ước tính | Nhận xét |
|---|---|---|---|---|
| `0x102` | 936 | 110 | ~10 ms | Dữ liệu "nhiễu" liên tục, xem mục 5 |
| `0x116` | 934 | 86 | ~10 ms | |
| `0x228` | 880 | 66 | ~10 ms | counter + CRC |
| `0x0E` | 725 | 3 | ~10 ms, có mất frame | counter nhảy cóc 123 lần |
| `0x754` | 529 | 18 | ~16 ms | |
| `0x1F8` | 421 | 9 | ~20 ms | |
| `0x168` | 359 | 17 | ~25 ms | |
| `0x3xx` | 4–11 | | ~1 s | trạng thái chậm |
| `0x500`, `0x558` | 1 | | chỉ xuất hiện một lần | sự kiện, không định kỳ |

Ba điều cần biết khi đọc chu kỳ trong file này, vì chúng là "lỗi của bộ ghi", không phải của xe:

1. **Độ phân giải timestamp chỉ 10 ms.** Cả file chỉ có 858 giá trị timestamp khác nhau, cách nhau đúng 10 000 µs, và nhiều frame cùng một timestamp. Vì vậy *Interval Histogram* sẽ cho khoảng cách 0 ms hoặc bội số của 10 ms; không đo được chu kỳ chính xác hơn 10 ms từ file này.
2. **Frame bị ghi trùng.** 736 dòng giống hệt một dòng khác (cùng timestamp, cùng data). Trên bus thật một frame không thể xuất hiện hai lần cùng lúc, đây là bộ ghi gửi lặp. Nên đếm chu kỳ trên dữ liệu đã bỏ trùng.
3. **Frame bị mất.** Counter của `0x0E` nhảy cóc 123 lần trên 722 frame (ví dụ `3 → 5`, thiếu 4). Bộ ghi không theo kịp tải bus.

Có thêm một dấu hiệu nữa: có 54 chỗ một byte đang là `FF` thì frame kế tiếp cùng ID thành `F1` rồi lại về `FF` (ví dụ `0x102` tại 166,154 s: `A2,F1` rồi `A2,FF` cùng timestamp). Bus CAN có CRC 15 bit, frame sai bit sẽ bị loại ngay trên bus, nên giá trị `F1` này nhiều khả năng do bộ ghi/đường serial làm hỏng. CRC-8 trong data xác nhận điều đó: cả 9 frame `0x228` sai CRC đều là frame chứa `F1`. Bài học: **luôn nghi ngờ dữ liệu log trước khi nghi ngờ ECU**.

## 5. Đoán signal không cần DBC: ví dụ `0x102`

```
byte0 byte1 byte2 byte3 byte4 byte5      (LEN = 6)
 09    96    D6    FF    D6    FF
 16    96    D0    FF    CB    FF
 FD    95    D1    FF    95    FF
 08    96    D6    FF    99    FF
```

- byte3 và byte5 gần như luôn là `FF`, byte2 và byte4 dao động quanh `0xD0`–`0xE1` và `0x95`–`0xD6`. Ghép thành số 16 bit **little-endian** (byte thấp trước): bytes 2–3 = `0xFFD6`, đọc như số **có dấu** là **−42**. Đó là một giá trị nhỏ, âm, rung nhẹ quanh một điểm: dáng của cảm biến (gia tốc, yaw rate, góc lái) khi xe đứng yên.
- bytes 0–1 = `0x9609`, `0x9616`, `0x95FD`…: cũng là giá trị 16 bit little-endian rung quanh `0x9600`. Lưu ý `0x95FD` → `0x9608` là chỉ +11, dù byte1 đổi từ `95` sang `96`: nếu đọc riêng từng byte bạn sẽ tưởng là nhảy lớn. Đây là lý do phải thử ghép byte.
- Đây mới là **giả thuyết**. Để khẳng định cần thao tác trên xe (đánh lái, tăng tốc) và xem giá trị có phản ứng không. Trên một log 8,56 s không rõ xe đang làm gì, ta chỉ đi đến được mức "có vẻ là 3 giá trị cảm biến 16 bit có dấu".

Hai frame đặc biệt:

- `0x558` xuất hiện **một lần**, data `32 39 66 33 31 64 33 00`. Đổi sang ASCII: `"29f31d3"` kết thúc bằng `00`. Đây là một **chuỗi ký tự** (có thể là một phần mã phần mềm hoặc số serial). Mẹo: khi thấy các byte nằm trong khoảng `0x30`–`0x7A`, thử đọc như ASCII.
- `0x500` cũng chỉ một lần: `FF FF FF FF 01 01 00 FF`. `FF` thường có nghĩa "không hợp lệ / chưa có giá trị".

## 6. Phương pháp tổng quát để áp dụng cho log khác

1. **Overwrite Mode**: đếm số ID, nhìn nhanh byte nào "sống".
2. **Frame Data Analysis**: với mỗi ID, ghi lại DLC, chu kỳ, byte cố định, byte thay đổi.
3. Phân loại byte thay đổi:
   - tăng đều 0→F hoặc 0→FF rồi quay vòng: **counter**;
   - nhảy loạn và đổi cùng lúc với counter: **checksum/CRC**;
   - thay đổi trơn tru, ghép 2 byte mới có nghĩa: **giá trị vật lý** (thử cả little-endian và big-endian, cả có dấu và không dấu);
   - chỉ 0/1 ở một bit: **cờ trạng thái** (đèn, cửa, công tắc);
   - nằm trong `0x30`–`0x7A`: thử **ASCII**.
4. **Sniffer + Notch**: thao tác một thứ (thật hoặc trong đoạn log có sự kiện), xem bit nào sáng.
5. Viết giả thuyết thành dòng `SG_` trong DBC, nạp lại, vẽ đồ thị để chứng minh. Đó là Bài 4 trong hướng dẫn chính.

## 7. Ví dụ áp dụng mục 6: `0x168` và `0x116`

Các con số dưới đây tính trên frame đã bỏ dòng trùng và bỏ frame chứa byte hỏng `F1`.

### `0x168` — DLC 8, chu kỳ 25 ms, 332 frame

```
byte:  0  1  2  3  4  5  6  7
      A9 0C 1C 7F 00 00 F0 A6
      A9 0C 1C 7F 00 00 20 D6
      A8 0C 1C 7F 00 00 30 E5
      A9 0C 1C 7F 00 00 40 F6
```

| Bước | Quan sát | Kết luận |
|---|---|---|
| Byte cố định | byte2 = `1C`, byte3 = `7F`, byte4 = `00`, byte5 = `00` trong cả log | hằng số hoặc signal không hoạt động lúc ghi log |
| Counter | nibble cao byte6: `F, 2, 3, 4, 7, 8, 9, B, C, E, F, 0…` (nhảy cóc là do bộ ghi mất frame) | **alive counter 4 bit** |
| Checksum | byte7 đổi mỗi frame. Thử CRC-8 J1850 như `0x228` thì **không khớp**. Thử cộng: `byte7 = (byte0 + … + byte6 + 0x66) & 0xFF` khớp **332/332** | **checksum kiểu tổng (sum)**, không phải CRC. Ví dụ: `A9+0C+1C+7F+00+00+F0 = 0x240`, `+0x66 = 0x2A6`, lấy byte thấp = `A6` |
| Giá trị vật lý | byte0 dao động `A8…FC` rồi quay về `02`, đúng lúc byte1 đổi `0C → 0D` | byte0 và byte1 là **một số 16 bit little-endian**: `0x0CA9`… `0x0D35`, tức 3239…3381, thay đổi trơn tru |
| Đoán ý nghĩa | `0x0CA9` × 0,25 = **810**, khoảng giá trị 810–845 | rất giống **tốc độ động cơ (rpm) ở chế độ idle** với hệ số 0,25 rpm/bit, đúng hệ số `EngineRpm` trong DBC của project. Đây là **giả thuyết**; cần log lúc đạp ga để khẳng định |

DBC giả thuyết:

```
BO_ 360 Unknown_168: 8 XXX
 SG_ EngineSpeed_guess : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Vector__XXX
 SG_ Counter_168 : 52|4@1+ (1,0) [0|15] "" Vector__XXX
 SG_ Checksum_168 : 56|8@1+ (1,0) [0|255] "" Vector__XXX
```

### `0x116` — DLC 6, chu kỳ 10 ms, 825 frame

```
byte:  0  1  2  3  4  5
      00 10 F4 11 8E BA
      00 10 F4 11 8F BB
      00 10 F4 11 80 AC
      00 10 F4 11 82 AE
```

| Bước | Quan sát | Kết luận |
|---|---|---|
| Byte cố định | byte0 = `00` trong cả log | hằng số |
| Counter | **nibble thấp** byte4 tăng `E, F, 0, 2, 3…` | **counter 4 bit**, lần này nằm ở nibble thấp. Nibble cao của byte4 đổi `8 → 9` cùng lúc với byte1 đổi `10 → 90`, nên nhiều khả năng là một cờ trạng thái |
| Checksum | `byte5 = (byte0 + … + byte4 + 0x16 + 0x01) & 0xFF` khớp **825/825** | **checksum = tổng data + 2 byte của chính ID** (`0x116` → `0x16`, `0x01`). Thêm ID vào checksum giúp phát hiện frame bị gửi nhầm ID |
| Còn lại | byte1 (bit 7 bật/tắt), byte2, byte3 thay đổi chậm | các trạng thái/giá trị chưa đủ dữ liệu để đoán |

Bài học rút ra:
- Cùng một xe nhưng mỗi ECU dùng một kiểu bảo vệ khác nhau: `0x228` dùng CRC-8 J1850, còn `0x168` và `0x116` dùng checksum tổng (cộng thêm ID hoặc một hằng số). Khi reverse-engineer, thử theo thứ tự: tổng, XOR, CRC-8 J1850, rồi các CRC khác.
- Counter có thể nằm ở nibble cao hoặc nibble thấp, và ở bất kỳ byte nào.
- Hai byte cạnh nhau mà một byte "quay vòng" đúng lúc byte kia tăng 1 thì đó là một số 16 bit.

## Bài tập tự làm

1. Làm lại mục 3 cho `0x1F8`: byte nào cố định, byte nào là counter, có checksum không?
2. Dùng Frame Data Analysis, tìm thêm **hai** ID khác có counter 4 bit.
3. Đếm xem có bao nhiêu ID xuất hiện ít hơn 15 lần trong log, và đoán vì sao chúng thưa như vậy.

Ghi câu trả lời vào `docs/notes/phase1-savvycan.md` rồi nhắn mình review.
