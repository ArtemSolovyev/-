#include <Arduino.h>
/*
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <Servo.h>
#include "ACS712.h"

#define ___DEBUG___

#ifdef ___DEBUG___
	#define DPRINT(X) Serial.print(X)
	#define DPRINTLN(X) Serial.println(X)
#else
	#define DPRINT(X)
	#define DPRINTLN(X)
#endif


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

#define SERVO_PIN D3

int state = MESH_WAITING_ST;
float max_I = 22.5;
bool overload_flag = 0;
bool manual_fl = 0;
int nodeNumber = 3;

double temp1 = 22.234;
double temp2 = 23.423;
double min_temp = 10.000000;
double max_temp = 20.00000;
int angle = 0;

//uint32_t nodeId = 2561273534;

ACS712 sensor(ACS712_30A, A0);
Servo servo;
Scheduler userScheduler;//планировщик
painlessMesh mesh;//объект сети mesh
String readings;

String part_msg = "no_extra_info";


void sendMessage();

String getReadings(); // получение показаний датчика

Task taskSendMessage(TASK_SECOND * 10 , TASK_FOREVER, &sendMessage);


String getReadings(){
    JSONVar jsonReadings;
    jsonReadings["node"] = nodeNumber;
    if (overload_flag == true){
        jsonReadings["extra_mode"] = "OVERLOAD";
    }
    jsonReadings["modes"] = part_msg;
    jsonReadings["angle"] = angle;
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
  int node = myObject["node"];
  int cmd = myObject["cmd"];
  String received = JSON.stringify(myObject);
  Serial.print(received);
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
    //uint32_t nodeId = 2561273534;
    //uint32_t node_ID = mesh.getNodeId();
    //Serial.println(node_ID);
    //state = WAITING_ST;
    //if (mesh.isConnected(mesh.getNodeId()) == false){
    //    state = MESH_WAITING_ST;
    //}
}

void mesh_waiting_hadler(){
    state = WAITING_ST;
    if (mesh.isConnected(mesh.getNodeId()) == true){
        state = MESH_WAITING_ST;
    }
}

void waiting_hadler(){
    part_msg = "AUTO MODE ON";
	DPRINTLN(">> waiting handler. AUTO MODE ON");
}

void data_handler(){
    double sr_ar = ((temp1 + temp2)/2);
	DPRINT(">> data_handler. ");
	DPRINTLN(sr_ar);
    if (sr_ar > max_temp){
		DPRINT(">> Temp is bigger, than max_temp. ");
        state = WIN_OPEN_ST;
    }
    else{
        if (sr_ar < min_temp){
			DPRINT(">> Temp is lower, than max_temp. ");
            state = WIN_CLOSED_ST;
        }
    }
}

void msgManual_hadler(){
    part_msg = "MANUAL MODE ON";
	DPRINT(">> msgManual_hadler. MANUAL MODE ON");
}

void open_hadler(){
	DPRINT(">> open_hadler");
    if (overload_flag == true){
		DPRINT(">> overload!!!!");
        if (angle > 45){
			DPRINT(">> Angle is more than 45! Now angle is 45!");
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
    //servo.attach(SERVO_PIN);
    //servo.write(0);
    // int zero = sensor.calibrate();
}

void loop(){
    mesh.update();
    switch (state){
        case MESH_WAITING_ST:{
			DPRINTLN(">> mesh_waiting state - STATE");
            mesh_waiting_hadler();
            break;
        }
        case WAITING_ST:{
			DPRINTLN(">> command waiting state - STATE");
            waiting_hadler();
            break;
        }
        case DATA_ANALYSIS_ST:{
			DPRINTLN(">> Sensors data was received - STATE");
            data_handler();
            break;
        }
        case MSG_MANUAL_WAITING_ST:{
			DPRINTLN(">> Manual mode is on - STATE");
            msgManual_hadler();
            break;
        }
        case WIN_OPEN_ST:{
			DPRINTLN(">> OPENNING WINDOW  - STATE");
            open_hadler();
            break;
        }
        case WIN_CLOSED_ST:{
			DPRINTLN(">> CLOSING WINDOW  - STATE");
            closed_hadler();
            break;
        }
    }
    float current = sensor.getCurrentDC();
    if (current > max_I){
        overload_flag = true;
    }
    else{
        overload_flag = false;
    }
}*/
////////////////////////////////////////////////////

#include <Servo.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
//#include "ACS712.h"

#define SERVO_PIN D3

Servo myServo;
//ACS712  ACS(A0, 5.0, 1023, 185);

#define MESH_PREFIX "RNTMESH" // имя MESH
#define MESH_PASSWORD "MESHpassword" // пароль

#define MESH_PORT 5555 // порт по умолчанию


//states
#define MESH_WAITING_ST 0
#define WAITING_ST 1
#define DATA_ANALYSIS_ST 2
#define MSG_MANUAL_WAITING_ST 5
#define WIN_OPEN_ST 3
#define WIN_CLOSED_ST 4

//cmd
#define NO_C -1
#define AUTO_MODE_C 0
#define MANUAL_MODE_C 1
#define CHANGE_TEMP_MIN 2
#define CHANGE_TEMP_MAX 3
#define WIN_OPEN 4
#define WIN_CLOSE 5


//node
#define NODE_DATA_1 1
#define NODE_DATA_2 2
#define NODE_BROKER 5
 
int angle = 0;
int state = MESH_WAITING_ST;
float max_I = 780.00;
bool overload_flag = 0;
bool manual_fl = 0;
int nodeNumber = 3;

