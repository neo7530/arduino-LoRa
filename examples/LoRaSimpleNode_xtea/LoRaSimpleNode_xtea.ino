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

const long frequency = 434000000;  // LoRa Frequency in Hz
const long bandwidth = 125000;  //Hz
const int sf = 7;
const int cr = 5;
const int txpwr = 20;
const int preamble = 35;

uint32_t ctr = 1;
uint32_t serial = 0x10000001;
uint32_t to = 0x10000002;



const int csPin = 10;          // LoRa radio chip select
const int resetPin = 9;        // LoRa radio reset
const int irqPin = 2;          // change for your board; must be a hardware interrupt pin

//xtea stuff
uint8_t buff[255];
uint32_t key[4] = {0x01020304,0x05060708,0x090a0b0c,0x0d0e0f00};
uint32_t v0,v1;
int len = 0;
uint32_t msgctr = 0;


void setup() {
  Serial.begin(9600);                   // initialize serial
  while (!Serial);


  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(frequency)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  LoRa.setSpreadingFactor(sf);
  LoRa.setSignalBandwidth(bandwidth);
  LoRa.setCodingRate4(cr);
  LoRa.setTxPower(txpwr,PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setPreambleLength(preamble);
  //LoRa.setTxPower(14,PA_OUTPUT_RFO_PIN); //RA-02 Module
  //LoRa.enableCrc();



  Serial.println("LoRa init succeeded.");
  Serial.println();
  Serial.println("LoRa Simple Node");
  Serial.println("Only receive messages from gateways");
  Serial.println("Tx: invertIQ disable");
  Serial.println("Rx: invertIQ enable");
  Serial.println();

  LoRa.onReceive(onReceive);
  LoRa.onTxDone(onTxDone);
  LoRa_rxMode();
}

void loop() {
  if(Serial.available()>0){
    String message = "";
    memcpy(&buff[0],&msgctr,4);
    message = Serial.readStringUntil('\n');
    for(int i=4;i<message.length()+4;i++){
      buff[i] = message[i-4];
      Serial.print(buff[i],HEX);
      }
    Serial.println("");
    len = message.length()+3;
    Serial.print("Send Message...");
    Serial.println(message);
    enc(len);
    LoRa_sendMessage(message);
    memset(buff,0,255);
    Serial.println("");
    msgctr++;
  }
}


void encrypt(){
  uint32_t delta = 0x9E3779B9,sum = 0;
  for (int i = 0; i < 32;i++)
  {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5))+v0) ^ (sum + key[(sum>>11) & 3]);
  }
}


void decrypt(){
  uint32_t delta = 0x9E3779B9,sum = 32*delta;
      for (int i=0; i < 32; i++) {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
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
    encrypt();
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
    decrypt();
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

void LoRa_sendMessage(String message) {
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

  Serial.print("From: 0x");
  Serial.println(von,HEX);
  Serial.print("To: 0x");
  Serial.println(an,HEX);
  Serial.print("Node Receive: ");

  while (LoRa.available()) {
  buff[len] = LoRa.read();
  Serial.print(buff[len],HEX);
  len++;
  }
  Serial.println("");
  dec(len-1);
  memcpy(&cc,&buff[0],4);
  Serial.print("Frame-Counter: ");
  Serial.println(cc);
  
    for(int i = 4;i<8*(len/8);i++){
      Serial.print(char(buff[i]));
    }
  Serial.println();  
  Serial.println("Received RSSI: " + String(LoRa.packetRssi()) + " / SNR: " + String(LoRa.packetSnr()));
  memset(buff,0,255);
}

void onTxDone() {
  Serial.println("TxDone");
  LoRa_rxMode();
}

boolean runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
