/***
Program Name: main.cpp
Purpose : To demonstrate electric forklift charging coordination using distributed networking.
Description : In warehouses , there may be many battery powered forklifts, but limited charging
                outlets are available, this project demonstrates a noble way to share charger(resources}
                based on battery SOC(State of Charge). This Part of code serves as a wifi to uart bridge for 
				Nodes participating in MQTT network.
Author: Kankan Sarkar
Modifications : 17/01/2018-- V1.0-- Initial Creation, MQTT Test, UART TEST
                21/03/2019-- V1.2-- Final version

***/
#include <ESP8266WiFi.h>  // Wifi Driver
#include <PubSubClient.h> // MQTT Header

const char* ssid = "AIRTEL_GILL"; // Put Your SSID
const char* password = "A!RTEL_G!LL"; // Put Your Password
const char* mqtt_server = "192.168.0.101";  // Server Address
//#define debug 1
#define node_id 5 // node id
const char *myname="NODE5"; // node name for mqtt broker
#define broadcast 255
WiFiClient espClient;		// Spawn Wifi Client 
PubSubClient client(espClient); // Spawn MQTT Client
long lastMsg = 0;				// Flag to send Ping Request
char msg[50];					// local buffer to store uart messages from NODE
int value = 0;					
char *token;					// CSV parser buffer
char buf3[30];					// local buffer to store ID(char)
int ID,statt;					// local buffer to store ID(int),SOC(int)
char buf[50];					// local buffer to store subscribe TOPIC
int id,stat,destination;		// variable to store UART received ID(int), SOC(int), REMOTE ID(int)
char temp_buf[100];				// local buffer
int index1=0;					
char buf2[20];					// buffer to store topic
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(9600);				//intialize UART with 9600
  setup_wifi();						// wifi init
  client.setServer(mqtt_server, 1883); // MQTT init
  client.setCallback(callback);			// MQTT setcallback on message
}

void setup_wifi() {
  delay(10); // simply some delay :)
  
  #ifdef debug // debug message enable Directive to enable
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef debug// debug message enable Directive to enable
    Serial.print(".");
    #endif
  }
  #ifdef debug// debug message enable Directive to enable
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  #ifdef debug
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
for(int i=0;i<length;i++)
  {
    buf3[i]=(char)payload[i]; // convert byte array to char array
  }
// parse CSV message into ID and stat
token = strtok(buf3, ",");
ID=atoi(token);
token = strtok(NULL, ",");
statt=atoi(token);
#ifdef debug
Serial.print("ID=");
Serial.println(ID);
Serial.print("Status=");
Serial.println(statt);
#endif
for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    #ifdef debug
    Serial.print("Attempting MQTT connection...");
    #endif
    if (client.connect(myname)) {
      #ifdef debug
      Serial.println("COnnected Now");
      #endif
      char buf_temp_sub[10];
      String(node_id).toCharArray(buf_temp_sub,10);
      client.subscribe(buf_temp_sub);
    } else {
      #ifdef debug
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      #endif
      delay(5000);
    }
  }
}

void loop(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(Serial.available())
    {
      char c=Serial.read();
      if(c=='#')
        {
		// if uart message reception done  
        index1=0;// set index as 0
        token = strtok(temp_buf, ","); //parse CSV received from UART
        destination = atoi(token);
        token = strtok(NULL, ",");
        id = atoi(token);
        token = strtok(NULL, ",");
        stat = atoi(token);
        sprintf(buf,"%d,%d#",id,stat); // prepare payload
        sprintf(buf2,"%d",destination); // prepare topic
        client.publish(buf2,buf); // publish message to remote NODE
        #ifdef debug // display for fun :)
        Serial.print("id=");
        Serial.println(id);
        Serial.print("stat=");
        Serial.println(stat);
        Serial.print("destination=");
        Serial.println(destination);
        #endif
        //do your thing
        }
      else{
        temp_buf[index1]=c;
        temp_buf[index1+1]='\0';
        index1+=1;
      }
    }
  
}
