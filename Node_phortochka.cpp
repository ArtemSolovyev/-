#include <Arduino.h>

#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <Servo.h>
#include "ACS712.h"

#define MESH_PREFIX "RNTMESH" // имя MESH
#define MESH_PASSWORD "MESHpassword" // пароль
#define MESH_PORT 5555 

#define MESH_WAITING_ST 0
#define WAITING_ST 1
#define DATA_ANALYSIS_ST 2
#define MSG_MANUAL_WAITING_ST 5
#define WIN_OPEN_ST 3
#define WIN_CLOSED_ST 4

#define NO_C -1
#define AUTO_MODE_C 0
#define MANUAL_MODE_C 1
#define CHANGE_TEMP_MIN 2
#define CHANGE_TEMP_MAX 3
#define WIN_OPEN 4
#define WIN_CLOSE 5

int state = MESH_WAITING_ST;
float max_I = 22.5;
bool overload_flag = 0;
bool manual_fl = 0;
int nodeNumber = 3;
double temp1 = 22.234;
double temp2 = 23.423;
double min_temp;
double max_temp;
int angle = 90;

ACS712 sensor(ACS712_30A, A0);
Servo servo;
Scheduler userScheduler;//планировщик
painlessMesh  mesh;//объект сети mesh
String readings;
String part_msg = "no_extra_info";
void sendMessage();
String getReadings(); // получение показаний датчика
Task taskSendMessage(TASK_SECOND * 10 , TASK_FOREVER, &sendMessage);
String getReadings (){
    JSONVar jsonReadings;
    jsonReadings["node"] = nodeNumber;
    if (overload_flag == true){
        jsonReadings["extra_mode"] = "OVERLOAD";
    }
    jsonReadings["modes"] = part_msg;
    readings = JSON.stringify(jsonReadings);
    return readings;
}

void sendMessage (){//отправляет строку JSON с показаниями и номером ноды (getReadings ()) ВСЕМ нодам в сети.
  String msg = getReadings();
  mesh.sendBroadcast(msg);
}

void receivedCallback( uint32_t from, String &msg ) {//выводит отправителя сообщения и содержимое сообщения (msg.c_str ())
  Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  int cmd = myObject["cmd"];
  int node = myObject["node"];
  //String received = JSON.stringify(myObject);
  //Serial.print(received);
  switch(node){
    case 1:{
        state = DATA_ANALYSIS_ST;
        temp1 = myObject["temp"];
        break;
    }
    case 2:{
        state = DATA_ANALYSIS_ST;
        temp2 = myObject["temp"];
        break;
    }
    case 5:{
      switch(cmd){
          case CHANGE_TEMP_MIN:{
              min_temp = myObject["value"];
              break;
          }
          case CHANGE_TEMP_MAX:{
              max_temp = myObject["value"];
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
          case WIN_OPEN:{
              state = WIN_OPEN_ST;
              angle = myObject["value"];
              break;
          }
          case WIN_CLOSE:{
              state = WIN_CLOSED_ST;
              break;
          }
          default:{
            break;
          }
      }
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
    uint32_t node_ID = mesh.getNodeId();
    state = WAITING_ST;
    if (!mesh.isConnected(node_ID)){
        state = MESH_WAITING_ST;
    }
}

void mesh_waiting_hadler(){
    mesh_init();
}

void waiting_hadler(){
    part_msg = "AUTO MODE ON";
}

void data_handler(){
    double sr_ar = ((temp1 + temp2)/2);
    if (sr_ar > max_temp){
        state = WIN_OPEN_ST;
    }
    else{
        if (sr_ar < min_temp){
            state = WIN_CLOSED_ST;
        }
    }
}

void msgManual_hadler(){
    part_msg = "MANUAL MODE ON";
}

void open_hadler(){
    if (overload_flag == true){
        if (angle > 45){
            angle = 45;
        }
    }
    servo.write(angle);
    if (manual_fl == true){
        state = MSG_MANUAL_WAITING_ST;
    }
    else{
        state = DATA_ANALYSIS_ST;
    }
}

void closed_hadler(){
    servo.write(0);
    if (manual_fl == true){
        state = MSG_MANUAL_WAITING_ST;
    }
    else{
        state = DATA_ANALYSIS_ST;
    }
}

void setup(){
    Serial.begin(115200);
    mesh_init();
    servo.attach(2);
    servo.write(0);
    // int zero = sensor.calibrate();
}

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
        case DATA_ANALYSIS_ST:{
            data_handler();
            break;
        }
        case MSG_MANUAL_WAITING_ST:{
            msgManual_hadler();
            break;
        }
        case WIN_OPEN_ST:{
            open_hadler();
            break;
        }
        case WIN_CLOSED_ST:{
            closed_hadler();
            break;
        }
    }
    float current = sensor.getCurrentDC();
    if (current > max_I){
        overload_flag = true;
    }
}

