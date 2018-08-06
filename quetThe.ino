/**
File: quetThe.ino
@version: 20180805
*/

// MARK: - Mot so cai dat tuy chinh duoc.

// Thoi gian delay truoc khi xoa het du lieu trong EEPROM.
// Default: 10000 (10s)
#define DELAY_TRUOC_KHI_RESET 10000

// Toc do giao tiep voi Serial.
// Default: 115200
#define SERIAL_BAUDRATE 115200

// Toc do servo di chuyen (don vi millisecond).
// Default: 5
#define SERVO_MOVE_SPEED 5

// Goc lon nhat de mo Servo.
// Khi mo cua servo se chay tu goc 0 cho toi goc lon nhat.
// Default: chua test.
#define SERVO_MAX_OPEN 150

// Toi da so the co the luu vao EEPROM.
// Cong thuc tinh ((So byte cua EEPROM) / 4) - 1.
// Vd cho Arduino Mega: ((4096 byte) / 4) - 1 = 1023.
// Default: 50
#define MAX_CARD 50

// Pin nay se HIGH cung luc khi cua mo.
#define NEW_PIN 3

// Pin MFRC522
#define RFID_RST_PIN 9
#define RFID_MISO_PIN 50
#define RFID_MOSI_PIN 51
#define RFID_SCK_PIN 52
#define RFID_SDA_PIN 53

// Pin LED
#define LED1_PIN 4
#define LED2_PIN 2
#define LED3_PIN 5

// Pin Chuong bao
#define ALARM_PIN 8

// Pin nut cua
#define DOOR_BTN_PIN 7
// Pin nut khan cap
#define EMERGENCY_BTN_PIN 10
// Pin nut xoa du lieu
#define CLEAR_MEM_BTN_PIN 11
// Pin nut them the
#define ADD_CARD_BTN_PIN 12
// Pin bam chuong
#define ALARM_ACTIVATE_BTN_PIN 13

// Pin Servo
#define SERVO_PIN 6

// MARK: - Includes.

#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <Servo.h>
#include <MFRC522.h>
#include "pitch.h"

MFRC522 mfrc522(RFID_SDA_PIN, RFID_RST_PIN);
Servo servo;

/** Vi tri servo. */
int servoPosition;
/** Danh sach the trong bo nho dem. */
byte cardList[MAX_CARD][4];
/** The hien tai trong bo nho dem. */
byte currentCard[4];

// MARK: - Arduino Life Cycle.

void setup() {
  Wire.begin();
  Serial.begin(SERIAL_BAUDRATE);

  SPI.begin();
  mfrc522.PCD_Init();


  // Chinh servo ve vi tri ban dau.
  servo.attach(SERVO_PIN);
  for (servoPosition = 50; servoPosition >= 0; servoPosition--) {
    servo.write(servoPosition);
    delay(SERVO_MOVE_SPEED);
  }
  servo.detach();

  setupPins();
  // Tat het den LED
  setAllLEDsToLow();

  // Kiem tra xem nut xoa du lieu co LOW khong.
  // Neu co thi vao che do reset EEPROM.
  if (digitalRead(CLEAR_MEM_BTN_PIN) == LOW) {
    enterResetMode();
  }

  // Truy xem co bao nhieu the dc luu trong EEPROM.
  // Neu khong co the nao thi vao che do them the.
  if (getEEPROMCardCount() == 0) {
    Serial.println("CHU Y: Khong co the duoc luu trong EEPROM.");
    runErrorLEDSequence();
    enterAddCardMode();
  }

  // Load the tu EEPROM sang bo nho dem.
  loadCardListFromEEPROM();
}

void loop() {
  Serial.println("Vui long scan the...");
  bool detected = false;
  do {
    // Kiem tra xem co nut nao dang nhan.
    // Neu co thi thuc hien chuc nang cua nut do.
    // Neu khong the scan the.
    checkButtonsInput();
    detected = scanCard();
  } while (!detected);

  // Khi phat hien duoc the thi xem the co trong danh sach khong.
  if (isCurrentCardPresentInList()) {
    onCardAccept();
  } else {
    onCardDeny();
  }
}

