/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/ttgo-lora32-sx1276-arduino-ide/
*********/

//Libraries for LoRa
#include <SPI.h>
#include <LoRa.h>
#include "soc/rtc_wdt.h"

//Libraries for OLED Display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

long frequency = 434000000;  // LoRa Frequency in Hz
long bandwidth = 125000;  //Hz
int sf = 7;
int sf1 = sf;
const int cr = 5;
int txpwr = 20;
const int preamble = 35;

uint32_t ctr = 1;
uint32_t serial = 2624711;
uint32_t to = 0;


//xtea stuff
uint8_t buff[255];
uint32_t syskey[4] = {0x01020304,0x05060708,0x090a0b0c,0x0d0e0f00};
uint32_t dekey[4];
uint32_t enkey[4];
uint32_t v0,v1;
int len = 0;
uint32_t msgctr = 0;
bool scan = false;
bool encryption = true;
static unsigned long previousMillis = 0;
unsigned long currentMillis = millis();

int state = 0;

//OLED pins
#define OLED_SDA 4
#define OLED_SCL 15 
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

//String LoRaData;

void setup() { 

  rtc_wdt_protect_off();
  rtc_wdt_disable();
  
  //reset OLED display via software
  pinMode(OLED_RST, OUTPUT);
  digitalWrite(OLED_RST, LOW);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  
  //initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("LORA RECEIVER ");
  display.display();
  
  //initialize Serial Monitor
  Serial.begin(115200);

  Serial.println("LoRa Receiver Test");
  
  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(frequency)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bandwidth);
  LoRa.setCodingRate4(cr);
  LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setPreambleLength(preamble);

  //LoRa.onTxDone(onTxDone);
  LoRa_rxMode();

  //dekey setup
  v0=serial;
  v1=0x66666666;
  encrypt(syskey);
  dekey[0] = v0;
  dekey[1] = v1;
  v0=serial;
  v1=0x22222222;
  encrypt(syskey);
  dekey[2] = v0;
  dekey[3] = v1;

   Serial.println(F("LoRa-Node init succeeded."));
  Serial.println();
  Serial.print(F("Frequency: "));
  Serial.print((float)frequency/1000000, 3);
  Serial.println(F(" MHz"));
  Serial.print(F("TX-Power: "));
  Serial.print(txpwr);
  Serial.println(F(" dbm"));
  Serial.print(F("Bandwidth: "));
  Serial.print(bandwidth/1000);
  Serial.println(F(" kHz"));
  Serial.print(F("Spreading: "));
  Serial.println(sf);
  Serial.print(F("Coding: 4/"));
  Serial.println(cr);
  Serial.println();
  
  display.setCursor(0,10);
  display.println("LoRa Initializing OK!");
  display.display();  


}

