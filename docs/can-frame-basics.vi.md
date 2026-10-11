# Một frame CAN gồm những gì, alive counter là gì, CRC là gì

Ví dụ xuyên suốt là một frame thật trong `GVRET_Log.csv` (xem [walkthrough Bài 1](savvycan-ex1-walkthrough.vi.md)): ID `0x228`, DLC 4, data `40 C0 30 F8`. Các con số bit-level dưới đây được tính bằng script, không phải vẽ minh họa.

## 1. Cấu trúc frame CAN classic (data frame, ID 11 bit)

Bus CAN chỉ có hai mức: **dominant (0)** và **recessive (1)**. Khi hai node phát cùng lúc, 0 luôn "thắng" 1. Mọi thứ bên dưới, kể cả arbitration và ACK, đều dựa vào quy tắc này.

```
 SOF | Identifier (11) | RTR | IDE | r0 | DLC (4) | Data (0..64) | CRC (15) | CRC del | ACK | ACK del | EOF (7) | IFS (3)
  0  | 010 0010 1000   |  0  |  0  | 0  |  0100   | 40 C0 30 F8  |  0x6B58  |    1    |  0  |    1    | 1111111 |  111
     |<------- Arbitration ------->|<-- Control -->|<-- Data -->|<---------------- trailer ------------------------->|
```

| Trường | Số bit | Trong frame `0x228` | Chức năng |
|---|---|---|---|
| **SOF** (Start of Frame) | 1 | `0` | Một bit dominant báo "frame bắt đầu". Mọi node dùng cạnh xuống của nó để đồng bộ cứng (hard sync) bộ đếm bit. |
| **Identifier** | 11 | `010 0010 1000` = `0x228` | Không phải "địa chỉ người nhận" mà là **tên của nội dung** (ví dụ "tốc độ bánh xe"). Node nào cần thì lọc theo ID này (chính là filter bank của bxCAN). ID còn là **độ ưu tiên**: ID nhỏ hơn có nhiều bit 0 ở đầu hơn nên thắng arbitration. |
| **RTR** | 1 | `0` | 0 = data frame, 1 = remote frame (yêu cầu node khác gửi data). Gần như không dùng trong ô tô hiện đại. |
| **IDE** | 1 | `0` | 0 = ID chuẩn 11 bit, 1 = ID mở rộng 29 bit (cột `Extended` trong CSV). |
| **r0** | 1 | `0` | Bit dự trữ, phát dominant. |
| **DLC** | 4 | `0100` = 4 | Số byte data, 0–8 (cột `LEN`). |
| **Data** | 0–64 | `40 C0 30 F8` | Payload. Các **signal** (tốc độ, nhiệt độ, counter, CRC ứng dụng…) được xếp bit vào đây theo DBC. |
| **CRC** | 15 | `0x6B58` | CRC-15 do **phần cứng CAN controller** tính trên SOF → hết Data. Chi tiết ở mục 3. |
| **CRC delimiter** | 1 | `1` | Bit recessive ngăn cách. |
| **ACK slot** | 1 | node phát gửi `1`, **node nhận ghi đè thành `0`** | Bất kỳ node nào nhận đúng frame (CRC khớp) sẽ kéo bit này xuống dominant. Node phát thấy 0 nghĩa là "có ít nhất một node nhận được". Không ai ACK thì node phát coi là lỗi và gửi lại. Đây là lý do bxCAN ở normal mode mà bus không có node nào khác thì gửi mãi không xong. |
| **ACK delimiter** | 1 | `1` | |
| **EOF** | 7 | `1111111` | Kết thúc frame. |
| **IFS** (interframe space) | 3 | `111` | Khoảng nghỉ tối thiểu trước frame tiếp theo. |

