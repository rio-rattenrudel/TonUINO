#include <DFMiniMp3.h>
#include <EEPROM.h>
#include <JC_Button.h>
#include <MFRC522.h>
#include <SPI.h>
#include <SoftwareSerial.h>

// DFPlayer Mini
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX
uint16_t numTracksInFolder;
uint16_t currentTrack;
uint8_t  volume = 15;
uint8_t  ledcc = 0;
uint8_t  ledinit = 1;
uint8_t  eq = 0;
uint16_t eqcc = 0;

// DEBUG
#define DEBUG_VERBOSE_LEVEL 0
#define WRITE_WITH_KEY_A    1

// LEDS
#define BLUE_LED      8
#define GREEN_LED_RX  1   // INV +5V TTL
#define YELLOW_LED_TX 0   // INV +5V TTL
#define ORANGE_LED    A4
#define AMBER_LED     A3
#define RED_LED       A5
#define VOLUME_LED    5   // PWM - ANALOG

// this object stores nfc tag data
struct nfcTagObject {
  uint32_t cookie;
  uint8_t version;
  uint8_t folder;
  uint8_t mode;
  uint8_t special;
};

nfcTagObject myCard;

static void nextTrack(uint16_t track);
int voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
              bool preview = false, int previewFromFolder = 0);

bool knownCard = false;

// implement a notification class,
// its member methods will get called
//
class Mp3Notify {
public:
  static void OnError(uint16_t errorCode) {
    // see DfMp3_Error for code meaning
    Serial.println();
    Serial.print("Com Error ");
    Serial.println(errorCode);
  }
  static void OnPlayFinished(uint16_t track) {
    Serial.print("Track beendet");
    Serial.println(track);
    delay(100);
    nextTrack(track);
  }
  static void OnCardOnline(uint16_t code) {
    Serial.println(F("SD Karte online "));
  }
  static void OnCardInserted(uint16_t code) {
    Serial.println(F("SD Karte bereit "));
  }
  static void OnCardRemoved(uint16_t code) {
    Serial.println(F("SD Karte entfernt "));
  }
  static void OnUsbOnline(uint16_t code) {
      Serial.println(F("USB online "));
  }
  static void OnUsbInserted(uint16_t code) {
      Serial.println(F("USB bereit "));
  }
  static void OnUsbRemoved(uint16_t code) {
    Serial.println(F("USB entfernt "));
  }
};

static DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(mySoftwareSerial);

// Leider kann das Modul keine Queue abspielen.
static uint16_t _lastTrackFinished;
static void nextTrack(uint16_t track) {
  if (track == _lastTrackFinished) {
    return;
   }
   _lastTrackFinished = track;
   
   if (knownCard == false)
    // Wenn eine neue Karte angelernt wird soll das Ende eines Tracks nicht
    // verarbeitet werden
    return;

  if (myCard.mode == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> keinen neuen Track spielen"));
//    mp3.sleep(); // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
  }
  if (myCard.mode == 2) {
    if (currentTrack != numTracksInFolder) {
      currentTrack = currentTrack + 1;
      mp3.playFolderTrack(myCard.folder, currentTrack);
      Serial.print(F("Albummodus ist aktiv -> nächster Track: "));
      Serial.print(currentTrack);
    } else 
//      mp3.sleep();   // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
    { }
  }
  if (myCard.mode == 3) {
    uint16_t oldTrack = currentTrack;
    currentTrack = random(1, numTracksInFolder + 1);
    if (currentTrack == oldTrack)
      currentTrack = currentTrack == numTracksInFolder ? 1 : currentTrack+1;
    Serial.print(F("Party Modus ist aktiv -> zufälligen Track spielen: "));
    Serial.println(currentTrack);
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Einzel Modus aktiv -> Strom sparen"));
//    mp3.sleep();      // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
  }
  if (myCard.mode == 5) {
    if (currentTrack != numTracksInFolder) {
      currentTrack = currentTrack + 1;
      Serial.print(F("Hörbuch Modus ist aktiv -> nächster Track und "
                     "Fortschritt speichern"));
      Serial.println(currentTrack);
      mp3.playFolderTrack(myCard.folder, currentTrack);
      // Fortschritt im EEPROM abspeichern
      EEPROM.write(myCard.folder, currentTrack);
    } else {
//      mp3.sleep();  // Je nach Modul kommt es nicht mehr zurück aus dem Sleep!
      // Fortschritt zurück setzen
      EEPROM.write(myCard.folder, 1);
    }
  }
}

