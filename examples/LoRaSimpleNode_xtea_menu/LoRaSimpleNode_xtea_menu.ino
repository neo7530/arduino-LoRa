/*
  LoRa Simple Gateway/Node Exemple

  This code uses InvertIQ function to create a simple Gateway/Node logic.

  Gateway - Sends messages with enableInvertIQ()
          - Receives messages with disableInvertIQ()

  Node    - Sends messages with disableInvertIQ()
          - Receives messages with enableInvertIQ()

  With this arrangement a Gateway never receive messages from another Gateway
  and a Node never receive message from another Node.
  Only Gateway to Node and vice versa.

  This code receives messages and sends a message every second.

  InvertIQ function basically invert the LoRa I and Q signals.

  See the Semtech datasheet, http://www.semtech.com/images/datasheet/sx1276.pdf
  for more on InvertIQ register 0x33.

  created 05 August 2018
  by Luiz H. Cassettari
*/

#include <SPI.h>              // include libraries
#include <LoRa.h>

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



const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin

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

void setup() {
  Serial.begin(57600);                   // initialize serial
  while (!Serial);


  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(frequency)) {
    Serial.println(F("LoRa init failed. Check your connections."));
    while (true);                       // if failed, do nothing
  }

  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bandwidth);
  LoRa.setCodingRate4(cr);
  LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setPreambleLength(preamble);
  //LoRa.setTxPower(14,PA_OUTPUT_RFO_PIN); //RA-02 Module
  //LoRa.enableCrc();

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

  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
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
    
    delay(20);
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
      asm volatile ("  jmp 0"); 
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
  LoRa_sendMessage();
  memset(buff,0,255);
  Serial.println();
  msgctr++;
  to = 0;
}



} //end loop


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
  LoRa.endPacket(true);                 // finish packet and send it
}

void onReceive(int packetSize) {
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

  while (LoRa.available()) {
  buff[len] = LoRa.read();
  Serial.print(buff[len],HEX);
  len++;
  }
  Serial.println("");
  if(von != 0xFFFFFFFF){
    if(encryption){
      dec(len-1);  
    }
  }
  if(von == 0xFFFFFFFF){
    scan = true;
    sf--;
    LoRa.setSpreadingFactor(sf);
    previousMillis = millis();
  }

  if((an == serial)||(an == 0xFFFFFFFF)){
  memcpy(&cc,&buff[0],4);
  Serial.print(F("Frame-Counter: "));
  Serial.println(cc);
  
    for(int i = 4;i<len;i++){
      Serial.print(char(buff[i]));
    }
  Serial.println();
  } else {
    Serial.println(F("Message is not for me"));  
  }
  Serial.println("Received RSSI: " + String(LoRa.packetRssi()) + " / SNR: " + String(LoRa.packetSnr()));
  memset(buff,0,255);
}

void onTxDone() {
  Serial.println(F("TxDone"));
  LoRa_rxMode();
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