double temp1 = 22.234;
double temp2 = 23.423;
double min_temp = 10.000000;
double max_temp = 20.00000;
float current = 0;

Scheduler userScheduler;//планировщик
painlessMesh mesh;//объект сети mesh
String readings;//сохраняем показания сюды

String part_msg = "no_extra_info";

void sendMessage(); // благодаря ему PlatformIO будет работать

String getReadings(); // получение показаний датчика
Task taskSendMessage(TASK_SECOND * 10 , TASK_FOREVER, &sendMessage);//каждые 10 сек ПОСТОЯННО передает сообщение

String getReadings () {

//Функция getReadings () получает показания температуры, влажности и давления от датчика 
// и объединяет всю информацию, включая номер ноды, в переменной jsonReadings.
  //uint32_t now = millis();

    JSONVar jsonReadings;
    jsonReadings["node"] = nodeNumber;
    jsonReadings["angle"] = angle;
    if (overload_flag == true){
        jsonReadings["extra_mode"] = "OVERLOAD";
    }
    jsonReadings["modes"] = part_msg;
    readings = JSON.stringify(jsonReadings);//значения переменной jsonReadings преобразуется в строку JSON
    //и сохраняется в переменной readings.
    return readings;
  
}

void sendMessage (){//отправляет строку JSON с показаниями и номером ноды (getReadings ()) ВСЕМ нодам в сети.
    String msg = getReadings();
    mesh.sendBroadcast(msg);
}

// Нужно для работы библиотеки painlessMesh
 
void receivedCallback( uint32_t from, String &msg ) {//выводит отправителя сообщения и содержимое сообщения (msg.c_str ())
    Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    JSONVar myObject = JSON.parse(msg.c_str());
    String received = JSON.stringify(myObject);
    Serial.print(received);
    int node = myObject["node"];
    int cmd = myObject["cmd"];
    switch(node){
        case NODE_DATA_1:{
            state = DATA_ANALYSIS_ST;
            temp1 = myObject["temp"];
            break;
        }
        case NODE_DATA_2:{
            state = DATA_ANALYSIS_ST;
            temp2 = myObject["temp"];
            break;
        }
        case NODE_BROKER:{
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

void mesh_initiate(){
    //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // выбираем типы  mesh.setDebugMsgTypes( ERROR | STARTUP );  // установите перед функцией init() чтобы выдавались приветственные сообщения
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
	//DPRINTLN(">> waiting handler. AUTO MODE ON");
}

void data_handler(){
    double sr_ar = ((temp1 + temp2)/2);
	//DPRINT(">> data_handler. ");
	//DPRINTLN(sr_ar);
    if (sr_ar > max_temp){
		//DPRINT(">> Temp is bigger, than max_temp. ");
        state = WIN_OPEN_ST;
    }
    else{
        if (sr_ar < min_temp){
			//DPRINT(">> Temp is lower, than max_temp. ");
            state = WIN_CLOSED_ST;
        }
    }
}

void msgManual_hadler(){
    part_msg = "MANUAL MODE ON";
	//DPRINT(">> msgManual_hadler. MANUAL MODE ON");
}

void open_hadler(){
	//DPRINT(">> open_hadler");
    if (overload_flag == true){
		//DPRINT(">> overload!!!!");
        if (angle > 45){
			//DPRINT(">> Angle is more than 45! Now angle is 45!");
            angle = 45;
        }
    }
    myServo.write(angle);
    if (manual_fl == true){
        state = MSG_MANUAL_WAITING_ST;
    }
    else{
        state = DATA_ANALYSIS_ST;
    }
}

void closed_hadler(){
    myServo.write(0);
    if (manual_fl == true){
        state = MSG_MANUAL_WAITING_ST;
    }
    else{
        state = DATA_ANALYSIS_ST;
    }
}

void setup() {
    Serial.begin(115200);
    myServo.attach(SERVO_PIN);
    myServo.write(120);
    mesh_initiate();
}

uint32_t last_time = 0;
static uint32_t timeout = 5000;
 
void loop() {
 // static uint32_t lastTime = 0;
 // uint32_t now = millis();
 // if (abs(int(now - lastTime)) >= TIMEOUT){
 //  angle = (angle + STEP_ANGLE) % MAX_ANGLE;
 //  myServo.write(angle);
 //  lastTime = millis();
 // }
    mesh.update();
    switch (state){
        case MESH_WAITING_ST:{
            //DPRINTLN(">> mesh_waiting state - STATE");
            mesh_waiting_hadler();
            break;
        }
        case WAITING_ST:{
            //DPRINTLN(">> command waiting state - STATE");
            waiting_hadler();
            break;
        }
        case DATA_ANALYSIS_ST:{
            //DPRINTLN(">> Sensors data was received - STATE");
            data_handler();
            break;
        }
        case MSG_MANUAL_WAITING_ST:{
            //DPRINTLN(">> Manual mode is on - STATE");
            msgManual_hadler();
            break;
        }
        case WIN_OPEN_ST:{
            //DPRINTLN(">> OPENNING WINDOW  - STATE");
            open_hadler();
            break;
        }
        case WIN_CLOSED_ST:{
            //DPRINTLN(">> CLOSING WINDOW  - STATE");
            closed_hadler();
            break;
        }
        default:{
            break;
        }
    }
    uint32_t now = millis();
    if (abs(int(now - last_time))>=timeout){
      current = analogRead(A0);
      last_time = millis();
      Serial.println(current);
    }
    if (current > max_I){
        overload_flag = true;
    }
    else{
        overload_flag = false;
    }
}
