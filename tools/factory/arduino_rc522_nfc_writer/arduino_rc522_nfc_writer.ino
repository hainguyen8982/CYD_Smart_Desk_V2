/*
 * FabX SmartDesk — Arduino + MFRC522 NFC NTAG21x Writer
 * -------------------------------------------------------------------
 * Đấu nối chuẩn (Arduino Uno / Nano <-> RC522):
 *   SDA (SS) -> D10
 *   SCK      -> D13
 *   MOSI     -> D11
 *   MISO     -> D12
 *   RST      -> D9
 *   3.3V     -> 3.3V  (KHÔNG cắm 5V!)
 *   GND      -> GND
 *
 * Thư viện cần cài trong Arduino IDE:
 *   - "MFRC522" by GithubCommunity
 *
 * Giao thức Serial (115200 baud, kết thúc bằng \n):
 *   PING                              -> PONG:SMARTDESK_NFC_WRITER
 *   WRITE:https://smartdesk.fabx.vn/d/7350F8
 *                                     -> WAIT:TAP_TAG
 *                                     -> OK:UID=04A1B2C3D4E580:URL=...
 *                                     (hoặc ERR:<lý do>)
 *   READ                              -> WAIT:TAP_TAG -> OK:UID=...:RAW=...
 */

#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN   10
#define RST_PIN   9
#define LED_PIN  LED_BUILTIN

MFRC522 mfrc522(SS_PIN, RST_PIN);

static String lineBuf = "";

void blinkLed(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

String getUidHex() {
  String s = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) s += "0";
    s += String(mfrc522.uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

// Tạo payload TLV + NDEF URI chuẩn NFC Forum Type 2 (NTAG213 / 215 / 216)
int buildNdefUriTlv(const String& url, byte* outBuf, int maxLen) {
  byte prefixCode = 0x00;
  String stripped = url;
  if (url.startsWith("https://www.")) {
    prefixCode = 0x02;
    stripped = url.substring(12);
  } else if (url.startsWith("http://www.")) {
    prefixCode = 0x01;
    stripped = url.substring(11);
  } else if (url.startsWith("https://")) {
    prefixCode = 0x04;
    stripped = url.substring(8);
  } else if (url.startsWith("http://")) {
    prefixCode = 0x03;
    stripped = url.substring(7);
  }

  int payloadLen = 1 + stripped.length(); // 1 byte prefix + URI string
  int ndefLen = 4 + payloadLen;           // D1 01 <payloadLen> 55 <prefix> <uri>
  int totalLen = 2 + ndefLen + 1;         // 03 <ndefLen> ... FE
  if (payloadLen > 240 || totalLen > maxLen) return -1;

  int idx = 0;
  outBuf[idx++] = 0x03;                   // TLV Type: NDEF Message
  outBuf[idx++] = (byte)ndefLen;          // TLV Length
  outBuf[idx++] = 0xD1;                   // NDEF Header: MB=1, ME=1, SR=1, TNF=1 (Well-Known)
  outBuf[idx++] = 0x01;                   // Type Length = 1
  outBuf[idx++] = (byte)payloadLen;       // Payload Length
  outBuf[idx++] = 0x55;                   // Type = 'U' (URI)
  outBuf[idx++] = prefixCode;             // URI Identifier Code
  for (unsigned int i = 0; i < stripped.length(); i++) {
    outBuf[idx++] = (byte)stripped[i];
  }
  outBuf[idx++] = 0xFE;                   // TLV Terminator

  // Pad đủ bội số 4 byte (1 page NTAG = 4 bytes)
  while (idx % 4 != 0 && idx < maxLen) {
    outBuf[idx++] = 0x00;
  }
  return idx;
}

bool waitForTag(unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "CANCEL") {
        Serial.println("ERR:CANCELLED");
        return false;
      }
    }
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      return true;
    }
    delay(30);
  }
  Serial.println("ERR:TIMEOUT_NO_TAG");
  return false;
}

void handleWrite(const String& url) {
  byte tlv[144];
  int tlvLen = buildNdefUriTlv(url, tlv, sizeof(tlv));
  if (tlvLen < 0) {
    Serial.println("ERR:URL_TOO_LONG");
    return;
  }

  Serial.println("WAIT:TAP_TAG");
  if (!waitForTag(15000)) return;

  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  if (piccType != MFRC522::PICC_TYPE_MIFARE_UL) {
    Serial.println("ERR:NOT_NTAG_OR_ULTRALIGHT");
    mfrc522.PICC_HaltA();
    return;
  }

  String uid = getUidHex();
  int numPages = tlvLen / 4;

  // Ghi từng page (4 byte) bắt đầu từ page 4 (vùng User Memory NTAG213/215/216)
  for (int p = 0; p < numPages; p++) {
    byte pageAddr = 4 + p;
    MFRC522::StatusCode status = mfrc522.MIFARE_Ultralight_Write(pageAddr, &tlv[p * 4], 4);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("ERR:WRITE_FAIL_PAGE_");
      Serial.println(pageAddr);
      mfrc522.PICC_HaltA();
      return;
    }
  }

  // Đọc kiểm tra lại (Verify)
  for (int p = 0; p < numPages; p += 4) {
    byte readBuf[18];
    byte bufSize = sizeof(readBuf);
    MFRC522::StatusCode status = mfrc522.MIFARE_Read(4 + p, readBuf, &bufSize);
    if (status != MFRC522::STATUS_OK) {
      Serial.println("ERR:VERIFY_READ_FAIL");
      mfrc522.PICC_HaltA();
      return;
    }
    int checkBytes = min(16, tlvLen - p * 4);
    for (int i = 0; i < checkBytes; i++) {
      if (readBuf[i] != tlv[p * 4 + i]) {
        Serial.println("ERR:VERIFY_MISMATCH");
        mfrc522.PICC_HaltA();
        return;
      }
    }
  }

  mfrc522.PICC_HaltA();
  blinkLed(2, 80);
  Serial.print("OK:UID=");
  Serial.print(uid);
  Serial.print(":URL=");
  Serial.println(url);
}

void handleRead() {
  Serial.println("WAIT:TAP_TAG");
  if (!waitForTag(15000)) return;

  String uid = getUidHex();
  byte buf[18];
  byte sz = sizeof(buf);
  String raw = "";
  for (byte page = 4; page < 20; page += 4) {
    sz = sizeof(buf);
    if (mfrc522.MIFARE_Read(page, buf, &sz) == MFRC522::STATUS_OK) {
      for (int i = 0; i < 16; i++) {
        if (buf[i] >= 32 && buf[i] <= 126) raw += (char)buf[i];
      }
    }
  }
  mfrc522.PICC_HaltA();
  blinkLed(1, 100);
  Serial.print("OK:UID=");
  Serial.print(uid);
  Serial.print(":RAW=");
  Serial.println(raw);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  delay(100);
  blinkLed(2, 60);
  Serial.println("READY:SMARTDESK_NFC_WRITER");
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf.trim();
      if (lineBuf == "PING") {
        Serial.println("PONG:SMARTDESK_NFC_WRITER");
      } else if (lineBuf.startsWith("WRITE:")) {
        handleWrite(lineBuf.substring(6));
      } else if (lineBuf == "READ") {
        handleRead();
      } else if (lineBuf.length() > 0) {
        Serial.println("ERR:UNKNOWN_CMD");
      }
      lineBuf = "";
    } else if (lineBuf.length() < 200) {
      lineBuf += c;
    }
  }
}
