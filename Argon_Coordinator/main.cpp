/***
Program Name: main.cpp
Purpose : To demonstrate electric forklift charging coordination using distributed networking.
Description : In warehouses , there may be many battery powered forklifts, but limited charging
                outlets are available, this project demonstrates a noble way to share charger(resources}
                based on battery SOC(State of Charge). This Part of code serves as a coordinator with charger(resource)
                to cater the requesting nodes on a local network based on MQTT protocol.
                The MQTT protocol is running on ESP8266 wifi SoC(System on Chip), this code talks to ESP
                over uart and sends/receives message to the local network to other nodes which are participating.
Author: Kankan Sarkar
Modifications : 16/03/2019-- V1.0-- Initial Creation, OLED test, LED,Button Test 
                17/03/2019-- V1.1-- Integration with Wifi Module(ESP8266)
                18/03/2019-- V1.2-- Integration with MQTT Broker
                19/03/2019-- V1.2.1-- Networking test with nodes, debugging 
                21/03/2019-- V1.3-- Final version

***/

#include "mbed.h"
#include "Adafruit_SSD1306.h"
#include <iostream> 
#include <string> 
#include "rtos.h"

//-----------------------------------------------Network Specific Message----------------------------
uint8_t ID = 5; // Coordinator ID
//----------------------------------------------Global Variable--------------------------------------
char wifi_buf[200]; // buffer to store wifi messages
int index_wifi = 0; // index to track wifi_buf char count
char _recv_buf[200];
uint16_t index = 0; //index to track CSV
bool data_available = 0; // flag to check data availability.
bool update = false; // update flag for OLED
bool debounce = true; // button debounce inhibitor

//--------------------------------------------Necessary Object Spawning------------------------------

osThreadId networkThreadID; // To store Process ID -- for termination / resume / sleep etc.
Timeout hb; // Timeout Function MBED :: TIMEOUT
DigitalIn Release(PE_4); // Onboard Switch
DigitalOut myled(PA_6); // Onboard RED LED
DigitalOut myled1(PA_7); // Onboard RED LED
Serial pc(USBTX, USBRX); // Debug Uart
Serial wifi(PA_2, PA_3); //Wifi Uart 
Thread Network; //Networking Thread
typedef struct {
  uint8_t id; // id to store Remote ID
  uint16_t status; //id to store Remote SOC
}
message_t; // Queue Message Structure
MemoryPool < message_t, 32 > mpool; // Memory Pool for Queue message structure
Queue < message_t, 100 > queue; // Queue init
bool charging_Done = 0; // Charging Status

void Uart_to_Wifi();
uint64_t clock_ms();
void callback();
void disp();

//------------------------------------Node Class Starts Here------------------------------------------
// For better understanding Please refer project document.
class Node {
  private:
    uint8_t node_ID; // node id
  uint16_t error_State; // error state variable 1->Network Error,2->Charger Error,0->No error
  char buf[200]; // local buffer
  bool charging; // charging status
  uint8_t node_Charging; // Charging Node ID(Remote)
  public:
    Node(uint8_t id) {
      node_ID = id;
    }
  void set_Charging(bool status) {
    charging = status;
  }
  bool get_Charging() {
    return charging;
  }
  void set_Error(uint8_t error) {
    error_State = error;
  }
  uint8_t get_Error() {
    return error_State;
  }
  uint8_t get_nodeID() {
    return node_ID;
  }
  void set_NodeCharging(uint8_t id) { // stores which node is getting charged.
    node_Charging = id;
  }
  uint8_t get_NodeCharging() { // returns which node is getting charged.
    return node_Charging;
  }
  char * get_Status() // dashboard specific message
  {
    sprintf(buf, "%d,%d,%d", node_ID, node_Charging, error_State);
    return buf;
  }
};

class I2CPreInit: public I2C // I2C abstraction for OLED
{
  public: I2CPreInit(PinName sda, PinName scl): I2C(sda, scl) {};
};