void loop() {
  int rc;
  char bell=0x07;
  int msgsize = 4;
  int freqsize = 0;
  int freq1 = 0; // freq MHZ part (3 digits)
  int freq2 = 0; // freq. 100 kHz part (4 digits)
  state = 0;

  while (state >= 0) {
  char c;
  //try to parse packet
  //int packetSize = LoRa.parsePacket();

while (!Serial.available()) {
  if(scan){
    if(runEvery(5000)){
      LoRa.setSpreadingFactor(sf1);
      Serial.print(F("Timeout...Spreading back to "));
      Serial.println(sf1);
      sf = sf1;
      scan = false;
    }
  }
  if (LoRa.parsePacket()) {
    receive();
  }
  };



  c=Serial.read();

  // break out on ESC
  if (c == 0x1b) {
    state=-999;
    break;
  };

  if (state == 0) {
    // state 0: wait for command
    // P = PAGE
    // F = FREQUENCY
        
    if ((c == 'p') || (c == 'P')) {
      // ok, got our "p" -> go to state 1
      state=1;
      
      // echo back char
      Serial.write(c);
    } else if (c == 'E') {
      encryption = !encryption;
      if(encryption){
        Serial.println(F("Encryption on."));
      } else {
        Serial.println(F("Encryption off."));
      }
      // echo back char
      Serial.write(c);
    } else if (c == 'S') {
      state = 40;
      Serial.println(F("Enter Spreading Menu..."));
      Serial.println(F("+ / - for SF calibration / enter for save"));
    } else if (c == 'R') {
      Serial.println(F("Executing RESET..."));
      Serial.flush();
      ESP.restart(); 
    }  else if (c == 'T') {
      state = 30;
      Serial.println(F("Entering Power selection..."));
    } else if (c == 'F') {
      state = 20;
      Serial.println(F("Entering Frequency Menu..."));
    } else {
      // error: echo "bell"
      Serial.write(bell);
    }; // end else - if

    // get next char
    continue;
  }; // end state 0

    if (state == 1) {
    if (c == ' ') {
      // space -> go to state 2 and get next char
      state=2;

      // echo back char
      Serial.write(c);

      // get next char
      continue;
    } else if ((c >= '0') && (c <= '9')) {
      // digit -> first digit of address. Go to state 2 and process
      state=2;

      // continue to state 2 without fetching next char
    } else {
      // error: echo "bell"
      Serial.write(bell);

      // get next char
      continue;
    }; // end else - if
  };  // end state 1

    // state 2: address ("0" to "9")
  if (state == 2) {
    
    if ((c >= '0') && (c <= '9')) {

      to = to * 10 + (c - '0');

      if (to <= 0x1FFFFFFF) {
        Serial.write(c);
      } else {
        // address to high. Send "beep"
        Serial.write(bell);
      }; // end else - if

    } else if(c== 'A'){ //unencrypted broadcast
        to = 0xFFFFFFFF;
        Serial.write(c);
    } else if(c== 'E'){ //Echo-Test
        to = serial;
        Serial.write(c);
    } else if (c == ' ') {
      // received space, go to next field (address source)
      Serial.write(c);
      state=3;
    } else {
      // error: echo "bell"
      Serial.write(bell);
    }; // end else - elsif - if

    // get next char
    continue;
  }; // end state 2

    // state 3: message, up to 255 chars, terminate with cr (0x0d) or lf (0x0a)
  if (state == 3) {
    // accepted is everything between space (ascii 0x20) and ~ (ascii 0x7e)
    if ((c >= 0x20) && (c <= 0x7e)) {
      // accept up to 255 chars
      if (msgsize < 255) {
        Serial.write(c);

        buff[msgsize]=c;
        msgsize++;
      } else {
        // to long
        Serial.write(bell);
      }; // end else - if
                        
    } else if ((c == 0x0a) || (c == 0x0d)) {
      // done
   
      Serial.println(F(""));
                        
      // add terminating NULL
      //textmsg[msgsize]=0x00;

      // break out of loop
      len = msgsize;
      state=-1;
      break;

    } else {
      // invalid char
      Serial.write(bell);
    }; // end else - elsif - if

    // get next char
    continue;
  }; // end state 4;

  if(state == 20){
    if(c == '+'){
      frequency += bandwidth;
      Serial.print(F("Frequency: "));
      Serial.print((float)frequency/1000000, 3);
      Serial.println(F(" MHz"));
      continue;
    } else if(c == '-'){
      frequency -= bandwidth;
      Serial.print(F("Frequency: "));
      Serial.print((float)frequency/1000000, 3);
      Serial.println(F(" MHz"));
      continue;
    } else if ((c == 0x0a) || (c == 0x0d)) {
      Serial.print(F("Set Frequency to: "));
      Serial.print((float)frequency/1000000, 3);
      Serial.println(F(" MHz"));
      LoRa.begin(frequency);
      LoRa.setSpreadingFactor(sf);
      LoRa.setSignalBandwidth(bandwidth);
      LoRa.setCodingRate4(cr);
      LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
      LoRa.setPreambleLength(preamble);
      state = 0;
      break;
    } else {
      // unknown char
      Serial.write(bell);
      continue;
    };
};
  

  if(state == 30) {
      if (c == '0') {
        Serial.println(F("1dbm"));
        txpwr = 1;
        continue;
      } else if(c == '1') {
        Serial.println(F("2dbm"));
        txpwr = 2;
        continue;
      } else if(c == '2') {
        Serial.println(F("5dbm"));
        txpwr = 5;
        continue;
      } else if(c == '3') {
        Serial.println(F("8dbm"));
        txpwr = 8;
        continue;
      } else if(c == '4') {
        Serial.println(F("11dbm"));
        txpwr = 11;
        continue;
      } else if(c == '5') {
        Serial.println(F("14dbm"));
        txpwr = 14;
        continue;
      } else if(c == '6') {
        Serial.println(F("18dbm"));
        txpwr = 18;
        continue;
      } else if(c == '7') {
        Serial.println(F("20dbm"));
        txpwr = 20;
        continue;
    } else if((c == 0x0a) || (c == 0x0d)) {
        LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
        //EEPROM.put(30,pwr);
        Serial.println(F("Setting TX-PWR..."));
        state = 0;
        break;
    } else {
        Serial.write(bell);
        continue;
    };
    //RFM_OFF;
  };

  if(state == 40){
    if(c == '+'){
      sf++;
      if(sf >= 12) sf = 12;
      Serial.print(F("Spreading: "));
      Serial.println(sf);
      continue;
    } else if(c == '-'){
      sf--;
      if(sf <= 7) sf = 7;
      Serial.print(F("Spreading: "));
      Serial.println(sf);
      continue;
    } else if ((c == 0x0a) || (c == 0x0d)) {
      LoRa.setSpreadingFactor(sf);
      Serial.print(F("Set new SF to: "));
      Serial.println(sf);
      sf1 = sf;
      state = 0;
      break;
    } else {
      // unknown char
      Serial.write(bell);
      continue;
    };
};


}; //end state 0 

if(state ==-1){
  memcpy(&buff[0],&msgctr,4);
if(to != 0xFFFFFFFF){ //if broadcast => do not encrypt
  v0=to;
  v1=0x66666666;
  encrypt(syskey);
  enkey[0] = v0;
  enkey[1] = v1;
  v0=to;
  v1=0x22222222;
  encrypt(syskey);
  enkey[2] = v0;
  enkey[3] = v1;
  
  if(encryption){
    enc(len);  
  }
}
  LoRa_sendMessage();
  memset(buff,0,255);
  Serial.println();
  msgctr++;
  to = 0;
}

}