**Bit stuffing:** từ SOF đến hết CRC, sau mỗi 5 bit giống nhau liên tiếp, node phát chèn thêm 1 bit ngược lại để bộ thu không mất đồng bộ. Frame `0x228` có 51 bit trước CRC, cộng 15 bit CRC là 66 bit, và cần chèn **4 bit stuff**. Tính cả trailer và IFS, toàn bộ frame dài **83 bit**. Ở 500 kbit/s (2 µs/bit) frame này chiếm bus **166 µs**. Đây là cách tính **bus load**: cộng thời gian của mọi frame trong 1 giây, rồi chia cho 1 giây.

**Arbitration:** nhiều node có thể bắt đầu SOF cùng lúc. Mỗi node vừa phát vừa đọc lại bus từng bit của ID. Node nào phát 1 nhưng đọc về 0 thì biết mình đã thua, nên im lặng chuyển sang nhận. Node có ID nhỏ nhất đi tiếp mà frame của nó không bị hỏng. Vì vậy CAN được gọi là "non-destructive bitwise arbitration", và ID trong DBC thực chất là bảng ưu tiên của cả xe.

## 2. Alive counter (rolling counter) là gì

Alive counter là một **signal do phần mềm ECU gửi đặt vào data**, thường dài 4 bit. Nó tăng thêm 1 mỗi lần ECU gửi frame đó, đến giá trị lớn nhất thì quay về 0. Trong `0x228` đó là nibble cao của byte 2: `0x30, 0x40, 0x50 … 0xF0, 0x00, 0x10 …`.

Câu hỏi nó trả lời là: "ECU gửi có **còn sống** và dữ liệu này có **mới** không?". CAN phần cứng đảm bảo frame không bị sai bit, nhưng không biết được các lỗi ở tầng phần mềm, nên ECU nhận cần counter để phát hiện:

| Lỗi | ECU nhận thấy gì qua counter |
|---|---|
| ECU gửi treo nhưng CAN controller vẫn phát lại buffer cũ | counter **đứng yên** (`5, 5, 5…`) |
| Mất frame (bus quá tải, gateway làm rơi) | counter **nhảy cóc** (`3 → 5`) |
| Frame cũ bị gửi lặp lại | counter **lặp lại** hoặc **lùi** |
| Thứ tự frame bị đảo (qua gateway) | counter **lùi** |

Ví dụ ECU phanh nhận yêu cầu mô-men từ ECU động cơ: nếu counter đứng yên quá N chu kỳ, nó phải coi giá trị là không tin cậy và chuyển sang giá trị thay thế an toàn. Đây là yêu cầu của ISO 26262 về **freshness** của dữ liệu an toàn.

## 3. CRC là gì, và vì sao một frame có hai CRC

**CRC (Cyclic Redundancy Check)** là phép chia đa thức trên dãy bit. Phần dư của phép chia được gửi kèm; bên nhận chia lại và so sánh. Nó phát hiện rất tốt lỗi lật bit, nhất là lỗi theo cụm, và tính được rất nhanh bằng phần cứng (thanh ghi dịch + XOR). Trong ô tô có **hai tầng CRC hoàn toàn khác nhau**, và đây là chỗ hay bị nhầm:

| | **CRC-15 của giao thức CAN** | **CRC trong data (E2E)** |
|---|---|---|
| Nằm ở đâu | Trường CRC sau data, **không thấy trong log** (SavvyCAN, CSV chỉ có ID + data) | Một byte trong data, **thấy được trong log** (byte 3 của `0x228`) |
| Ai tính | Phần cứng CAN controller (bxCAN tự làm) | Phần mềm ECU (module E2E/Com, theo AUTOSAR E2E Library) |
| Bảo vệ đoạn nào | Một lần truyền trên **một đoạn dây**: SOF → Data | **Đầu-đến-cuối** (end-to-end): từ phần mềm ECU gửi đến phần mềm ECU nhận, xuyên qua gateway, bộ nhớ, driver |
| Sai thì sao | Node nhận phát **error frame**, frame bị hủy, node gửi tự gửi lại; phần mềm không bao giờ thấy frame sai | Frame vẫn tới phần mềm; E2E báo lỗi và ứng dụng tự quyết định (dùng giá trị thay thế, báo DTC) |
| Đa thức | `x¹⁵+x¹⁴+x¹⁰+x⁸+x⁷+x⁴+x³+1` (0x4599) | Thường là CRC-8 SAE J1850 (đa thức 0x1D) hoặc CRC-8H2F, tùy profile E2E |
| Frame `0x228` | `0x6B58` (tính từ bit thật) | `0xF8` |