I2CPreInit gI2C(PC_9, PA_8); // init I2C3 
Adafruit_SSD1306_I2c gOled2(gI2C, PE_8); // OLED Spawn 
Node coordinator(ID);
int main() {

  Release.mode(PullUp); // button pullup
  wifi.baud(9600); // uart communication with ESP8266 
  pc.baud(9600); // uart DEBUG
  Network.start(Uart_to_Wifi); // Start Networking Thread
  if (coordinator.get_Charging()) { //if the coordinator is already charging display the same
    gOled2.setTextCursor(0, 0);
    gOled2.printf("Status Charging");
    gOled2.setTextCursor(0, 8);
    gOled2.printf("Node ID=%d", coordinator.get_NodeCharging());
    gOled2.display();
  } else { // clear the display and display idle message if not charging
    gOled2.clearDisplay();
    gOled2.setTextCursor(0, 0);
    gOled2.printf("Status Idle\n");
    gOled2.display();

  }
  while (1) { // main loop to check button press event and release charger
    if (!Release) // button check
    {
      if (debounce == true) { // flipflop logic
        debounce = false;
        charging_Done = true;
        myled = 1; // turn RED LED on
      }
    } else { // on release
      debounce = true;
      myled = 0; // turn RED LED off
    }
    Thread::wait(50); // waiting :)
  }
}

void Uart_to_Wifi() {
  //local varaibles
  char c; // variable to store one char received from ESP8266
  char local_buf[10]; // local buffer to store ID/SOC/Destination
  uint8_t id = 0; // local variable to store remote ID
  uint8_t stat = 0; // local variable to store remote SOC
  char * token; //char array for CSV parsing
  unsigned long time_t3 = clock_ms(), time_t4 = clock_ms(); // timer variables
  while (true) {
    if (wifi.readable() == true) { // if message available
      c = wifi.getc();
      if (c == '#') {
        index = 0; // message is received fully now parse the commad
        token = strtok(_recv_buf, ",");
        id = atoi(token);
        token = strtok(NULL, ",");
        stat = atoi(token);
        if (coordinator.get_Charging()) {
          // do nothing already busy charging.
          if (id == coordinator.get_NodeCharging()) {
            wifi.printf("%d,%d,%d#", id, coordinator.get_nodeID(), coordinator.get_Charging()); // send denial to requesting node 
          }
        } else { // serve the requesting node if charger is free
          coordinator.set_NodeCharging(id);
          coordinator.set_Charging(true);
          wifi.printf("%d,%d,%d#", id, coordinator.get_nodeID(), coordinator.get_Charging()); // send charging ack to requesting node.
          gOled2.clearDisplay(); // clear OLED
          gOled2.setTextCursor(0, 0); // First Line
          gOled2.printf("Status Charging"); // Display "Status Charging"
          gOled2.setTextCursor(0, 8); // goto Next Line
          gOled2.printf("Node ID=%d", coordinator.get_NodeCharging()); // Display Node id and charging status
          gOled2.display(); // display on oled
          gOled2.clearDisplay();
          gOled2.setTextCursor(0, 0);
          gOled2.printf("Status Charging");
          gOled2.setTextCursor(0, 8);
          gOled2.printf("Node ID=%d", coordinator.get_NodeCharging());
          gOled2.display();
        }
      } else {
        //message reception is not done............ fill the buffer :(
        _recv_buf[index] = c;
        _recv_buf[index + 1] = '\0';
        index += 1;
      }
    }

    if (charging_Done) {
      //charging done
      coordinator.set_Charging(false);
      charging_Done = false;
      wifi.printf("%d,%d,%d#", coordinator.get_NodeCharging(), coordinator.get_nodeID(), coordinator.get_Charging()); // send charger release statement to remote node
      gOled2.clearDisplay(); // display idle in OLED
      gOled2.setTextCursor(0, 0);
      gOled2.printf("Status Idle\n");
      gOled2.display();
      gOled2.clearDisplay();
      gOled2.setTextCursor(0, 0);
      gOled2.printf("Status Idle\n");
      gOled2.display();
    }
  }
}
/*
Function Name: clock_ms()
Input: N/A
Base function type: User defined function.
Return: returns unsigned long
Functionality:
•   Returns system on time in milliseconds.

*/
uint64_t clock_ms() {
  return us_ticker_read() / 1000;
}
void disp() {

}
void callback() {
  charging_Done = true;
}