void LoRa_sendMessage(void) {
  LoRa_txMode();                        // set tx mode
  LoRa.beginPacket();                   // start packet
  LoRa.write((serial >> 24)& 0xff);
  LoRa.write((serial >> 16)& 0xff);
  LoRa.write((serial >> 8)& 0xff);
  LoRa.write((serial)& 0xff);
  LoRa.write((to >> 24)& 0xff);
  LoRa.write((to >> 16)& 0xff);
  LoRa.write((to >> 8)& 0xff);
  LoRa.write(to & 0xff);
    for(int i = 0;i<8*(len/8+1);i++){
      LoRa.write(buff[i]);
    }
  LoRa.endPacket(false);                 // finish packet and send it
  
  Serial.println(F("TxDone"));
  LoRa_rxMode();
}

void receive(){
  String message = "";
  uint32_t von = 0;
  uint32_t an = 0;
  uint32_t cc = 0;
  len = 0;
    for(int i=0;i<4;i++){
      von <<= 8;
      von |= LoRa.read();
    }
    for(int i=0;i<4;i++){
      an <<= 8;
      an |= LoRa.read();
    }

  Serial.print(F("From: 0x"));
  Serial.println(von,HEX);
  Serial.print(F("To: 0x"));
  Serial.println(an,HEX);
  Serial.print(F("Node Receive: "));

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("from: ");
  display.print(von,HEX);
  //display.setCursor(0,10);
  //display.print("to: ");
  //display.print(an,HEX);
  display.setCursor(0,10);

  while (LoRa.available()) {
  buff[len] = LoRa.read();
  
  if(buff[len] < 0x10){
    Serial.print("0");
  }
  Serial.print(buff[len],HEX);
  len++;
  }
  Serial.println("");
if((an == serial)||(an == 0xFFFFFFFF)){ //if message is for me or a broadcast
  if((von != 0xFFFFFFFF)&&(an != 0xFFFFFFFF)){ //only decrypt if it is for me
    if(encryption){ // and encryption is active
      dec(len-1);  
    }
  }
  if(von == 0xFFFFFFFF){ //if it is from a gateway => do a sf-scan and set timeout
    scan = true;
    sf--;
    LoRa.setSpreadingFactor(sf);
    previousMillis = millis();
  }

  
  memcpy(&cc,&buff[0],4);
  Serial.print(F("Frame-Counter: "));
  Serial.println(cc);
  
    for(int i = 4;i<len;i++){
      Serial.print(char(buff[i]));
      display.print(char(buff[i]));  
    }
  Serial.println();
  } else {
    Serial.println(F("Message is not for me"));
    display.print("Message is not for me");  
  }
  display.println();
  Serial.println("Received RSSI: " + String(LoRa.packetRssi()) + " / SNR: " + String(LoRa.packetSnr()));
  display.setCursor(0,55);
  display.print("RSSI: " + String(LoRa.packetRssi()) + " SNR: " + String(LoRa.packetSnr()));
  memset(buff,0,255);
  display.display();
}