Vì sao đã có CRC-15 rồi vẫn cần CRC trong data? Vì CRC-15 chỉ bảo vệ chặng dây đồng. Nếu lỗi xảy ra **bên trong** một ECU (RAM hỏng, gateway chép nhầm byte, phần mềm ghi đè buffer), frame lỗi được gửi đi với một CRC-15 **hoàn toàn đúng**, vì phần cứng tính CRC trên dữ liệu đã hỏng. Chỉ CRC do phần mềm gửi tính từ đầu nguồn mới bắt được lỗi này. Counter + CRC ứng dụng chính là cặp đôi của **AUTOSAR E2E protection**.

**Kiểm chứng trên log thật:** byte 3 của `0x228` đúng là **CRC-8 SAE J1850** (đa thức `0x1D`, giá trị khởi tạo `0xFF`, XOR đầu ra `0xFF`) tính trên byte 0–2. Kết quả khớp **871 trên 880** frame. Cả 9 frame không khớp đều chứa giá trị `F1` lạ mà walkthrough Bài 1 đã nghi là bộ ghi làm hỏng, và CRC vừa xác nhận điều đó. Tương tự, byte 7 của `0x0E` là CRC-8 J1850 trên byte 0–6, khớp **722 trên 725** frame. Đây là cách một reverse-engineer xác nhận "byte nhảy loạn này là CRC": thử các thuật toán chuẩn đến khi khớp gần 100 %.

```c
/* CRC-8 SAE J1850: poly 0x1D, init 0xFF, final XOR 0xFF (AUTOSAR Crc_CalculateCRC8).
 * Bitwise version for clarity; production code uses a 256-entry const table in Flash. */
#include <stdint.h>

uint8_t Crc8_SaeJ1850(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFU;
    uint8_t i;
    uint8_t bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x80U) != 0U) ? (uint8_t)((uint8_t)(crc << 1) ^ 0x1DU)
                                        : (uint8_t)(crc << 1);
        }
    }
    return (uint8_t)(crc ^ 0xFFU);
}

/* Frame 0x228 = 40 C0 30 F8: Crc8_SaeJ1850({0x40, 0xC0, 0x30}, 3) == 0xF8 */
```

Thử tay ngay: với frame tiếp theo `40 C0 40 ??`, CRC đúng phải là `0xA1`. Trong log, ô đó lại là `F1`, chính là một trong 9 frame bị bộ ghi làm hỏng.

## 4. Liên hệ với project

- `samples/demo_drive` có `SpeedCounter` + `SpeedChecksum` trong `0x100`. Đó là phiên bản đơn giản (XOR nibble) của cặp counter + CRC ở trên, và Bài 4 nâng cao trong hướng dẫn SavvyCAN yêu cầu bạn tìm ra thuật toán của nó.
- Phần mở rộng tùy chọn trong roadmap là thêm **E2E (CRC-8 J1850 + alive counter)** vào `Com` của project. Hàm ở trên chính là viên gạch đầu tiên cho phần đó.
- Trong driver bxCAN ([bài MCAL 3](exercises/ex03-can-bxcan.vi.md)), bạn **không** viết CRC-15: phần cứng làm. Bạn chỉ thấy hậu quả của nó qua `CAN_ESR.LEC` (mã lỗi cuối cùng, `110` = CRC error) và bộ đếm `TEC`/`REC`.