/** Setup cac pin. */
void setupPins() {
  // Outputs.
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(LED3_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(NEW_PIN, OUTPUT);
  // Inputs.
  pinMode(DOOR_BTN_PIN, INPUT_PULLUP);
  pinMode(EMERGENCY_BTN_PIN, INPUT_PULLUP);
  pinMode(CLEAR_MEM_BTN_PIN, INPUT_PULLUP);
  pinMode(ADD_CARD_BTN_PIN, INPUT_PULLUP);
  pinMode(ALARM_ACTIVATE_BTN_PIN, INPUT_PULLUP);
}

// MARK: - Operating Modes.

/**
Chuyen vao che do reset.
Thiet bi se mo den LED va delay 1 khoang
thoi gian theo DELAY_TRUOC_KHI_RESET roi
xoa het du lieu trong EEPROM.
*/
void enterResetMode() {
  Serial.println("----- RESET MODE -----");
  Serial.print("CHU Y: du lieu trong EEPROM bi xoa sau ");
  Serial.print(DELAY_TRUOC_KHI_RESET / 1000);
  Serial.println("s.");
  Serial.println("Tat nguon may truoc khi den LED tat de huy qua trinh xoa.");
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  delay(DELAY_TRUOC_KHI_RESET);
  setAllLEDsToLow();
  Serial.println("Dang xoa du lieu EEPROM.");
  for (unsigned int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
    Serial.print(".");
  }
  Serial.println("Xong");
  runSuccessLEDSequence();
}

/**
Chuyen vao che do them the moi.
Thiet bi se scan the va luu vao
EEPROM.
*/
void enterAddCardMode() {
  Serial.println("------ ADD CARD MODE ------");
  if (getEEPROMCardCount() >= MAX_CARD) {
    runErrorLEDSequence();
    Serial.println("EEPROM het cho luu them the moi.");
    Serial.print("Toi da: ");
    Serial.println(MAX_CARD);
    return;
  }
  digitalWrite(LED3_PIN, HIGH);
  Serial.println("Scan the de them vao EEPROM.");
  bool detected = false;
  do {
    detected = scanCard();
  } while (!detected);
  saveCurrentCardToEEPROM();
}

// MARK: - LED Sequence Functions.

/** Tat het tat ca den LED. */
void setAllLEDsToLow() {
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  digitalWrite(LED3_PIN, LOW);
}

/** Bat het tat ca den LED. */
void setAllLEDsToHIGH() {
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, HIGH);
  digitalWrite(LED3_PIN, HIGH);
}

/** Bat den LED va chuong de thong bao thanh cong. */
void runSuccessLEDSequence() {
  setAllLEDsToLow();
  delay(3);
  digitalWrite(LED2_PIN, HIGH);
  for (int i = 1; i <= 2; i++) {
    digitalWrite(ALARM_PIN, HIGH);
    delay(250);
    digitalWrite(ALARM_PIN, LOW);
  }
  digitalWrite(LED2_PIN, LOW);
}

/** Bat den LED va chuong de thong bao that bai. */
void runErrorLEDSequence() {
  setAllLEDsToLow();
  delay(3);
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(ALARM_PIN, HIGH);
  delay(1000);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(ALARM_PIN, LOW);
}

// MARK: - EEPROM Functions.

/**
Truy xem co bao nhieu the duoc luu trong EEPROM.
@return so the duoc luu.
*/
byte getEEPROMCardCount() {
  return EEPROM.read(0);
}

/** Tang so luong the duoc luu them 1. */
void incrementEEPROMCardCount() {
  EEPROM.write(0, getEEPROMCardCount() + 1);
}

/** Luu the hien tai vao EEPROM. */
void saveCurrentCardToEEPROM() {
  Serial.print("Dang luu the vao EEPROM.");
  int startAddress = getEEPROMCardCount() * 4 + 1;
  int endAddress = startAddress + 4;
  int index = 0;
  for (int address = startAddress; address < endAddress; address++) {
    EEPROM.write(address, currentCard[index++]);
    Serial.print(".");
  }
  incrementEEPROMCardCount();
  Serial.println("Xong");
  runSuccessLEDSequence();
}

/**
Load the tu EEPROM vao bo nho dem, dong
thoi in the ma so the ra Serial.
*/
void loadCardListFromEEPROM() {
  int cardCount = getEEPROMCardCount();
  Serial.println("----- DANH SACH THE ------");
  for (int cardID = 0; cardID < cardCount; cardID++) {
    Serial.print("THE ");
    Serial.print(cardID + 1);
    Serial.print(": ");
    for (int i = 0; i < 4; i++) {
      byte value = EEPROM.read(cardID * 4 + (i + 1));
      cardList[cardID][i] = value;
      Serial.print(value, HEX);
    }
    Serial.println("");
  }
}

/**
Kiem tra xem the hien tai co nam trong danh sach.
@return true neu the hien tai co nam trong danh sach.
*/
bool isCurrentCardPresentInList() {
  for (int i = 0; i < getEEPROMCardCount(); i++) {
    byte b0 = cardList[i][0];
    byte b1 = cardList[i][1];
    byte b2 = cardList[i][2];
    byte b3 = cardList[i][3];

    if (b0 == currentCard[0] &&
        b1 == currentCard[1] &&
        b2 == currentCard[2] &&
        b3 == currentCard[3]) {
      return true;
    }

  }
  return false;
}

// MARK: - MFRC522 Functions.

