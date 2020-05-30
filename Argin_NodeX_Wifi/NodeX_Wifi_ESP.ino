/***
Program Name: main.cpp
Purpose : To demonstrate electric forklift charging coordination using distributed networking.
Description : In warehouses , there may be many battery powered forklifts, but limited charging
                outlets are available, this project demonstrates a noble way to share charger(resources}
                based on battery SOC(State of Charge). This Part of code serves as a wifi to uart bridge for 
				Nodes participating in MQTT network.
Author: Kankan Sarkar
Modifications : 17/01/2018-- V1.0-- Initial Creation, MQTT Test, UART TEST
                18/01/2018-- V1.1-- Implemented Network Filter
                21/03/2019-- V1.2-- Final version

***/
#include <ESP8266WiFi.h>  // Wifi Driver
#include <PubSubClient.h> // MQTT Header

const char* ssid = "AIRTEL_GILL"; // Put Your SSID
const char* password = "A!RTEL_G!LL"; // Put Your Password
const char* mqtt_server = "192.168.0.101";  // Server Address
//#define debug 1
#define node_id 3 // Define Node ID which the Wifi Module will be paired
const char *myname="NODE3"; // define Client Name for MQTT Connection initiation
#define coordinator_id 0x05 // Define coordinator ID
#define disp_id 10			// Dashboard ID
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
  #ifdef debug// debug message enable Directive to enable - prints message received
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
    buf3[i]=(char)payload[i];
  }
 //parse message received
token = strtok(buf3, ",");
ID=atoi(token);
//Serial.print("ID=");
//Serial.println(atoi(token));
token = strtok(NULL, ",");
statt=atoi(token);
//Serial.print("Status=");
//Serial.println(atoi(token));

//network filter to block OWN broadcast message
if(strstr(topic,"255") != NULL){
if(ID!=node_id){
   for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}
}
else
{
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    #ifdef debug// debug message enable Directive to enable
    Serial.print("Attempting MQTT connection...");
    #endif
    if (client.connect(myname)) {
      #ifdef debug
      Serial.println("COnnected Now");
      #endif
      client.subscribe("255"); // subscribe to broadcast
      char buf_temp_sub[10];
      String(node_id).toCharArray(buf_temp_sub,10); //subscribe to OWN ID
      client.subscribe(buf_temp_sub);
    } else {
      #ifdef debug// debug message enable Directive to enable
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
  if(Serial.available())// if message received 
    {
      char c=Serial.read(); // read message
      if(c=='#')			//if message ending detected
        {					// now parse all message
          if(temp_buf[0]=='$')	// Dashboard message 
            {
              for(int i=0;i<index1;i++)
                {
                  buf[i]=temp_buf[i+1];
                  buf[i+1]='\0';
                }
        String(disp_id).toCharArray(buf2,10); // convert message to char array
        client.publish(buf2,buf);  				// publish message to dashboard
            }
        else{
        token = strtok(temp_buf, ","); // parse CSV
        destination = atoi(token);
        token = strtok(NULL, ",");
        id = atoi(token);
        token = strtok(NULL, ",");
        stat = atoi(token);
        sprintf(buf,"%d,%d#",id,stat);
        sprintf(buf2,"%d",destination);   
        client.publish(buf2,buf); //publish message to destination
        }
        index1=0;
        #ifdef debug
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
