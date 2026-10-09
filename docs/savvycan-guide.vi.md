# Thực hành SavvyCAN: đọc frame, decode signal, reverse-engineer

Người học: hi3rdt · Giai đoạn 1 của [roadmap](roadmap.vi.md) · Không cần phần cứng

Mục tiêu: sau 5 bài dưới đây bạn tự mở được một log CAN bất kỳ, biết ID nào chu kỳ bao nhiêu, decode signal bằng DBC, vẽ đồ thị, và tự tìm ra một signal chưa có trong DBC. Mỗi bài có phần **Tự kiểm tra** với con số cụ thể để bạn biết mình làm đúng hay chưa.

## 0. Chuẩn bị

**Cài SavvyCAN.** Vào trang [Releases của SavvyCAN](https://github.com/collin80/SavvyCAN/releases), tải bản mới nhất cho hệ điều hành của bạn (Windows: file `.zip`, giải nén rồi chạy `SavvyCAN.exe`; Linux: file `.AppImage`, `chmod +x` rồi chạy; macOS: file `.dmg`). Không cần cài driver hay kết nối thiết bị nào cho các bài này, vì toàn bộ là phân tích offline.

**Hai file log dùng trong bài:**

| File | Nguồn | Nội dung | Dùng cho |
|---|---|---|---|
| `GVRET_Log.csv` | [SavvyCAN/examples](https://github.com/collin80/SavvyCAN/blob/master/examples/GVRET_Log.csv) (tải bằng nút *Download raw file*) | Log thật từ một xe không rõ hãng, 12 974 frame, 180 ID, dài 8,56 s, **không có DBC** | Bài 1: làm quen giao diện, đo chu kỳ, đoán counter |
| `samples/demo_drive.csv` (và `.log` dạng candump) | Repo này, sinh bởi `tools/gen_demo_log.py` từ `config/gateway.dbc` | 30 s lái thử: nổ máy, tăng tốc lên 80 km/h, vào cua trái, phanh gấp có ABS, dừng, mở cửa. 5 130 frame | Bài 2–5: decode bằng DBC, vẽ đồ thị, reverse-engineer, export |

Vì sao log thứ hai là log sinh ra: mình không tìm được một cặp log thật + DBC công khai mà khớp nhau và đủ nhỏ để học. Log `demo_drive` được sinh từ chính DBC của project nên mọi signal decode được đúng 100%, và chính log này sẽ được phát lại vào STM32 ở mốc 4. Bạn có thể sinh lại bất cứ lúc nào (kết quả giống hệt từng byte vì seed cố định):

```bash
pip install -r tools/requirements.txt
python tools/gen_demo_log.py --dbc config/gateway.dbc --out samples/demo_drive
```

**Công cụ đối chiếu.** `cantools` (Python) giải mã cùng log ở dòng lệnh. Mỗi khi SavvyCAN cho ra một con số, đối chiếu với cantools: nếu hai bên lệch nhau thì hoặc bạn cấu hình sai, hoặc DBC sai. Đây cũng là cách project này kiểm chứng output của STM32 sau này.

```bash
python -m cantools decode --single-line config/gateway.dbc < samples/demo_drive.log | head
```

## Bài 1 — Đọc log thật không có DBC (`GVRET_Log.csv`)

1. **File → Load Log File**, chọn `GVRET_Log.csv`, ở ô loại file chọn *GVRET Logs (\*.csv)*. Cửa sổ chính hiện 12 974 dòng: Timestamp, ID, Ext, Dir, Bus, Len, Data.
2. Bật **Overwrite Mode** (checkbox ở cửa sổ chính). Mỗi ID giờ chỉ còn một dòng và byte nào đang thay đổi sẽ nhảy liên tục; đây là chế độ giống *Trace* cố định vị trí trong CANoe. Tắt đi để quay lại xem theo thời gian.
3. Cột **Frame Filtering** bên trái liệt kê mọi ID. Bấm **None** rồi chỉ tick `0x102` để chỉ xem một ID.
4. **RE Tools → Frame Data Analysis**: chọn ID `0x102` ở danh sách bên trái. Phần *Details* cho biết số frame, khoảng thời gian giữa các frame (min/max/avg) và với mỗi byte thì bit nào từng thay đổi; *Interval Histogram* cho thấy chu kỳ có đều không.
5. **RE Tools → Sniffer**: chọn **Never Expire IDs**. Sniffer tô màu byte vừa đổi giá trị; tick **Notch** để "đánh dấu" trạng thái hiện tại, sau đó chỉ những bit thay đổi tiếp mới sáng lên. Đây là kỹ thuật bit-flip cơ bản: đứng yên, notch, rồi thao tác một thứ trên xe và xem bit nào sáng.

**Tự kiểm tra (Bài 1):**
- `0x102` có 936 frame trong 8,56 s, tức chu kỳ khoảng 9 ms ≈ **10 ms**. `0x0E` có 725 frame (≈ 12 ms). `0x754` có 529 frame (≈ 16 ms). Các ID `0x3xx` chỉ có 4–11 frame (chu kỳ ≈ 1 s): thường là frame trạng thái/cấu hình chậm.
- Câu hỏi cho bạn: trong `0x102`, có nibble nào tăng đều 0→F rồi quay vòng không (alive counter)? Ghi lại byte/bit và cách bạn nhận ra nó trong Frame Data Analysis.

> Ghi chú automotive: chu kỳ 10/20/100/1000 ms là các "task rate" kinh điển của ECU (OS task 10 ms cho powertrain/chassis, 100 ms cho body, 1 s cho diag/odometer). Nhìn chu kỳ là đoán được frame thuộc domain nào trước cả khi có DBC.

## Bài 2 — Decode signal bằng DBC (`demo_drive.csv` + `gateway.dbc`)

1. **Clear Frames** (nút ở cửa sổ chính), rồi **File → Load Log File** → `samples/demo_drive.csv` (*GVRET Logs*).
2. **File → DBC File Manager → Load** → chọn `config/gateway.dbc`. Kiểm tra cột bus của DBC là `-1` (áp dụng cho mọi bus) hoặc `0` (bus của log). Đóng cửa sổ.
3. Ở cửa sổ chính, tick **Interpret Frames**. Mỗi frame giờ có thêm tên message và giá trị vật lý của từng signal, ví dụ `VehicleSpeed: 37.11 km/h`.
4. Đọc DBC bằng text editor, đặt cạnh SavvyCAN. Với mỗi dòng `SG_`, tự trả lời: start bit, độ dài, `@1` (Intel) hay `@0` (Motorola), `+`/`-` (unsigned/signed), factor, offset.

**Tự kiểm tra (Bài 2), tính tay từ byte rồi so với SavvyCAN:**
- Frame `0x100` tại t = 8,770 s có data `7F 0E 74 61 17 00 00 9D`. `VehicleSpeed` = `0|16@1+ (0.01,0)` → raw = byte1:byte0 = `0x0E7F` = 3711 → **37,11 km/h**.
- Frame `0x200` tại t = 8,760 s có data `17 29 41 58 07 80 00 00`. `EngineRpm` = `7|16@0+ (0.25,0)` là **Motorola**: MSB ở bit 7 của byte 0, nên raw = byte0:byte1 = `0x1729` = 5929 → **1482,25 rpm**. Nếu bạn ra 2629,75 rpm (`0x2917` × 0,25) thì bạn vừa đọc nhầm thành Intel; đây đúng là lỗi decode số 1 ngoài thực tế.
- Cũng frame đó: `EngineTorque` = `37|11@0- (0.5,0)` là Motorola **có dấu** 11 bit. Ở t = 23,38 s (đang phanh bằng động cơ) giá trị phải là **−30,0 Nm**. Tự lấy byte 4–5 của frame đó, tách 11 bit và làm phép bù 2 để ra −60 raw.
- Frame `0x18FEEF00` (29-bit, ID extended) chu kỳ 1 s: `OdometerKm` = **12345 km**, `AmbientTemp` = **27,5 °C**.

## Bài 3 — Vẽ đồ thị và đọc câu chuyện của chuyến đi

1. **RE Tools → New Graphing Window**. Click chuột phải vào vùng đồ thị → **Add new graph**. Ở mục *Graph a DBC Signal* chọn Message `VehicleSpeed`, Signal `VehicleSpeed` → **Add this graph**.
2. Thêm lần lượt: `EngineRpm`, `EngineTorque`, `SteeringAngle`, `AbsActive`, `DoorFL`. Chuột phải → **Rescale to data**.
3. Chuột phải → **Save graph image to file**, lưu thành `docs/img/savvycan_speed_rpm.png` (ảnh này sẽ vào README của project ở mốc 5).

**Tự kiểm tra (Bài 3): đồ thị phải kể đúng câu chuyện này**

| Thời điểm | Sự kiện | Signal |
|---|---|---|
| 1,0 s | Nổ máy | `EngineRpm` từ 0 lên ~800 rpm (idle) trong 1 s |
| 4–14 s | Tăng tốc | `VehicleSpeed` 0 → 80 km/h; `EngineRpm` hình răng cưa do sang số, đỉnh **2102,5 rpm**; `EngineTorque` = +120 Nm |
| 16–19 s | Vào cua trái | `SteeringAngle` lên tối đa ~**44,8°**; `WheelSpeedFR` > `WheelSpeedFL` (bánh ngoài quay nhanh hơn) |
| 20–27 s | Phanh | `EngineTorque` = **−30 Nm**; `AbsActive` = 1 từ **23,5 s** đến 25 s |
| 28,5 s | Mở cửa trước trái | `DoorFL` = 1 |

## Bài 4 — Reverse-engineer frame không có trong DBC (`0x2F0`)

Trong `demo_drive` có ID `0x2F0` mà DBC không định nghĩa (SavvyCAN hiển thị nó không có tên dù đã bật Interpret Frames). Nhiệm vụ: tìm ra ý nghĩa của nó, chỉ dựa vào log, giống hệt việc người ta làm với xe thật.

1. **Frame Data Analysis** với `0x2F0`: chu kỳ bao nhiêu? Byte nào không bao giờ đổi? Bit nào có đổi?
2. **Sniffer**: tua log (dùng **RE Tools → Flow View**, nút play, chỉnh *Playback Speed*) và nhìn byte nào sáng **đúng lúc** các sự kiện ở bảng Bài 3 xảy ra. Gợi ý: trùng thời điểm với nổ máy? với lúc vào cua? Cái gì trên xe nhấp nháy khi vào cua?
3. **Flow View**: chọn `0x2F0`, xem đồ thị từng bit theo thời gian. Bit dạng xung vuông đều là đèn/nháy; nibble tăng 0→F là counter.
4. Với cặp byte nghi là một giá trị 16 bit, thử cả hai cách ghép (byte n:n+1 và n+1:n). Cách nào cho ra con số "có ý nghĩa vật lý" và thay đổi trơn tru thì đó là byte order đúng.
5. Viết giả thuyết thành các dòng `SG_` dưới một `BO_ 752 Mystery: 8 BCM` trong một **bản sao** của DBC (đừng sửa `config/gateway.dbc` vì file đó sinh code cho project), nạp lại bằng DBC File Manager và vẽ đồ thị để chứng minh.

Có 4 signal để tìm: 1 bit nháy theo nhịp, 1 bit bật một lần và giữ nguyên, 1 giá trị 16 bit, 1 counter 4 bit. Đáp án ở cuối file; chỉ mở sau khi đã viết xong DBC của bạn.

**Bài nâng cao:** `SpeedChecksum` (4 bit cao của byte 7 trong `0x100`) là một checksum đơn giản. Dùng vài frame trong log, tìm thuật toán (gợi ý: nó tính trên các nibble). Viết một hàm Python kiểm tra checksum cho toàn bộ 3000 frame `0x100`; nếu tất cả khớp là bạn đúng.

## Bài 5 — Lọc và export

1. Frame Filtering: **None**, rồi chỉ tick `0x100` và `0x200`.
2. **File → Save Filtered Log File**, chọn định dạng *Candump/Kayak* → `speed_rpm.log`.
3. **File → Save Decoded Frames CSV** để có bảng giá trị vật lý (mở bằng Excel).
4. Kiểm tra lại file export bằng cantools: `python -m cantools decode config/gateway.dbc < speed_rpm.log | head`. Nếu cantools decode được thì file export đúng định dạng.

## Nộp bài

Viết `docs/notes/phase1-savvycan.md` (tiếng Việt hoặc tiếng Anh tùy bạn) gồm: chu kỳ các ID ở Bài 1 và counter bạn tìm được, phép tính tay ở Bài 2, ảnh đồ thị Bài 3, DBC của frame `0x2F0` ở Bài 4. Push lên một nhánh và nhắn trong thread, mình sẽ review.

---

<details>
<summary>Đáp án Bài 4 (chỉ mở sau khi đã làm)</summary>

```
BO_ 752 Mystery: 8 BCM
 SG_ TurnIndicatorLeft : 0|1@1+ (1,0) [0|1] "" GW
 SG_ HeadlampOn : 2|1@1+ (1,0) [0|1] "" GW
 SG_ FuelLevel : 15|8@0+ (0.5,0) [0|127.5] "%" GW
 SG_ BatteryVoltage : 23|16@0+ (0.01,0) [0|655.35] "V" GW
 SG_ MysteryCounter : 56|4@1+ (1,0) [0|15] "" GW
```

- Chu kỳ 100 ms.
- Bit 0 byte 0 nháy 1,5 Hz từ 15,5 s đến 19 s: xi nhan trái, bật trước khi đánh lái và tắt khi trả lái.
- Bit 2 byte 0 lên 1 tại 15 s và giữ nguyên: đèn pha.
- Byte 1 = `0x7C` không đổi: mức nhiên liệu 62 % (0,5 %/bit). Không thể đoán chắc chỉ từ log 30 s; đó là giới hạn thật của reverse-engineering.
- Byte 2–3 big-endian (Motorola), 0,01 V/bit: **12,40 V** khi tắt máy, **~14,20 V** sau khi nổ máy (máy phát sạc). Ghép sai thứ tự byte sẽ ra giá trị nhảy loạn.
- 4 bit thấp byte 7: alive counter 0→15.
- Bài nâng cao: `SpeedChecksum` = XOR của 14 nibble ở byte 0–6 và nibble counter (4 bit thấp byte 7).

</details>
