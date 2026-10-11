# Bài tập DBC 1 — Tự viết DBC cho `GVRET_Log.csv`

Người học: hi3rdt · Bạn tự viết DBC, mình review · Nền tảng: [walkthrough Bài 1](../savvycan-ex1-walkthrough.vi.md) (mục 5–7) và [frame CAN, counter, CRC](../can-frame-basics.vi.md)

> Đây là bài đầu tiên của **nhánh DBC** trong [roadmap](../roadmap.vi.md#phạm-vi-hiện-tại-2026-10-11-mcal--dbc-với-savvycan). Ở walkthrough bạn đã đoán được ý nghĩa của từng byte. Bài này biến các phỏng đoán đó thành một file DBC: SavvyCAN dùng nó để decode và vẽ đồ thị, còn cantools dùng nó để kiểm chứng. Về sau, cũng chính DBC này decode các frame mà Blue Pill gửi lên qua driver Can và Uart.

| Bạn làm | Kiểm chứng bằng | Xong khi |
|---|---|---|
| `dbc/gvret_log.dbc` mô tả ít nhất `0x228`, `0x0E`, `0x168`, `0x116`, `0x102` | `tools/check_dbc.py` (đã viết sẵn), cộng với đồ thị trong SavvyCAN | Checker chạy không lỗi, mọi counter chủ yếu là bước `+1`, và mỗi checksum/CRC có `CM_` ghi rõ thuật toán đã kiểm |

## 1. Đọc một dòng DBC

Một DBC tối thiểu chỉ cần vài khối. Ví dụ dưới đây là giả thuyết cho `0x168` trong walkthrough, viết đầy đủ:

```
VERSION ""

NS_ :

BS_:

BU_: XXX

BO_ 360 Msg_168: 8 XXX
 SG_ EngineSpeed_guess : 0|16@1+ (0.25,0) [0|16383.75] "rpm" Vector__XXX
 SG_ Counter_168 : 52|4@1+ (1,0) [0|15] "" Vector__XXX
 SG_ Checksum_168 : 56|8@1+ (1,0) [0|255] "" Vector__XXX

CM_ SG_ 360 Checksum_168 "Sum of bytes 0..6 plus 0x66, low byte. Matches 350 of 359 raw frames.";
BA_DEF_ BO_ "GenMsgCycleTime" INT 0 65535;
BA_DEF_DEF_ "GenMsgCycleTime" 0;
BA_ "GenMsgCycleTime" BO_ 360 20;
```

| Phần | Trong ví dụ | Ý nghĩa |
|---|---|---|
| `BU_:` | `XXX` | Danh sách node (ECU). Không biết ECU nào gửi thì dùng một tên chung |
| `BO_ <id> <tên>: <DLC> <node gửi>` | `BO_ 360 Msg_168: 8 XXX` | ID viết ở **hệ thập phân**: `0x168` = 360. ID extended thì cộng thêm `0x80000000` |
| `<start>\|<len>` | `52\|4` | Với Intel (`@1`), start bit là **bit thấp nhất** của signal, đánh số `byte * 8 + bit`. Nibble cao của byte 6 là bit 52..55 |
| `@1` / `@0` | `@1` | `1` = Intel (little-endian), `0` = Motorola (big-endian). Với Motorola, start bit là bit cao nhất và cách đánh số "răng cưa" khác hẳn, xem lại mục 3 của roadmap |
| `+` / `-` | `+` | Không dấu / có dấu (bù 2) |
| `(factor,offset)` | `(0.25,0)` | Giá trị vật lý = raw × factor + offset |
| `[min\|max]` | `[0\|16383.75]` | Khoảng hợp lệ. Checker đếm số frame nằm ngoài khoảng này |
| `"unit"` | `"rpm"` | Đơn vị |
| `CM_` | | Chú thích. Dùng nó để ghi **bằng chứng**: thuật toán checksum, số frame khớp |
| `BA_ "GenMsgCycleTime"` | `20` | Chu kỳ gửi (ms). Tool như CANoe dùng thuộc tính này để mô phỏng và giám sát timeout |

## 2. Tạo DBC trong SavvyCAN

Tên nút dưới đây lấy từ mã nguồn SavvyCAN; bản bạn cài có thể khác đôi chút.

1. Mở `GVRET_Log.csv` (**File → Load Log File**), như ở Bài 1.
2. **File → DBC File Manager → Create new DBC**. Một dòng mới hiện ra trong danh sách. Chọn dòng đó rồi bấm **Edit** để mở cửa sổ **DBC Editing Window**.
3. **New Node** để tạo node gửi (ví dụ `XXX`). Chọn node, bấm **New Message**, rồi điền **Frame ID** (nhập `0x168`), **Message Name**, **Frame Length** (DLC) và **Comments** trong **Message Editor**.
4. Chọn message, bấm **New Signal** rồi mở signal đó (double-click) để vào **Signal Editor**:
   - **Name**, **Bit Length**; tick **LSB First (Little Endian)** cho Intel;
   - **click vào ô bit trên lưới** để chọn start bit (với Intel là bit thấp nhất của signal). Lưới tô màu các bit mà signal chiếm, nên bạn thấy ngay signal có đè lên signal khác hay không;
   - **Type**: unsigned / signed; **Scale** là factor, **Bias** là offset; **Min Value**, **Max Value**, **Units Name**, **Comment**;
   - **Value Table** cho signal dạng trạng thái (ví dụ `0 = Off`, `1 = On`), sẽ ghi ra thành dòng `VAL_`.
5. Quay về **DBC File Manager → Save**, lưu thành `dbc/gvret_log.dbc` trong repo.
6. Bật **Interpret Frames** ở cửa sổ chính để xem giá trị đã decode, và dùng **RE Tools → New Graphing Window → Graph a DBC Signal** để vẽ đồ thị (như Bài 3 của hướng dẫn SavvyCAN).
7. Mở file `.dbc` vừa lưu bằng text editor và đọc lại từng dòng `SG_` theo bảng ở mục 1. SavvyCAN có thể ghi thêm các thuộc tính riêng của nó; giữ nguyên cũng được. `GenMsgCycleTime` và `CM_` có thể thêm bằng tay ngay trong file. Sau mỗi lần sửa tay, bấm **Remove** rồi **Load** lại file trong DBC File Manager.

Cách làm nhanh hơn khi đã quen: viết `SG_` bằng text editor, rồi chỉ dùng SavvyCAN để nạp file và nhìn đồ thị. Kỹ sư thật cũng làm cả hai cách.

## 3. Nội dung tối thiểu của `dbc/gvret_log.dbc`

| ID | Phải có | Gợi ý / nguồn |
|---|---|---|
| `0x228` | Counter 4 bit, CRC-8 | Walkthrough mục 3; CRC-8 SAE J1850 trên byte 0–2 ([giải thích](../can-frame-basics.vi.md)) |
| `0x0E` | Counter, CRC-8 ở byte 7 | Tự tìm vị trí counter bằng Frame Data Analysis; CRC giống `0x228` nhưng trên byte 0–6 |
| `0x168` | Giá trị 16 bit, counter, checksum | Walkthrough mục 7 |
| `0x116` | Counter ở **nibble thấp** byte 4, checksum | Walkthrough mục 7. Checksum có cộng hai byte của chính ID |
| `0x102` | Ba giá trị 16 bit | Walkthrough mục 5. Tự quyết định có dấu hay không dấu, rồi ghi lý do vào `CM_` |

Đặt tên signal có chứa `Counter` cho counter để checker nhận ra. Signal mà bạn chỉ đoán được ý nghĩa thì thêm hậu tố `_guess`. Một DBC trung thực phân biệt rõ cái đã chứng minh và cái đang là giả thuyết.

## 4. Kiểm chứng bằng `tools/check_dbc.py`

```bash
pip install -r tools/requirements.txt
python tools/check_dbc.py --dbc dbc/gvret_log.dbc --log path/to/GVRET_Log.csv
```

Checker đọc thẳng file CSV của SavvyCAN (hoặc file candump `.log`), decode mọi frame có ID trong DBC, và in ra cho mỗi message: số frame, số frame sai DLC, chu kỳ đo được trong log so với `GenMsgCycleTime`, khoảng min..max của từng signal, số frame nằm ngoài `[min|max]`, và với counter thì số bước `+1`, số lần lặp và số lần nhảy. DBC được nạp ở chế độ strict, nên signal đè lên nhau hoặc vượt quá DLC sẽ bị báo lỗi ngay. Mã thoát: 0 nếu không có vấn đề, 1 nếu có, 2 nếu DBC sai cú pháp.

Cách đọc phần counter trên log này: "repeats" phần lớn là các dòng bị bộ ghi ghi trùng, còn "jumps" là frame bị mất (walkthrough mục 4). Nếu bạn đặt counter đúng vị trí, bước `+1` chiếm đa số. Nếu đặt sai, kết quả trông khác hẳn. Ví dụ đặt counter của `0x228` ở nibble **thấp** của byte 2 (`16|4`) thì checker in `0 .. 1` và `865 repeats`, vì nibble đó gần như không đổi.

Tự kiểm tra (các số dưới đây đếm trên log thô, chưa bỏ dòng trùng):

| Kiểm tra | Kết quả đúng |
|---|---|
| `0x228`: counter | 880 frame, `755 x +1, 67 repeats, 57 jumps` |
| `0x228`: CRC-8 J1850 trên byte 0–2 | khớp 871/880 (9 frame lệch đều chứa byte `F1` hỏng) |
| `0x0E`: CRC-8 J1850 trên byte 0–6 | khớp 722/725 |
| `0x168`: giá trị 16 bit × 0,25 | `809.75 .. 845.25` |
| `0x168`: counter | `259 x +1, 18 repeats, 81 jumps`; chu kỳ log `20.0 ms` |
| `0x168`: checksum tổng + `0x66` | khớp 350/359 |
| `0x116`: counter | `796 x +1, 100 repeats, 37 jumps` |
| `0x116`: checksum tổng + `0x16` + `0x01` | khớp 911/934 |

Checker không đoán thuật toán checksum/CRC: phần đó bạn kiểm bằng hàm `Crc8_SaeJ1850` trong [can-frame-basics](../can-frame-basics.vi.md) hoặc bằng một script nhỏ của riêng bạn, rồi ghi kết quả vào `CM_`. Không cần commit script đó, nhưng nếu muốn thì đặt trong `tools/` và mình review luôn.

Kiểm cuối cùng bằng mắt: nạp DBC vào SavvyCAN, vẽ `EngineSpeed_guess` theo thời gian, và mở Overwrite Mode với Interpret Frames để xem counter đếm đều. Chụp ảnh đồ thị, ảnh này sẽ dùng cho README của project.

## 5. Checklist review (mình sẽ dùng khi bạn push)

- **Cú pháp:** checker nạp được ở chế độ strict; ID ghi đúng hệ thập phân; DLC khớp log.
- **Bit layout:** start bit đúng quy ước Intel; không signal nào đè lên nhau; counter có độ dài đúng (4 bit thì `[0|15]`).
- **Bằng chứng:** mỗi checksum/CRC có `CM_` ghi thuật toán và tỉ lệ khớp; mỗi giá trị chỉ đoán được ý nghĩa có hậu tố `_guess` và `CM_` ghi lý do.
- **Chu kỳ:** `GenMsgCycleTime` theo chu kỳ thật (khoảng cách hay gặp nhất), không theo trung bình đã bị kéo lên vì mất frame.
- **Đặt tên:** tên message và signal theo một quy ước thống nhất (ví dụ `Msg_168`, `Counter_168`), không có dấu cách.

## 6. Cách nộp

1. Tạo nhánh, ví dụ `dbc01-gvret-log`, commit `dbc/gvret_log.dbc` và ảnh đồ thị (ví dụ `docs/img/`), push và mở PR vào `master`.
2. Dán output của `check_dbc.py` vào mô tả PR.
3. Nhắn trong thread "review dbc01" kèm link PR.
