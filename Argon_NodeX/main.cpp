/***
Program Name: main.cpp
Purpose : To demonstrate electric forklift charging coordination using distributed networking.
Description : In warehouses , there may be many battery powered forklifts, but limited charging
                outlets are available, this project demonstrates a noble way to share charger(resources}
                based on battery SOC(State of Charge). This Part of code serves as a telemetry 
                module to share data with the local network based on MQTT protocol.
                The MQTT protocol is running on ESP8266 wifi SoC(System on Chip), this code talks to ESP
                over uart and sends message to the local network to other nodes which are participating.
Author: Kankan Sarkar, Ravindra, Mohit
Modifications : 25/02/2019-- Initial Creation --Basic test of Thread,LED,Buzzer,OLED
                02/03/2019-- V1.1--Test of CAN, UART,Queue processing.
                03/03/2019-- V1.2--Network Negotiation Implementation, testing of the same using UART.
                07/03/2019-- V1.2.1--Debugging of Network Negotiation , Queue is removed to reduce latency.
                09/03/2019-- V1.3-- Integration LED,Buzzer, sensors with main thread and testing.
                10/03/2019-- V1.4-- Integration of ESP8266 and networking test. CAN Module integration test failed with thread.
                16/03/2019-- V1.5-- Overall Integration test of all sensor and output, removed CAN communication.
                17/03/2019-- V1.6-- Overall Integration test with 3 Nodes, implementation of clock_ms()
                18/03/2019-- V1.6.1-- Overall Integration test with 3 Nodes,thread synchronization, latency error removal by adding nonblocking loop
                19/03/2019-- V1.6.2-- Overall Integration test with 3 Nodes, integration with coordinator
                21/03/2019-- V1.7-- Final version

***/
#include "mbed.h"
#include <iostream> 
#include <string> 
#include "rtos.h"

//------------------------------------------Global Variables Start Here-------------------------------

//****************************************Network Specific*******************************************//

uint8_t ID = 1; // Change ID for different Nodes
uint8_t coordinator_id = 5; // Coordinator ID in the network
#define disp_id 10 // network dashboard ID
#define dash_freq 10000 //Dashboard Message sending frequency
//****************************************Network Specific Ends*******************************************//

# define max_Battery_Voltage 13600 // maximum battery voltage
# define min_Battery_Voltage 11500 // minimum battery voltage
bool data_available = 0; // flag for data availibility from wifi to UART
float temperature; // variable to store temperature
char _recv_buf[200]; // Wifi received data store buffer
bool waiting = false; // flag to check if the node is expeting any network objection
uint16_t index = 0; // counter to store network message in _recv_buf
bool charging = 0; // local flag to indicate charger acquired or not.
bool critical = 0; // flag to indicate if the charge threshold is < critical value
bool n_critical = 0; // flag to indicate charge is less than nominal value 
bool toggle = false; // flag to indicate LED toggling.
typedef struct {
  uint8_t id; // stores ID
  uint16_t status; // stores State of charge
}
message_t; // structure for transmit message from Main to Uart_to_Wifi
typedef struct {
  uint8_t id;
  uint8_t bd;
  uint16_t status;
}
message_r; // structure for receive message from Uart_to_Wifi

//-----------------------------------------Function Prototypes--------------------------------------

void Uart_to_Wifi(); // network manager fucntion
void heartbeat(); // System Health information
uint64_t clock_ms(); // Returns system time
uint16_t map(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t); // Maps one range of values to another range.
unsigned int atoi2(char * ); // alternate implementation of char to int.

//------------------------------------------Necessary Objects spawning------------------------------

osThreadId networkThreadID; // Thread ID to store network thread id
Timeout hb; // timer to blink heartbeat led
Ticker ticktick; // ticker to calculate elapsed time in network 
DigitalOut myled(PE_7); // Green LED
DigitalOut myled1(PE_8); // White LED
DigitalOut myled2(PE_9); // Red LED
DigitalOut buzzer(PE_10); // Buzzer
AnalogIn Voltage(PC_0); // Analog Potentiometer
AnalogIn temp(PC_1); //LM35 Temperature Sensor
Serial pc(PA_9, PA_10); // Debug UART
Serial wifi(PA_2, PA_3); // Uart to communicate with ESP8266
AnalogIn Current(PC_2); //Analog Potentiometer
Thread Network; //MBED:: Thread to run Uart_to_Wifi function.

MemoryPool < message_t, 32 > mpool; // TX memory allocation
MemoryPool < message_r, 32 > mpool1; // RX memory allocation
Queue < message_t, 100 > queue; // TX queue init
Queue < message_r, 100 > queue1; // RX queue init

