//
//  ZF8HP TCU 
//		trendsetter
//  	dave strum
//		2/8/2021
//
//  for esp32 board
//
// target mac ESP Board MAC Address:  24:6F:28:88:03:24

#include <esp_now.h>
#include "WiFi.h"


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


uint8_t broadcastAddress[] = {0x24, 0x6F, 0x28, 0x88, 0x03, 0x24};

#define		ASOL		23
#define		BSOL		22
#define		CSOL		21
#define		DSOL		19
#define		ESOL		18

#define		TCCSOL		24
#define		LPSOL		25
#define		PLSOL		26
#define		PARKLOCK	27

#define		VSSA		1
#define		VSSB		1

#define		PARKA		1
#define		PARKB		1

#define		SOLFREQ 	64
#define		SOLRES		8

#define		LPSCHAN		1
#define		TCCCHAN		2
#define		PLCHAN		3
#define		BSOLCHAN	4


// ------ main program stuff ----
#define DELAYSLOW	100  // 10hz
#define DELAYFAST	10		// 100hz

#define SERIAL_EN                //comment out if you don't want any debug output

#ifdef SERIAL_EN
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

int recvFlag = 0;
int curGear = 1;  // 0=in park/pawl applied
int lastGear = 0;
int nextGear = 0;
int curTemp = 0;
int curVSS = 0;


String success;
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

// Create a struct_message called BME280Readings to hold sensor readings
struct_message TCUcommand;
// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  DEBUGln("");
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
	memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
	recvFlag = 1;  // data receive flag
	//Serial.println("rec");
	//Serial.println(incomingReadings.command);
}

void processPacket() {
	if (recvFlag == 0) return;
	recvFlag = 0;
	DEBUG("received:");
	DEBUGln(incomingReadings.seq);
	DEBUG("com:");
	DEBUGln(incomingReadings.command);
	for (int i=0;i<8;i++) {
		if (incomingReadings.seq == lastPacks[i]) { return; } // see if we received this packet id in the last 8 packets
	}
	DEBUGln(" passed seq test");
	seq = incomingReadings.seq;
	if (packIndex++ > 7) packIndex = 0;		// sorta like a circular buffer?
	lastPacks[packIndex] = incomingReadings.seq;
	switch (incomingReadings.command) {
		case REVERSE_COM:		// go to reverse
			
			break;
			
		case SHIFTUP_COM:		// upshift
			if (curGear > 7) break; // already in 8th gear
			if (curGear < 1) break; //	in park
			switch (curGear) {
				case 1:
					shift12();
					break;
				case 2:
					shift23();
					break;
				case 3:
					shift34();
					break;
				case 4:
					shift45();
					break;
				case 5:
					shift56();
					break;
				case 6:
					shift67();
					break;
				case 7:
					shift78();
					break;
				default:
					break;
			}					
			break;
			
		case SHIFTDOWN_COM:		// down shift
			if (curGear < 1) break;	// in park
			if (curGear > 8) break; // in reverse
			switch (curGear) {
				case 2:
					shift21();
					break;
				case 3:
					shift32();
					break;
				case 4:
					shift43();
					break;
				case 5:
					shift54();
					break;
				case 6:
					shift65();
					break;
				case 7:
					shift76();
					break;
				case 8:
					shift87();
					break;
				default:
					break;	
			}
			break;
			
		default:
			break;
	}
	TCUcommand.seq = seq++;TCUcommand.seq = seq++;
	TCUcommand.gear = curGear;
	TCUcommand.temp = curTemp;
	TCUcommand.vss = curVSS;
	sendMsg();	
	DEBUG("gear:");
	DEBUGln(TCUcommand.gear);
}

void sendMsg() {
   // Send message via ESP-NOW
  	for (int i=0;i<2;i++) {   // try sending 3 times
		Serial.print("sending:");
		Serial.print(TCUcommand.seq);
		Serial.print(" try:");
		Serial.println(i);
		esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &TCUcommand, sizeof(TCUcommand));
		if (result == ESP_OK) { break; }
	}
}

uint32_t now = 0, prevMillisSlow=0, prevMillisFast=0;