/**
Scan the.
@return true neu MFRC522 do duoc ma so the.
*/
bool scanCard() {
  setAllLEDsToLow();
  digitalWrite(LED3_PIN, HIGH);
  if (!mfrc522.PICC_IsNewCardPresent()) {
    Serial.println("No card present");
    return false; 
  }
  if (!mfrc522.PICC_ReadCardSerial()) { 
    Serial.println("Cannot read");
    return false; 
  }
  Serial.print("The da scan: ");
  for (size_t i = 0; i < 4; i++) {
    currentCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(currentCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  runSuccessLEDSequence();
  return true;
}

// MARK: - Buttons Checking.

/**
Kiem tra xem nut khan cap, nut them the va nut chuong co LOW khong.
Kiem tra xem cua co dang dong khong.
*/
void checkButtonsInput() {
  // Neu nut khan cap LOW thi mo cua.
  if (digitalRead(EMERGENCY_BTN_PIN) == LOW) {
    Serial.println("----- EMERGENCY MODE ------");
    onCardAccept();
  }

  // Neu nut cua dang HIGH thi bat chuong bao den khi
  // nao dong cua thi thoi.
  secureDoor();

  // Neu nut them the LOW thi vao che do them the.
  if (digitalRead(ADD_CARD_BTN_PIN) == LOW) {
    setAllLEDsToHIGH();
    enterAddCardMode();
    loadCardListFromEEPROM();
  }

  // Neu nut chuong LOW thi choi nhac nhung khong mo cua.
  if (digitalRead(ALARM_ACTIVATE_BTN_PIN) == LOW) {
    playOpeningTunes();
  }
  
}

/** 
Neu nut cua dang HIGH thi bat chuong bao den khi
nao dong cua thi thoi.
*/
void secureDoor() {
  int doorStatus = digitalRead(DOOR_BTN_PIN);
  while (doorStatus == HIGH) {
    Serial.println("Chua dong cua.");
    setAllLEDsToLow();
    digitalWrite(LED1_PIN, HIGH);
    digitalWrite(ALARM_PIN, HIGH);
    delay(250);
    digitalWrite(ALARM_PIN, LOW);
    doorStatus = digitalRead(DOOR_BTN_PIN);
  }
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED3_PIN, HIGH);
}

// MARK: - Door open and close behaviour.

/** 
Thuc hien qua tring mo cua. 
NEW_PIN se duoc bat HIGH trong qua trinh mo cua.
*/
void onCardAccept() {
  Serial.println("The duoc chap nhan.");
  setAllLEDsToLow();
  digitalWrite(LED2_PIN, HIGH);

  digitalWrite(NEW_PIN, HIGH);
  servo.attach(SERVO_PIN);
  Serial.print("Dang mo cua.");
  for (servoPosition = 0; servoPosition <= SERVO_MAX_OPEN; servoPosition++) {
    servo.write(servoPosition);
    Serial.print(".");
    delay(SERVO_MOVE_SPEED);
  }
  Serial.println("Xong");

  playOpeningTunes();

  Serial.print("Dang dong cua.");
  for (servoPosition = SERVO_MAX_OPEN; servoPosition >= 0; servoPosition--) {
    servo.write(servoPosition);
    Serial.print(".");
    delay(SERVO_MOVE_SPEED);
  }
  Serial.println("Xong");
  servo.detach();
  digitalWrite(NEW_PIN, LOW);
  setAllLEDsToHIGH();
}

/** Thuc hien qua trinh tu choi the khong mo cua. */
void onCardDeny() {
  Serial.println("The bi tu choi.");
  setAllLEDsToLow();
  digitalWrite(LED1_PIN, HIGH);
  playDenyTunes();
}

// MARK: - Tunes

/** Nhac luc mo cua. */
void playOpeningTunes() {
  int melody[] = {
    NOTE_C3, NOTE_E3, NOTE_G3, NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5
  };
  int duration[] = {
    4, 4, 4, 4, 4, 4, 4
  };

  for (int note = 0; note < 7; note++) {
    int noteDuration = 1000 / duration[note];
    tone(ALARM_PIN, melody[note], noteDuration);
    delay(noteDuration * 1.30);
    noTone(ALARM_PIN);
  }
}

/** Nhac luc tu choi the khong mo cua. */
void playDenyTunes() {
  int melody[] = {
    NOTE_A3, NOTE_GS3, NOTE_A3, NOTE_GS3, NOTE_A3, NOTE_DS3, NOTE_D3
  };
  int duration[] = {
    4, 8, 4, 8, 4, 4, 4
  };

  for (int note = 0; note < 7; note++) {
    int noteDuration = 1000 / duration[note];
    tone(ALARM_PIN, melody[note], noteDuration);
    delay(noteDuration * 1.30);
    noTone(ALARM_PIN);
  }
}