//------------------------------------Node Class Starts Here------------------------------------------
// For better understanding Please refer project document.
class Node {
  private:
  uint8_t node_ID; // Node ID
  uint16_t battery_Voltage; //Node Battery Voltage<-Potentiometer
  uint16_t battery_Current; //Node Battery Current<-Potensiometer
  uint8_t coolant_Level; // From CAN Message -- currently disabled
  float m1_temp; // From CAN Message -- currently disabled
  float m2_temp; // From CAN Message -- currently disabled
  uint16_t vehicle_Speed; // From CAN Message -- currently disabled
  uint16_t error_State; // Stores network error, sensor error 1-> Sensor Error, 2->Network Error,3->Charger Error,0->No error
  uint8_t charging; // Node Charging State Flag
  char buf[200]; // local buffer
  uint8_t node_ACK; // node ack flag
  uint16_t battery_max_v; // maximum battery voltage
  uint16_t battery_min_v; //minimum battery voltage
  float battery_temp; // battery temperature
  uint16_t battery_Status; // battery SOC
  public:
    Node(uint8_t n_id, uint16_t max_v, uint16_t min_v) //Constructor
  {
    node_ID = n_id;
    battery_Voltage = 0;
    battery_Current = 0;
    coolant_Level = 0;
    m1_temp = 25.0;
    m2_temp = 25.0;
    vehicle_Speed = 0;
    error_State = 0;
    charging = 0;
    node_ACK = 0;
    battery_max_v = max_v;
    battery_min_v = min_v;
    battery_temp = 0;
    battery_Status = 0;
  }
  void set_charging(bool charge) {
    charging = charge;
  }
  bool get_charging() {
    return charging;
  }
  uint8_t get_nodeID() {
    return node_ID;
  }
  void set_Voltage(uint16_t voltage) {
    battery_Voltage = voltage;
  }
  uint16_t get_Voltage() {
    return battery_Voltage;
  }

  void set_Current(uint16_t current) {
    battery_Current = current;
  }

  uint16_t get_Current() {
    return battery_Current;
  }

  void set_Coolant(uint8_t coolant) {
    coolant_Level = coolant;
  }

  uint8_t get_Coolant() {
    return coolant_Level;
  }

  void set_Moto1(float temp) {
    m1_temp = temp;
  }
  float get_Motor1() {
    return m1_temp;
  }
  void set_Moto2(float temp) {
    m2_temp = temp;
  }
  float get_Motor2() {
    return m2_temp;
  }
  void set_VehicleSpeed(uint8_t speed) {
    vehicle_Speed = speed;
  }
  uint8_t get_VehicleSpeed() {
    return vehicle_Speed;
  }
  uint8_t calculate_BatteryStatus(uint16_t battery_Voltage) // calculate SOC Refer report for More insight
  {
    battery_Status = ((battery_Voltage - battery_min_v) * 100) / (battery_max_v - battery_min_v);
    return battery_Status;
  }
  uint16_t get_BatteryStatus() {
    return battery_Status;
  }
  char * get_Status() // Dashboard Specific Message
  {
    sprintf(buf, "%d,%d,%d,%d,%0.2f,%0.2f,%d,%d", node_ID, battery_Current, battery_Status, coolant_Level, m1_temp, m2_temp, error_State, charging);
    return buf;
  }
  bool remote_Objection(uint8_t id, uint16_t rbattery_Status) // function to decide to deny remote node charger acquiring
  {
    if (battery_Status < rbattery_Status) {
      node_ACK = 1;
      return node_ACK;
    } else {
      node_ACK = 0;
      return node_ACK;
    }
  }
  bool get_Node_Ack() {
    return node_ACK;
  }
  void set_Node_Ack(uint8_t ac) {
    node_ACK = ac;
  }
};

