#include <Arduino.h>

#include "painlessMesh.h"
#include <Arduino_JSON.h>

#define MESH_PREFIX "RNTMESH" // имя MESH
#define MESH_PASSWORD "MESHpassword" // пароль
#define MESH_PORT 5555 

#define IRR D5
#define POMP D6
#define MESH_WAITING_ST 0
#define WAITING_ST 1
#define SENSOR_DATA_ANALYSIS_ST 2
#define irON_ST 3
#define wateringON_ST 4
#define MSG_MANUAL_WAITING_ST 5

#define NO_C -1
#define AUTO_MODE_C 0
#define MANUAL_MODE_C 1
#define IRIGATOR_ON 2
#define IRIGATOR_OFF 3
#define CHANGE_HUM_LIMIT 4
#define CHANGE_MOISTURE_LIMIT 5
#define WATERING_ON 6
#define WATERING_OFF 7

#define NODE_DATA_1 1
#define NODE_DATA_2 2
#define NODE_BROKER 5

int state = MESH_WAITING_ST;
bool manual_fl = 0;
int nodeNumber = 3;
double hum1;
double hum2;
double soil1;
double soil2;
double min_hum;
double min_soil;
float water_level = 54.00;

uint32_t last_time_ir;
uint32_t last_time_pomp;
uint32_t timeout_ir = 10000;
uint32_t timeout_pomp = 10000;

Scheduler userScheduler;//планировщик
painlessMesh  mesh;//объект сети mesh
String readings;
String part_msg = "no_extra_info";
void sendMessage();
String getReadings();
Task taskSendMessage(TASK_SECOND * 10 , TASK_FOREVER, &sendMessage);
String getReadings (){
    JSONVar jsonReadings;
    jsonReadings["node"] = nodeNumber;
    jsonReadings["modes"] = part_msg;
    jsonReadings["water level"] = water_level;
    readings = JSON.stringify(jsonReadings);
    return readings;
}

void sendMessage (){//отправляет строку JSON с показаниями и номером ноды (getReadings ()) ВСЕМ нодам в сети.
  String msg = getReadings();
  mesh.sendBroadcast(msg);
}

void manual_ir_off(){
    digitalWrite(IRR, 0);
    state = MSG_MANUAL_WAITING_ST;
}

void manual_watering_off(){
    digitalWrite(POMP, 0);
    state = MSG_MANUAL_WAITING_ST;
}

void receivedCallback( uint32_t from, String &msg ) {//выводит отправителя сообщения и содержимое сообщения (msg.c_str ())
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int cmd = myObject["cmd"];
  int node = myObject["node"];
  //String received = JSON.stringify(myObject);
  //Serial.print(received);
  switch(node){
    case NODE_DATA_1:{
        state = SENSOR_DATA_ANALYSIS_ST;
        hum1 = myObject["hum"];
        soil1 = myObject["soil"];
        break;
    }
    case NODE_DATA_2:{
        hum2 = myObject["hum"];
        soil2 = myObject["soil"];
        break;
    }
    case NODE_BROKER:{
      switch(cmd){
          case CHANGE_HUM_LIMIT:{
            min_hum = myObject["value"];
            break;
          }
          case CHANGE_MOISTURE_LIMIT:{
            min_soil = myObject["value"];
            break;
          }
          case MANUAL_MODE_C:{
            state = MSG_MANUAL_WAITING_ST;
            manual_fl = true;
            break;
          }
          case AUTO_MODE_C:{
            state = WAITING_ST;
            manual_fl = false;
            break;
          }
          case IRIGATOR_ON:{
            state = irON_ST;
            break;
          }
          case IRIGATOR_OFF:{
            manual_ir_off();
            break;
          }
          case WATERING_ON:{
            state = wateringON_ST;
            break;
          }
          case WATERING_OFF:{
            manual_watering_off();
            break;
          }
          default:{
            break;
          }
      }
      break;
    }
    default:{
      break;
    }
  }
}

void newConnectionCallback(uint32_t nodeId) {//работает,когда к сети подключается новая нода
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {//когда соединение изменяется в сети (когда узел присоединяется к сети или покидает ее).
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {//запускается, когда сеть регулирует время,
// так что все узлы синхронизируются. Печатает смещение.
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void mesh_init(){

    mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback); 

    userScheduler.addTask(taskSendMessage);//Планировщик отвечает за обработку и выполнение задач в нужное время.
    taskSendMessage.enable();//стартуем
}

void mesh_waiting_hadler(){
  state = WAITING_ST;
    if (mesh.isConnected(mesh.getNodeId()) == true){
        state = MESH_WAITING_ST;
    }
}

void waiting_hadler(){
    part_msg = "AUTO MODE ON";
}

void sensData_handler(){
    double sr_hum = ((hum1 + hum2)/2);
    if (sr_hum > min_hum){
        state = irON_ST;
        last_time_ir = millis();
    }
    double sr_soil = ((soil1 + soil2)/2);
    if (sr_soil > min_soil){
        state = wateringON_ST;
        last_time_pomp = millis();
    }
}

void msgManual_hadler(){
    part_msg = "MANUAL MODE ON";
}

void irON_hadler(){
    digitalWrite(IRR, 1);
    uint32_t now = millis();
    if (((now - last_time_ir) >= timeout_ir) && (manual_fl == false)){
        digitalWrite(IRR, 0);
        state = SENSOR_DATA_ANALYSIS_ST;
    }
}

void wateringON_hadler(){
    digitalWrite(POMP, 1);
    uint32_t now = millis();
    if (((now - last_time_pomp) >= timeout_pomp) && (manual_fl == false)){
        digitalWrite(POMP, 0);
        state = SENSOR_DATA_ANALYSIS_ST;
    }
}

void setup(){
    Serial.begin(115200);
    mesh_init();
}

uint32_t last_time = 0;
static uint32_t timeout = 5000;

void loop(){
    mesh.update();
    switch (state){
        case MESH_WAITING_ST:{
            mesh_waiting_hadler();
            break;
        }
        case WAITING_ST:{
            waiting_hadler();
            break;
        }
        case SENSOR_DATA_ANALYSIS_ST:{
            sensData_handler();
            break;
        }
        case MSG_MANUAL_WAITING_ST:{
            msgManual_hadler();
            break;
        }
        case irON_ST:{
            irON_hadler();
            break;
        }
        case wateringON_ST:{
            wateringON_hadler();
            break;
        }
        default:{
          break;
        }
    }
    uint32_t now = millis();
    if (abs(int(now - last_time))>=timeout){
      water_level = analogRead(A0);
      last_time = millis();
      Serial.println(water_level);
    }
}