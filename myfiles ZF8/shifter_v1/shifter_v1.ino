//
//  ZF8HP wireless shifter
//		trendsetter
//  	dave strum
//		2/8/2021
//
//  for esp32
//
//  target mac 24:0A:C4:5E:F9:1C
//
//
//setCpuFrequencyMhz(40);
//
//    esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
//    esp_light_sleep_start();
//

// ----- commands
#define		SHIFTDOWN_COM	1
#define		SHIFTUP_COM		2
#define		REVERSE_COM		3
#define		POWEROFF_COM	4
#define		WAKE_COM		5
// -----------------
// ----- States
#define		BOOT			0
#define		PARK			1
#define		FORWARD			2
#define		REVERSE			3
#define		SLEEP			4
// ------------------

#define		UPSHIFT		17
#define		DOWNSHIFT	23

#define SERIAL_EN
#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#include <esp_now.h>
#include "WiFi.h"

#include <U8g2lib.h>

// the OLED used
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R1, /* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);

uint8_t broadcastAddress[] = {0x24, 0x0A, 0xC4, 0x5E, 0xF9, 0x1C};

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
	uint16_t seq;
    int temp;
    int gear;
    int vss;
	int vssb;
	int command;
} struct_message;

uint16_t seq;
uint16_t lastPacks[8];
int packIndex=0;

int stateFlag = 0;
int newInfoRec = 0;


struct_message TCUcommand;
struct_message lastReadings;
struct_message incomingReadings;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

int recFlag = 0;
// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  if (incomingReadings.command == WAKE_COM) stateFlag =  BOOT;
  recFlag = 1;
  //Serial.print("Bytes received: ");
  //Serial.println(len);
  //oled.drawStr(0, 1, "Gear: ");
  //oled.setCursor(0, 2);
  //u8x8.setFont(u8x8_font_inb46_4x8_n);
  //u8x8.print(incomingReadings.gear);
}
 
void initScreen() {

	u8g2.begin();
	//u8g2.enableUTF8Print();
	u8g2.clearBuffer();
	//delay(100);
	u8g2.setFont(u8g2_font_logisoso92_tn);
	u8g2.clearBuffer();
	u8g2.setCursor(5, 92);
	u8g2.print(0);
	u8g2.setFont(u8g2_font_profont15_tf);
	u8g2.setCursor(0, 107);
	u8g2.print("temp: ");
	u8g2.print(incomingReadings.temp);
	u8g2.setCursor(0, 122);
	u8g2.print("VSS: ");
	u8g2.print(incomingReadings.vss);
	u8g2.sendBuffer();
}

void setup()
{
  // Init Serial Monitor
  Serial.begin(115200);
  
  pinMode(UPSHIFT, INPUT_PULLUP);
  pinMode(DOWNSHIFT, INPUT_PULLUP);

  u8g2.setPowerSave(1);
  
  //u8g2.updateDisplay();
  //oled.drawStr(0, 0, "ZF 8HP TCU");
    //oled.drawStr(0, 1, "Gear: ");
	//oled.setFont(u8g2_font_6x10_tf);
	//oled.setCursor(6, 1);
  //oled.print(incomingReadings.gear);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    //oled.drawStr(0, 0, "Error init ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    u8g2.drawStr(0, 0, "Failed to add peer");
    return;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
  //Serial.println(len);
  //oled.drawStr(0, 1, "Gear: ");
  //oled.setCursor(0, 2);
  //oled.setFont(u8g2_font_ncenB14_tr);
  //oled.print(4);
  initScreen();
  stateFlag = FORWARD; //PARK;
}

void doReceive() {
	//recFlag = 0;
	if (recFlag == 0) return;
	DEBUG("Received:");
	DEBUGln(incomingReadings.seq);
	DEBUG("gear:");
	DEBUGln(incomingReadings.gear);
	recFlag = 0;
	for (int i=0;i<8;i++) {
		//DEBUG(i);DEBUG(":");DEBUGln(lastPacks[i]);
		if (incomingReadings.seq == lastPacks[i]) {
			DEBUGln("failed seq test:");return; 
			} // see if we received this packet id in the last 8 packets
	}
	DEBUG(" passed seq test:");
	DEBUGln(incomingReadings.seq);
	seq = incomingReadings.seq;
	if (packIndex++ > 7) packIndex = 0;		// sorta like a circular buffer?
	lastPacks[packIndex] = incomingReadings.seq;
	updateScreen();
	
}

void updateLittleStuff(int clearFirst=1) {
	if (clearFirst > 0) {
		u8g2.setDrawColor(0);
		u8g2.drawBox(0, 93, u8g2.getDisplayWidth()-1, u8g2.getDisplayHeight()-1);
		u8g2.setDrawColor(1);
	}	
	u8g2.setFont(u8g2_font_profont15_tf);

	u8g2.setCursor(0, 107);
	u8g2.print("temp: ");
	u8g2.print(incomingReadings.temp);
	u8g2.setCursor(0, 122);
	u8g2.print("VSS: ");
	u8g2.print(incomingReadings.vss);
	u8g2.sendBuffer();
}

void updateScreen() {
	if (lastReadings.gear != incomingReadings.gear) {
		u8g2.clearBuffer();
		u8g2.setFont(u8g2_font_logisoso92_tn);
		u8g2.setCursor(5, 92);
		u8g2.print(incomingReadings.gear);
		lastReadings.gear = incomingReadings.gear;
		updateLittleStuff(0);
	} else { updateLittleStuff(1); }
	
}



void sendMsg() {
   // Send message via ESP-NOW
	for (int i=0;i<2;i++) {   // try sending 3 times
		TCUcommand.seq = seq++;
		DEBUG("sending:");
		DEBUG(TCUcommand.seq);
		DEBUG(" try:");
		DEBUGln(i);
		esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &TCUcommand, sizeof(TCUcommand));
		if (result == ESP_OK) { break; }
	}
}

int butPress = 0;
int butDown = 0;
uint32_t now = 0, prevMillisSlow=0, prevMillisFast=0;
uint32_t prevMillisSleep = 0;
int sleepCounter = 0;

void loop() {
	doReceive();
	switch (stateFlag) {
		case BOOT:
			setCpuFrequencyMhz(240);
			break;
		case PARK:
			break;
		case FORWARD:
			if (millis() - prevMillisFast >= 1) {  // 100 hz loop
				if (digitalRead(UPSHIFT) && digitalRead(DOWNSHIFT)) { butPress = 0; butDown = 0;}
				if (!digitalRead(UPSHIFT) || !digitalRead(DOWNSHIFT)) { 
					if (butPress++ > 200) butPress = 200;				
				}
				
				if (!digitalRead(UPSHIFT) && digitalRead(DOWNSHIFT) && (butPress > 8) && (butDown < 1)) {
						TCUcommand.command = SHIFTUP_COM;
						sendMsg();
						butDown = 1;
				}
				if (!digitalRead(DOWNSHIFT) && digitalRead(UPSHIFT) && (butPress > 8) && (butDown < 1)) {
						TCUcommand.command = SHIFTDOWN_COM;
						sendMsg();
						butDown = 1;
				}
				if (!digitalRead(DOWNSHIFT) && digitalRead(UPSHIFT) && (butPress > 190) && (butDown < 2)) {
					DEBUGln("");DEBUGln("*****reverse");DEBUGln("");
					TCUcommand.command = REVERSE_COM;
					sendMsg();
					butDown = 2;
				}
				prevMillisFast = millis();
			}
			
			if (millis() - prevMillisSlow >= 100) {  // 10 hz loop
				updateScreen();
				prevMillisSlow = millis();
			}
			break;
		case REVERSE:
			break;
		case SLEEP:
			setCpuFrequencyMhz(80);
			if (millis() - prevMillisSleep >= 10) {  // 10 hz loop
				if (sleepCounter++ > 10) {
					sleepCounter=0;
					esp_sleep_enable_timer_wakeup(100000);
					esp_light_sleep_start();
				}
				prevMillisSleep = millis();
			}
			
			break;
	}

}