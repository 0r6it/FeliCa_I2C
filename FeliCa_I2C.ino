#include <Wire.h>
#include <LCD_I2C.h>
#include <RCS620S.h>

// Circuit
// RESET    - 1 RST
// Analog 5 - 2 SCL (internally pulled up)
// Analog 4 - 3 SDA (internally pulled up)
// GND      - 4 VSS
// 5V       - 5 VDD

// RCS620S
#define COMMAND_TIMEOUT               400
#define POLLING_INTERVAL              500
#define RCS620S_MAX_CARD_RESPONSE_LEN 30

// FeliCa Service/System Code
#define CYBERNE_SYSTEM_CODE           0x0003
#define COMMON_SYSTEM_CODE            0xFE00
#define PASSNET_SERVICE_CODE          0x090F
#define EDY_SERVICE_CODE              0x170F
#define NANACO_SERVICE_CODE           0x564F
#define WAON_SERVICE_CODE             0x680B

RCS620S rcs620s;

void setup(){
  LCD.begin(5);
  Serial.begin(115200);
}

void loop(){
  uint32_t balance;
  uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
  
  rcs620s.timeout = COMMAND_TIMEOUT;
  
  // サイバネ領域
  if(rcs620s.polling(CYBERNE_SYSTEM_CODE)){
    // Suica PASMO
    if(requestService(PASSNET_SERVICE_CODE)){
      if(readEncryption(PASSNET_SERVICE_CODE, 0, buf)){
        // Little Endianで入っているPASSNETの残高を取り出す
        balance = buf[23];                  // 11 byte目
        balance = (balance << 8) + buf[22]; // 10 byte目
        // 残高表示
        printBalanceLCD("PASSNET", &balance);
      }
    }
  }
  
  // 共通領域
  else if(rcs620s.polling(COMMON_SYSTEM_CODE)){
    // Edy
    if(requestService(EDY_SERVICE_CODE)){
      if(readEncryption(EDY_SERVICE_CODE, 0, buf)){
        // Big Endianで入っているEdyの残高を取り出す
        balance = buf[26];                  // 14 byte目
        balance = (balance << 8) + buf[27]; // 15 byte目
        // 残高表示
        printBalanceLCD("Edy", &balance);
      }
    }
    
    // nanaco
    if(requestService(NANACO_SERVICE_CODE)){
      if(readEncryption(NANACO_SERVICE_CODE, 0, buf)){
        // Big Endianで入っているNanacoの残高を取り出す
        balance = buf[17];                  // 5 byte目
        balance = (balance << 8) + buf[18]; // 6 byte目
        balance = (balance << 8) + buf[19]; // 7 byte目
        balance = (balance << 8) + buf[20]; // 8 byte目
        // 残高表示
        printBalanceLCD("nanaco", &balance);
      }
    }
    
    // waon
    if(requestService(WAON_SERVICE_CODE)){
      if(readEncryption(WAON_SERVICE_CODE, 1, buf)){
        // Big Endianで入っているWaonの残高を取り出す
        balance = buf[17];                  // 21 byte目
        balance = (balance << 8) + buf[18]; // 22 byte目
        balance = (balance << 8) + buf[19]; // 23 byte目
        balance = balance & 0x7FFFE0;       // 残高18bit分のみ論理積で取り出す
        balance = balance >> 5;             // 5bit分ビットシフト
        // 残高表示
        printBalanceLCD("waon", &balance);
      }
    }
  }
  
  // デフォルト表示
  else{
    LCD.clear();
    LCD.move(0);
    LCD.print("Touch");
    LCD.move(0x44);
    LCD.print("Card");
  }
  
  rcs620s.rfOff();
  delay(POLLING_INTERVAL);
}

// request service
int requestService(uint16_t serviceCode){
  int ret;
  uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
  uint8_t responseLen = 0;
  
  buf[0] = 0x02;
  memcpy(buf + 1, rcs620s.idm, 8);
  buf[9] = 0x01;
  buf[10] = (uint8_t)((serviceCode >> 0) & 0xff);
  buf[11] = (uint8_t)((serviceCode >> 8) & 0xff);

  ret = rcs620s.cardCommand(buf, 12, buf, &responseLen);
  
  if(!ret || (responseLen != 12) || (buf[0] != 0x03) ||
      (memcmp(buf + 1, rcs620s.idm, 8) != 0) || ((buf[10] == 0xff) && (buf[11] == 0xff))) {
    return 0;
  }

  return 1;
}

int readEncryption(uint16_t serviceCode, uint8_t blockNumber, uint8_t *buf){
  int ret;
  uint8_t responseLen = 0;
  
  buf[0] = 0x06;
  memcpy(buf + 1, rcs620s.idm, 8);
  buf[9] = 0x01; // サービス数
  buf[10] = (uint8_t)((serviceCode >> 0) & 0xff);
  buf[11] = (uint8_t)((serviceCode >> 8) & 0xff);
  buf[12] = 0x01; // ブロック数
  buf[13] = 0x80;
  buf[14] = blockNumber;

  ret = rcs620s.cardCommand(buf, 15, buf, &responseLen);

  if (!ret || (responseLen != 28) || (buf[0] != 0x07) ||
      (memcmp(buf + 1, rcs620s.idm, 8) != 0)) {
    return 0;
  }

  return 1;
}

void printBalanceLCD(char *card_name, uint32_t *balance){
  char result[8];
  sprintf(result, "%u", *balance);
  LCD.clear();
  LCD.move(0);
  LCD.print(card_name);
  LCD.move(0x40);
  LCD.print(0xE6);
  LCD.print(0x20);
  LCD.print(result);
  return;
}