void encrypt(uint32_t *inkey){
  uint32_t delta = 0x9E3779B9,sum = 0;
  for (int i = 0; i < 32;i++)
  {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + inkey[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5))+v0) ^ (sum + inkey[(sum>>11) & 3]);
  }
}


void decrypt(uint32_t *inkey){
  uint32_t delta = 0x9E3779B9,sum = 32*delta;
      for (int i=0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + inkey[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + inkey[sum & 3]);
    }
}



void enc(int in){
  int ctr = 0;
  uint32_t cbc0,cbc1;
  v0 = v1 = cbc0 = cbc1= 0;
    for(int j=-1;j<((in/8));j++){
    memcpy(&v0,&buff[ctr*8+0],4);
    memcpy(&v1,&buff[ctr*8+4],4);
    v0 ^= cbc0;
    v1 ^= cbc1;
    encrypt(enkey);
    memcpy(&buff[ctr*8+0],&v0,4);
    memcpy(&buff[ctr*8+4],&v1,4);
    cbc0 = v0;
    cbc1 = v1;
    ctr++;
    }
  }

void dec(int in){
  int ctr = 0;
  uint32_t cbc0,cbc1,cbcn0,cbcn1;
  v0 = v1 = cbc0 = cbc1 = cbcn0 = cbcn1 = 0;
    for(int j=-1;j<((in/8));j++){
    memcpy(&v0,&buff[ctr*8+0],4);
    memcpy(&v1,&buff[ctr*8+4],4);
    cbc0 = cbcn0;
    cbc1 = cbcn1;
    cbcn0 = v0;
    cbcn1 = v1;
    decrypt(dekey);
    v0 ^= cbc0;
    v1 ^= cbc1;    
    memcpy(&buff[ctr*8+0],&v0,4);
    memcpy(&buff[ctr*8+4],&v1,4);
    ctr++;
    }
  }

void LoRa_rxMode(){
  LoRa.enableInvertIQ();                // active invert I and Q signals
  LoRa.receive();                       // set receive mode
}

void LoRa_txMode(){
  LoRa.idle();                          // set standby mode
  LoRa.disableInvertIQ();               // normal mode
}


boolean runEvery(unsigned long interval)
{
  //previousMillis = 0;
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
