#include <Arduino.h>

#include "painlessMesh.h"
#include <Arduino_JSON.h>

#define MESH_PREFIX "RNTMESH" // имя MESH
#define MESH_PASSWORD "MESHpassword" // пароль
#define MESH_PORT 5555 

#define IRR D5
#define POMP D6
#define ttp223_butt D7
#define MESH_WAITING_ST 0
#define WAITING_ST 1
#define SENSOR_DATA_ANALYSIS_ST 2
#define irON_ST 3
#define wateringON_ST 4
#define MSG_MANUAL_WAITING_ST 5

//cmd
#define MODE_C 'm'
#define IRIGATOR_ST 'k'
#define CHANGE_HUM_LIMIT 'c'
#define CHANGE_MOISTURE_LIMIT 'g'
#define WATERING_ST 'j'

#define NODE_DATA_1 1
#define NODE_DATA_2 2
#define NODE_BROKER 5


int state = MESH_WAITING_ST;
bool manual_fl = 0;
int nodeNumber = 4;
int hum1 = 0;
int hum2 = 0;
int soil1 = 0;
int soil2 = 0;
int min_hum = 10;
int min_soil = 30;
int water_level = 54;

uint32_t last_time_ir = 0;
uint32_t last_time_pomp = 0;
uint32_t timeout_ir = 10000;
uint32_t timeout_pomp = 10000;

Scheduler userScheduler;//планировщик
painlessMesh  mesh;//объект сети mesh
String readings;
String part_msg = "70";
void sendMessage();
String getReadings();
Task taskSendMessage(TASK_SECOND * 10 , TASK_ONCE, &sendMessage);
String getReadings (){
  JSONVar jsonReadings;
  jsonReadings["node"] = nodeNumber;
  jsonReadings["cmd"] = part_msg;
  String send_value = String(water_level);
  if (send_value.length() < 2){
    send_value += "0";
  }
  jsonReadings["value"] = send_value;
  readings = JSON.stringify(jsonReadings);
  return readings;
}

void sendMessage (){//отправляет строку JSON с показаниями и номером ноды (getReadings ()) ВСЕМ нодам в сети.
  String msg = getReadings();
  mesh.sendBroadcast(msg);
}

void manual_ir_off(){
  digitalWrite(IRR, 1);
  delay(50);
  digitalWrite(IRR, 0);
  delay(50);
  digitalWrite(IRR, 1);
  delay(50);
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
  int node = myObject["node"];
  unsigned char cmd = myObject["cmd"];
  String received = JSON.stringify(myObject);
  Serial.println(received);
  Serial.println(node);
  Serial.println(int(node) == 5);
  // {"node":5, "cmd": 3, "value": 0}
  // {"node":5, "cmd": 4, "value": 30}
  switch(node){
    case NODE_DATA_1:{
      Serial.println("RES_D from node 1");
      state = SENSOR_DATA_ANALYSIS_ST;
      String cmd_h_s_s = myObject["cmd"];
      if (cmd_h_s_s == "102"){
        hum1 = myObject["value"];
      }
      if (cmd_h_s_s == "103"){
        soil1 = myObject["value"];
      }
      break;
    }
    case NODE_DATA_2:{
      Serial.println("RES_D from node 2");
      String cmd_h_s_s = myObject["cmd"];
      if (cmd_h_s_s == "102"){
        hum2 = myObject["value"];
      }
      if (cmd_h_s_s == "103"){
        soil2 = myObject["value"];
      }
      break;
    }
    case NODE_BROKER:{
      Serial.println("RES from node 5");
      switch(cmd){
        case CHANGE_HUM_LIMIT:{
          min_hum = myObject["value"];
          break;
        }
        case CHANGE_MOISTURE_LIMIT:{
          min_soil = myObject["value"];
          break;
        }
        case MODE_C:{
          int mode = myObject["value"];
          if (mode == 0){
            state = MSG_MANUAL_WAITING_ST;
            manual_fl = true;
          }
          else if (mode == 1){
            state = WAITING_ST;
            manual_fl = false;
          }
          break;
        }
        case IRIGATOR_ST:{
          int ir_st = myObject["value"];
          if (ir_st == 60){
            state = irON_ST;
          }
          else if (ir_st == 0){
            //manual_ir_off();
          }
          break;
        }
        case WATERING_ST:{
          int pomp_st = myObject["value"];
          Serial.println("water_on res-d");
          if (pomp_st == 60){
            Serial.println("check passed");
            state = wateringON_ST;
          }
          else if (pomp_st == int(0)){
            //manual_watering_off();
          }
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
  //part_msg = '77';
  //sendMessage();
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
  //part_msg = '78';
  //sendMessage();
}

void irON_hadler(){
  digitalWrite(IRR, 1);
  delay(50);
  digitalWrite(IRR, 0);
  Serial.println("hum act 1");
  uint32_t now = millis();
  if (((now - last_time_ir) >= timeout_ir) && (manual_fl == false)){
    digitalWrite(IRR, 1);
    delay(50);
    digitalWrite(IRR, 0);
    delay(50);
    digitalWrite(IRR, 1);
    delay(50);
    digitalWrite(IRR, 0);
    state = SENSOR_DATA_ANALYSIS_ST;
  }
}

void wateringON_hadler(){
  digitalWrite(POMP, 1);
  delay(10000);
  Serial.println("wat_on");
  uint32_t now = millis();
  if (((now - last_time_pomp) >= timeout_pomp) && (manual_fl == false)){
    digitalWrite(POMP, LOW);
    state = SENSOR_DATA_ANALYSIS_ST;
  }
}

void setup(){
  Serial.begin(115200);
  mesh_init();
  pinMode(ttp223_butt, INPUT_PULLUP);
  pinMode(POMP, OUTPUT);
  pinMode(IRR, OUTPUT);
}

uint32_t last_time = 0;
static uint32_t timeout = 5000;

void loop(){
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
    water_level = digitalRead(ttp223_butt);
    part_msg = "108";
    last_time = millis();
    sendMessage();
  }
  mesh.update();
}