void setup() {
	Serial.begin(115200);
	Serial.println("tcu begin");
	
	pinMode(ASOL, OUTPUT);
	
	ledcSetup(BSOLCHAN, SOLFREQ, SOLRES);
	ledcAttachPin(BSOL, BSOLCHAN);
	
	pinMode(CSOL, OUTPUT);
	pinMode(DSOL, OUTPUT);
	pinMode(ESOL, OUTPUT);
	
	ledcSetup(TCCCHAN, SOLFREQ, SOLRES);
	ledcAttachPin(TCCSOL, TCCCHAN);
	
	ledcSetup(LPSCHAN, SOLFREQ, SOLRES);
	ledcAttachPin(LPSOL, LPSCHAN);
	
	pinMode(PLSOL, OUTPUT);
	pinMode(PARKLOCK, OUTPUT);
	
	pinMode(PARKA, INPUT);
	pinMode(PARKB, INPUT);
	pinMode(VSSA, INPUT);
	pinMode(VSSB, INPUT);
	
	  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error init ESP-NOW");
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
    Serial.println("Failed to add peer");
    return;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  
	
	// add safety here:
	// check if we are moving in case we get a reboot while driving
	// read speed or we can also read the park position sensor
	
	//set solenoids for park
	digitalWrite(ASOL, HIGH);
	ledcWrite(BSOLCHAN, 0);
	digitalWrite(CSOL, HIGH);
	digitalWrite(DSOL, HIGH);
	digitalWrite(ESOL, HIGH);
	//ledcWrite(TCCCHAN, 0);
	//ledcWrite(LPSCHAN, 0);
	//digitalWrite(PLSOL, LOW);
	//digitalWrite(PARKLOCK, LOW);

}


void loop() {
	processPacket();
	
	if (millis() - prevMillisSlow >= 10000) {  // 10 hz loop
		Serial.println("sending periodic update");
		DEBUG("gear: ");
		DEBUGln(TCUcommand.gear);
		TCUcommand.seq = seq++;
		TCUcommand.gear = curGear;
		TCUcommand.temp = curTemp++;
		TCUcommand.vss = curVSS++;
		DEBUG("gear: ");
		DEBUGln(TCUcommand.gear);
		sendMsg();
		
		prevMillisSlow = millis();
	}
	
}


void shiftp1() {
	// park to drive (1st gear)
	digitalWrite(PLSOL, HIGH);	// release park pawl
	// check that pawl released (or time delay?)
	ledcWrite(BSOLCHAN, 0); // release B
	digitalWrite(CSOL,LOW);  // apply C
}

void shift12() {
	digitalWrite(CSOL, HIGH); // release C
	delay(50);
	digitalWrite(ESOL, LOW); // apply E
	curGear = 2;
}

void shift23() {
	digitalWrite(ASOL, LOW);  // release A
	digitalWrite(CSOL, LOW);  // apply C
	curGear = 3;
}

void shift34() {
	digitalWrite(CSOL, HIGH);  // release C
	delay(50);
	digitalWrite(DSOL, LOW);  // apply D
	curGear = 4;
}

void shift45() {
	digitalWrite(CSOL, LOW);  // apply C
	delay(50);
	digitalWrite(ESOL, HIGH); // release E
	curGear = 5;
}

void shift56() {
	digitalWrite(BSOL, LOW);  // release B
	digitalWrite(ESOL, LOW);  // Apply E
	curGear = 6;
}

void shift67() {
	digitalWrite(ASOL, HIGH);	// apply A
	digitalWrite(ESOL, HIGH);	// release E
	curGear = 7;
}

void shift78() {
	digitalWrite(CSOL, HIGH);	// release C
	digitalWrite(ESOL, LOW);	//	apply E	
	curGear = 8;
}


void shift21() {
	digitalWrite(CSOL, LOW); // apply C
	delay(50);
	digitalWrite(ESOL, HIGH); // release E
	curGear = 1;
}

void shift32() {
	digitalWrite(ASOL, HIGH);  // apply A
	digitalWrite(CSOL, HIGH);  // release C
	curGear = 2;
}

void shift43() {
	digitalWrite(CSOL, LOW);  // apply C
	delay(50);
	digitalWrite(DSOL, HIGH);  // release D
	curGear = 3;
}

void shift54() {
	digitalWrite(CSOL, HIGH);  // release C
	delay(50);
	digitalWrite(ESOL, LOW); // apply E
	curGear = 4;
}

void shift65() {
	digitalWrite(BSOL, HIGH);  // apply B
	digitalWrite(ESOL, HIGH);  // release E
	curGear = 5;
}

void shift76() {
	digitalWrite(ASOL, LOW);	// release A
	digitalWrite(ESOL, LOW);	// apply E
	curGear = 6;
}

void shift87() {
	digitalWrite(CSOL, LOW);	// apply C
	digitalWrite(ESOL, HIGH);	//	release E	
	curGear = 7;
}