static void previousTrack() {
  if (myCard.mode == 1) {
    Serial.println(F("Hörspielmodus ist aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 2) {
    Serial.println(F("Albummodus ist aktiv -> vorheriger Track"));
    if (currentTrack != 1) {
      currentTrack = currentTrack - 1;
    }
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 3) {
    Serial.println(F("Party Modus ist aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 4) {
    Serial.println(F("Einzel Modus aktiv -> Track von vorne spielen"));
    mp3.playFolderTrack(myCard.folder, currentTrack);
  }
  if (myCard.mode == 5) {
    Serial.println(F("Hörbuch Modus ist aktiv -> vorheriger Track und "
                     "Fortschritt speichern"));
    if (currentTrack != 1) {
      currentTrack = currentTrack - 1;
    }
    mp3.playFolderTrack(myCard.folder, currentTrack);
    // Fortschritt im EEPROM abspeichern
    EEPROM.write(myCard.folder, currentTrack);
  }
}

static void setVolume() {
  uint8_t val = volume;
  switch(eq) {
    case DfMp3_Eq_Pop:      if (val < 30) val++;  break;
    case DfMp3_Eq_Rock:     if (val > 0)  val--;  break;
    case DfMp3_Eq_Jazz:     if (val > 0)  val--;  break;
    case DfMp3_Eq_Bass:     if (val > 0)  val--; 
                            if (val > 0)  val--;
                            if (val > 0)  val--;
                            if (val > 0)  val--;  break;
  }
  mp3.setVolume(val);
}

static void increaseVolume() {
  uint8_t val;
  if (volume < 30) {
    ++volume;
    setVolume();
    analogWrite(VOLUME_LED, volume << 3);
  }
}

static void decreaseVolume() {
  uint8_t val;
  if (volume > 0) {
    --volume;
    setVolume();
    analogWrite(VOLUME_LED, volume << 3);
  }
}

static void increaseEQ() {
  ++eqcc;
  if (!ledinit && eqcc > 2000) {
    eqcc = 0;

    switch(eq) {
      case DfMp3_Eq_Classic:  eq = DfMp3_Eq_Normal;   break;
      case DfMp3_Eq_Normal:   eq = DfMp3_Eq_Pop;      break;
      case DfMp3_Eq_Pop:      eq = DfMp3_Eq_Rock;     break;
      case DfMp3_Eq_Rock:     eq = DfMp3_Eq_Jazz;     break;
      case DfMp3_Eq_Jazz:     eq = DfMp3_Eq_Bass;     break;
      case DfMp3_Eq_Bass:     eq = DfMp3_Eq_Classic;  break;
    }
   
    setVolume();
    mp3.setEq(eq);

    // reset led
    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED_RX, HIGH);
    digitalWrite(YELLOW_LED_TX, HIGH);
    digitalWrite(ORANGE_LED, LOW);
    digitalWrite(AMBER_LED, LOW);
    digitalWrite(RED_LED, LOW);

    // set led
    if (eq == DfMp3_Eq_Classic) digitalWrite(BLUE_LED, HIGH);
    if (eq == DfMp3_Eq_Normal) digitalWrite(GREEN_LED_RX, LOW);
    if (eq == DfMp3_Eq_Pop) digitalWrite(YELLOW_LED_TX, LOW);
    if (eq == DfMp3_Eq_Rock) digitalWrite(ORANGE_LED, HIGH);
    if (eq == DfMp3_Eq_Jazz) digitalWrite(AMBER_LED, HIGH);
    if (eq == DfMp3_Eq_Bass) digitalWrite(RED_LED, HIGH);
  }
}

static void decreaseEQ() {
  ++eqcc;
  if (!ledinit && eqcc > 2000) {
    eqcc = 0;

    switch(eq) {
      case DfMp3_Eq_Classic:  eq = DfMp3_Eq_Bass;     break;
      case DfMp3_Eq_Normal:   eq = DfMp3_Eq_Classic;  break;
      case DfMp3_Eq_Pop:      eq = DfMp3_Eq_Normal;   break;
      case DfMp3_Eq_Rock:     eq = DfMp3_Eq_Pop;      break;
      case DfMp3_Eq_Jazz:     eq = DfMp3_Eq_Rock;     break;
      case DfMp3_Eq_Bass:     eq = DfMp3_Eq_Jazz;     break;
    }

    setVolume();
    mp3.setEq(eq);

    digitalWrite(BLUE_LED, LOW);
    digitalWrite(GREEN_LED_RX, HIGH);
    digitalWrite(YELLOW_LED_TX, HIGH);
    digitalWrite(ORANGE_LED, LOW);
    digitalWrite(AMBER_LED, LOW);
    digitalWrite(RED_LED, LOW);

    if (eq == DfMp3_Eq_Classic) digitalWrite(BLUE_LED, HIGH);
    if (eq == DfMp3_Eq_Normal) digitalWrite(GREEN_LED_RX, LOW);
    if (eq == DfMp3_Eq_Pop) digitalWrite(YELLOW_LED_TX, LOW);
    if (eq == DfMp3_Eq_Rock) digitalWrite(ORANGE_LED, HIGH);
    if (eq == DfMp3_Eq_Jazz) digitalWrite(AMBER_LED, HIGH);
    if (eq == DfMp3_Eq_Bass) digitalWrite(RED_LED, HIGH);
  }
}

// MFRC522
#define RST_PIN 9                 // Configurable, see typical pin layout above
#define SS_PIN 10                 // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522
MFRC522::MIFARE_Key key;
bool successRead;
byte sector = 1;
byte blockAddr = 4;
byte trailerBlock = 7;
MFRC522::StatusCode status;

#define buttonPause A0
#define buttonUp A1
#define buttonDown A2
#define busyPin 4

#define LONG_PRESS 300

Button pauseButton(buttonPause);
Button upButton(buttonUp);
Button downButton(buttonDown);
bool ignorePauseButton = false;
bool ignoreUpButton = false;
bool ignoreDownButton = false;

uint8_t numberOfCards = 0;

bool isPlaying() { return !digitalRead(busyPin); }

#define LED_INIT_END  30

#define CLK           6   // Pin B [D1]
#define DATA          7   // Pin A [D0]

uint32_t preMS = 0;  
uint32_t waitMS = 150;

uint32_t preLEDMS = 0;
uint32_t waitLEDMS = 40;

void setup() {

  pinMode(CLK, INPUT_PULLUP);
  pinMode(DATA, INPUT_PULLUP);

  // LEDs
  pinMode(BLUE_LED, OUTPUT);
  pinMode(GREEN_LED_RX, OUTPUT);
  pinMode(YELLOW_LED_TX, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(AMBER_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(VOLUME_LED, OUTPUT);

  #if DEBUG_VERBOSE_LEVEL >= 1
  Serial.begin(115200); // Es gibt ein paar Debug Ausgaben über die serielle
                        // Schnittstelle
  #endif
                       
  randomSeed(analogRead(A0)); // Zufallsgenerator initialisieren

  Serial.println(F("TonUINO Version 2.0"));
  Serial.println(F("(c) Thorsten Voß"));

  // Knöpfe mit PullUp
  pinMode(buttonPause, INPUT_PULLUP);
  pinMode(buttonUp, INPUT_PULLUP);
  pinMode(buttonDown, INPUT_PULLUP);

  // Busy Pin
  pinMode(busyPin, INPUT);

  // DFPlayer Mini initialisieren
  mp3.begin();
  mp3.setVolume(volume);
  analogWrite(VOLUME_LED, volume << 3);

  digitalWrite(BLUE_LED, LOW);
  digitalWrite(GREEN_LED_RX, HIGH);
  digitalWrite(YELLOW_LED_TX, HIGH);
  digitalWrite(ORANGE_LED, LOW);
  digitalWrite(AMBER_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // NFC Leser initialisieren
  SPI.begin();        // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  mfrc522
      .PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // RESET --- ALLE DREI KNÖPFE BEIM STARTEN GEDRÜCKT HALTEN -> alle bekannten
  // Karten werden gelöscht
  if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW &&
      digitalRead(buttonDown) == LOW) {
    Serial.println(F("Reset -> EEPROM wird gelöscht"));
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.write(i, 0);
    }
  }

}

static uint8_t prevNextCode = 0;
static uint16_t store=0;
  

// A vald CW or  CCW move returns 1, invalid returns 0.
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode <<= 2;
  if (digitalRead(DATA)) prevNextCode |= 0x02;
  if (digitalRead(CLK)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      //if (store==0xd42b) return 1;
      //if (store==0xe817) return -1;
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

void loop() {

  static int8_t c,val;
  
  do {
    mp3.loop();
    // Buttons werden nun über JS_Button gehandelt, dadurch kann jede Taste
    // doppelt belegt werden
    pauseButton.read();
    upButton.read();
    downButton.read();

    if (pauseButton.wasReleased()) {
      if (ignorePauseButton == false) {
        if (isPlaying())
          mp3.pause();
        else {
          mp3.start();
          ledcc = LED_INIT_END; // Abort LED Init
        }
      }
      ignorePauseButton = false;
    } else if (pauseButton.pressedFor(LONG_PRESS) &&
               ignorePauseButton == false) {
      if (isPlaying())
        mp3.playAdvertisement(currentTrack);
      else {
        ledcc = LED_INIT_END; // Abort LED Init
        knownCard = false;
        mp3.playMp3FolderTrack(800);
        Serial.println(F("Karte resetten..."));
        resetCard();
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
      ignorePauseButton = true;
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      Serial.println(F("EQ Up"));
      increaseEQ();
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton)
        nextTrack(random(65536));
      else
        ignoreUpButton = false;
    }

    if (downButton.pressedFor(LONG_PRESS)) {
      Serial.println(F("EQ Down"));
      decreaseEQ();
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton)
        previousTrack();
      else
        ignoreDownButton = false;
    }
    // Ende der Buttons
    
    if( val=read_rotary() ) {
      //c +=val;
      //Serial.print(c);Serial.print(" ");
    
      if ( prevNextCode==0x0b) {
        //Serial.print("eleven ");
        //Serial.println(store,HEX);
        increaseVolume();
      }
    
      if ( prevNextCode==0x07) {
        //Serial.print("seven ");
        //Serial.println(store,HEX);
        decreaseVolume();
      }
    }

    // little timer for card check 
    // should > 30ms (~mfrc522 time), to
    // give CPU time to do other stuff
    
    uint32_t curMS = millis();  

    if (ledinit && (curMS - preLEDMS) > waitLEDMS) {
      preLEDMS = curMS;
      
      waitLEDMS += 4;

      if (ledcc == 0) digitalWrite(BLUE_LED, HIGH);
      if (ledcc == 1) digitalWrite(GREEN_LED_RX, LOW);
      if (ledcc == 2) digitalWrite(YELLOW_LED_TX, LOW);
      if (ledcc == 3) digitalWrite(ORANGE_LED, HIGH);
      if (ledcc == 4) digitalWrite(AMBER_LED, HIGH);
      if (ledcc == 5) digitalWrite(RED_LED, HIGH);

      if (ledcc == 6) digitalWrite(BLUE_LED, LOW);
      if (ledcc == 7) digitalWrite(GREEN_LED_RX, HIGH);
      if (ledcc == 8) digitalWrite(YELLOW_LED_TX, HIGH);
      if (ledcc == 9) digitalWrite(ORANGE_LED, LOW);
      if (ledcc == 10) digitalWrite(AMBER_LED, LOW);
      if (ledcc == 11) digitalWrite(RED_LED, LOW);

      if (ledcc == 17) digitalWrite(BLUE_LED, HIGH);
      if (ledcc == 16) digitalWrite(GREEN_LED_RX, LOW);
      if (ledcc == 15) digitalWrite(YELLOW_LED_TX, LOW);
      if (ledcc == 14) digitalWrite(ORANGE_LED, HIGH);
      if (ledcc == 13) digitalWrite(AMBER_LED, HIGH);
      if (ledcc == 12) digitalWrite(RED_LED, HIGH);

      if (ledcc > 17) {

        if (ledcc == 18) waitLEDMS = 40;
        
        if (ledcc % 2 == 0) {
          digitalWrite(BLUE_LED, LOW);
          digitalWrite(GREEN_LED_RX, HIGH);
          digitalWrite(YELLOW_LED_TX, HIGH);
          digitalWrite(ORANGE_LED, LOW);
          digitalWrite(AMBER_LED, LOW);
          digitalWrite(RED_LED, LOW);
        } else {
          digitalWrite(BLUE_LED, HIGH);
          digitalWrite(GREEN_LED_RX, LOW);
          digitalWrite(YELLOW_LED_TX, LOW);
          digitalWrite(ORANGE_LED, HIGH);
          digitalWrite(AMBER_LED, HIGH);
          digitalWrite(RED_LED, HIGH);
        }
      }

      if (ledcc > 25) {
        mp3.setEq(eq);
        digitalWrite(GREEN_LED_RX, LOW);
        ledinit = 0;
      }

      ledcc++;
    }

    if ((curMS - preMS) > waitMS) {
      preMS = curMS;

      // abort by new card
      if (mfrc522.PICC_IsNewCardPresent()) break;
    }
    
  } while (true);

  // RFID Karte wurde aufgelegt

  ledcc = LED_INIT_END;

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  if (readCard(&myCard) == true) {
    if (myCard.cookie == 322417479 && myCard.folder != 0 && myCard.mode != 0) {

      knownCard = true;
      numTracksInFolder = mp3.getFolderTrackCount(myCard.folder);
      Serial.print(numTracksInFolder);
      Serial.print(F(" Dateien in Ordner "));
      Serial.println(myCard.folder);

      // Hörspielmodus: eine zufällige Datei aus dem Ordner
      if (myCard.mode == 1) {
        Serial.println(F("Hörspielmodus -> zufälligen Track wiedergeben"));
        currentTrack = random(1, numTracksInFolder + 1);
      }
      
      // Album Modus: kompletten Ordner spielen
      if (myCard.mode == 2) {
        Serial.println(F("Album Modus -> kompletten Ordner wiedergeben"));
        currentTrack = 1;
      }
      
      // Party Modus: Ordner in zufälliger Reihenfolge
      if (myCard.mode == 3) {
        Serial.println(
            F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
        currentTrack = random(1, numTracksInFolder + 1);
      }
      
      // Einzel Modus: eine Datei aus dem Ordner abspielen
      if (myCard.mode == 4) {
        Serial.println(
            F("Einzel Modus -> eine Datei aus dem Odrdner abspielen"));
        currentTrack = myCard.special;
      }
      
      // Hörbuch Modus: kompletten Ordner spielen und Fortschritt merken
      if (myCard.mode == 5) {
        Serial.println(F("Hörbuch Modus -> kompletten Ordner spielen und "
                         "Fortschritt merken"));
        currentTrack = EEPROM.read(myCard.folder);
      }

      // validate track & play 
      if (myCard.mode > 0 && myCard.mode < 6) {
        if (currentTrack == 0 || currentTrack > numTracksInFolder) currentTrack = 1;
        mp3.playFolderTrack(myCard.folder, currentTrack);
      }
    }

    // Neue Karte konfigurieren
    else {
      knownCard = false;
      setupCard();
    }
  }
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

int voiceMenu(int numberOfOptions, int startMessage, int messageOffset,
              bool preview = false, int previewFromFolder = 0) {
  int returnValue = 0;
  if (startMessage != 0)
    mp3.playMp3FolderTrack(startMessage);
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();
    mp3.loop();
    if (pauseButton.wasPressed()) {
      if (returnValue != 0)
        return returnValue;
      delay(1000);
    }

    if (upButton.pressedFor(LONG_PRESS)) {
      returnValue = min(returnValue + 10, numberOfOptions);
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          mp3.playFolderTrack(returnValue, 1);
        else
          mp3.playFolderTrack(previewFromFolder, returnValue);
      }
      ignoreUpButton = true;
    } else if (upButton.wasReleased()) {
      if (!ignoreUpButton) {
        returnValue = min(returnValue + 1, numberOfOptions);
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            mp3.playFolderTrack(returnValue, 1);
          else
            mp3.playFolderTrack(previewFromFolder, returnValue);
        }
      } else
        ignoreUpButton = false;
    }
    
    if (downButton.pressedFor(LONG_PRESS)) {
      returnValue = max(returnValue - 10, 1);
      mp3.playMp3FolderTrack(messageOffset + returnValue);
      delay(1000);
      if (preview) {
        do {
          delay(10);
        } while (isPlaying());
        if (previewFromFolder == 0)
          mp3.playFolderTrack(returnValue, 1);
        else
          mp3.playFolderTrack(previewFromFolder, returnValue);
      }
      ignoreDownButton = true;
    } else if (downButton.wasReleased()) {
      if (!ignoreDownButton) {
        returnValue = max(returnValue - 1, 1);
        mp3.playMp3FolderTrack(messageOffset + returnValue);
        delay(1000);
        if (preview) {
          do {
            delay(10);
          } while (isPlaying());
          if (previewFromFolder == 0)
            mp3.playFolderTrack(returnValue, 1);
          else
            mp3.playFolderTrack(previewFromFolder, returnValue);
        }
      } else
        ignoreDownButton = false;
    }
  } while (true);
}

void resetCard() {
  do {
    pauseButton.read();
    upButton.read();
    downButton.read();

    if (upButton.wasReleased() || downButton.wasReleased()) {
      Serial.print(F("Abgebrochen!"));
      mp3.playMp3FolderTrack(802);
      return;
    }
  } while (!mfrc522.PICC_IsNewCardPresent());

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  Serial.print(F("Karte wird neu Konfiguriert!"));
  setupCard();
}

void setupCard() {
  mp3.pause();
  Serial.print(F("Neue Karte konfigurieren"));

  // Ordner abfragen
  myCard.folder = voiceMenu(99, 300, 0, true);

  // Wiedergabemodus abfragen
  myCard.mode = 5;//voiceMenu(6, 310, 310);

  // Hörbuchmodus -> Fortschritt im EEPROM auf 1 setzen
  EEPROM.write(myCard.folder,1);

  // Einzelmodus -> Datei abfragen
  if (myCard.mode == 4)
    myCard.special = voiceMenu(mp3.getFolderTrackCount(myCard.folder), 320, 0,
                               true, myCard.folder);

  // Admin Funktionen
  if (myCard.mode == 6)
    myCard.special = voiceMenu(3, 320, 320);

  // Karte ist konfiguriert -> speichern
  mp3.pause();
  writeCard(myCard);
}

bool readCard(nfcTagObject *nfcTag) {
  bool returnValue = true;
  // Show some details of the PICC (that is: the tag/card)
  Serial.print(F("Card UID:"));
  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.println();
  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  byte buffer[18];
  byte size = sizeof(buffer);

  // Authenticate using key A
  Serial.println(F("Authenticating using key A..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Show the whole sector as it currently is
  Serial.println(F("Current data in sector:"));
  mfrc522.PICC_DumpMifareClassicSectorToSerial(&(mfrc522.uid), &key, sector);
  Serial.println();

  // Read data from the block
  Serial.print(F("Reading data from block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    returnValue = false;
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
  Serial.print(F("Data in block "));
  Serial.print(blockAddr);
  Serial.println(F(":"));
  dump_byte_array(buffer, 16);
  Serial.println();
  Serial.println();

  uint32_t tempCookie;
  tempCookie = (uint32_t)buffer[0] << 24;
  tempCookie += (uint32_t)buffer[1] << 16;
  tempCookie += (uint32_t)buffer[2] << 8;
  tempCookie += (uint32_t)buffer[3];

  nfcTag->cookie = tempCookie;
  nfcTag->version = buffer[4];
  nfcTag->folder = buffer[5];
  nfcTag->mode = buffer[6];
  nfcTag->special = buffer[7];

  return returnValue;
}

void writeCard(nfcTagObject nfcTag) {
  MFRC522::PICC_Type mifareType;
  byte buffer[16] = {0x13, 0x37, 0xb3, 0x47, // 0x1337 0xb347 magic cookie to
                                             // identify our nfc tags
                     0x01,                   // version 1
                     nfcTag.folder,          // the folder picked by the user
                     nfcTag.mode,    // the playback mode picked by the user
                     nfcTag.special, // track or function for admin cards
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  byte size = sizeof(buffer);

  mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

  // Authenticate using key B
  Serial.println(F("Authenticating again using key B..."));
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    mp3.playMp3FolderTrack(401);
    return;
  }

  // Write data to the block
  Serial.print(F("Writing data into block "));
  Serial.print(blockAddr);
  Serial.println(F(" ..."));
  dump_byte_array(buffer, 16);
  Serial.println();
  status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(blockAddr, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Write() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
      mp3.playMp3FolderTrack(401);
  }
  else
    mp3.playMp3FolderTrack(400);
  Serial.println();
  delay(100);
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