Node mynode(ID, max_Battery_Voltage, min_Battery_Voltage); // Initialization of Class Node with id,min_battery_voltage,max_battery_voltage 
int main() {
  char local_buf[10];
  // start heartbeat LED
  //Init all LED as HIGH
  myled = 1;
  myled1 = 1;
  myled2 = 1;
  buzzer = 1;
  // Init UART Communication with baud rate 9600
  pc.baud(9600);
  wifi.baud(9600);
  // Start networking thread
  Network.start(Uart_to_Wifi);
  // local variables 
  unsigned long time_t = 0, time_t1 = 0;
  time_t = clock_ms();
  time_t1 = clock_ms();

  //message_r *message1 = mpool1.alloc();
  while (1) {

    while (clock_ms() > time_t) { // timeout loop to calculate Battery SOC
      time_t = clock_ms() + 3000;
      mynode.calculate_BatteryStatus(map(Voltage.read_u16(), 0, 65535, 11500, 13600));
      mynode.set_Moto2(temp.read() * 3.685503686 * 100);
      //pc.printf("%d Node ACK=%d\n",,mynode.get_Node_Ack());
    }
    while (clock_ms() > time_t1) { // timeout loop to check if charge is less than threshold
      time_t1 = clock_ms() + 1000;
      if (mynode.get_BatteryStatus() < 15 && !charging) { // timeout loop to check if SOC is less than critical
        critical = 1; // set critical True
      } else if (mynode.get_BatteryStatus() <= 30 && !charging) { //timeout loop to check if SOC is less then nominal
        n_critical = 1; //set nominal flag
      }
    }
    Thread::wait(1); // wait for 1 ms :)
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
    if (clock_ms() > time_t4) { // dashboard message sending after some time
      time_t4 = clock_ms() + dash_freq;
      wifi.printf("$%d,%s#", disp_id, string(mynode.get_Status())); // send to dashbaord
      pc.printf("$%d,%s#", disp_id, string(mynode.get_Status())); //send to debug
    }
    if (wifi.readable() == true) {
      c = wifi.getc();
      if (c == '#') {
        index = 0;
        //wifi.printf("%s index=%d\n",_recv_buf,index);
        message_t * message = mpool.alloc();
        token = strtok(_recv_buf, ",");
        id = atoi(token);
        message -> id = id;
        token = strtok(NULL, ",");
        stat = atoi(token);
        message -> status = stat;
        //wifi.printf("Received msg id=%d,status=%d#",id,stat);
        queue.put(message);
        mpool.free(message); // send messsage to main thread
        if (mynode.get_BatteryStatus() < stat) {
          wifi.printf("%d,%d,%d#", id, ID, mynode.get_BatteryStatus()); // send objection
          pc.printf("Objecting Remote ID=>%d,my ID=>%d,my status=>%d\n", id, ID, mynode.get_BatteryStatus()); // debug message
        }
        if (id == coordinator_id) { // if message is received from coordinator.
          if (stat == 0x01) // if coordinator has accepepted charging req
          {

            myled = 1; // turn off Green LED
            buzzer = 1; // turn off buzzer
            mynode.set_charging(true);
            charging = true;
          } else {
            mynode.set_charging(false);
            charging = false;
          }
        }
        if (waiting) {
          waiting = false; // waiting is false
          mynode.remote_Objection(id, stat); // check remote objection
          pc.printf("Message Received id=%d,status=%d and ack=%d\n", id, stat, mynode.get_Node_Ack()); // debug

        }
      } else {
        // store EPS8266 information here
        _recv_buf[index] = c;
        _recv_buf[index + 1] = '\0';
        index += 1;
      }
    } else if (critical == 1) {
      // if critical send message directly to coordinator
      critical = 0;
      wifi.printf("%d,%d,%d#", coordinator_id, mynode.get_nodeID(), mynode.get_BatteryStatus()); // sending message to coordinator
      pc.printf("Coordinator get=>%d,%d,%d#\n", coordinator_id, mynode.get_nodeID(), mynode.get_BatteryStatus()); // debug
    } else if (n_critical == 1) {
      n_critical = 0;
      // if not so critical , broadcast to network for acknowledgement 
      while (clock_ms() > time_t3 && !waiting) {
        wifi.printf("255,%d,%d#", mynode.get_nodeID(), mynode.get_BatteryStatus()); // broadcasting in network
        pc.printf("BroadCast get ACK=>255,%d,%d#\n", mynode.get_nodeID(), mynode.get_BatteryStatus()); //debug
        //wifi.printf("Timeout from waiting loop\n");
        mynode.set_Node_Ack(1);
        time_t3 = clock_ms() + 7000;
        waiting = true;
        hb.attach( & heartbeat, 5.0); // start green led blinking till get the charger lock.
        myled = 0; // turn on the LED
        buzzer = 0; // turn on the LED
      }
      if (toggle) // if toggle set by heartbeat --
      {
        myled = 1;
        buzzer = 1;
        toggle = false;
        hb.detach();
        waiting = 0;
        //wifi.printf("Timeout from ack loop\n");
        if (mynode.get_Node_Ack()) {
          wifi.printf("%d,%d,%d#", coordinator_id, mynode.get_nodeID(), mynode.get_BatteryStatus());
          pc.printf("Coordinator Ack=>%d,%d,%d#\n", coordinator_id, mynode.get_nodeID(), mynode.get_BatteryStatus());
        } else {
          wifi.printf("255,%d,%d#", mynode.get_nodeID(), mynode.get_BatteryStatus());
          pc.printf("BroadCast Non Ack=>255,%d,%d#\n", mynode.get_nodeID(), mynode.get_BatteryStatus());
        }
        //wifi.printf("leaving ack\n");
      }
      Thread::wait(1);
    }
  }
}
/*
Function Name: map
Input: input value to map, input minimum value, input maximum value, output minimum value, output maximum value. All inputs values are uint16_t type.
Base function type: User Defined function
Return: uint16_t type mapped value
Functionality:
•   Re-maps a value from one range to another range.
*/

uint16_t map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
Function Name: atoi2
Input: char array pointer
Base function type: User Defined function
Return: returns uint16_t value.
Functionality:
•   Similar to atoi implementation, converts input char array to integer.
•   It has error handling to raise exception in case of null input.

*/
unsigned int atoi2(char * str) {
  if (!str)
    printf("Enter valid string");

  int number = 0;
  char * p = str;

  while (( * p >= '0') && ( * p <= '9')) {
    number = number * 10 + ( * p - '0');
    p++;
  }
  return (unsigned int) number;
}
/*
Function Name: heartbeat()
Input: N/A
Base function type: User defined function, invoked via mbed::timeout interrupt.
Return: N/A
Functionality:
•   Blinks Red LED to show System working.
*/
void heartbeat() {
  myled2 != myled2;
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
