#include <Arduino.h>


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
#define MODE_C 'm'
#define CHANGE_TEMP_MIN 'b'
#define CHANGE_TEMP_MAX 'a'
#define WIN_OPEN 'o'
#define WIN_CLOSE 'n'


//node
#define NODE_DATA_1 1
#define NODE_DATA_2 2
#define NODE_BROKER 5
 
int angle = 90;
int state = MESH_WAITING_ST;
float max_I = 780.00;
bool overload_flag = 0;
bool manual_fl = 0;
int nodeNumber = 3;

double temp1 = 22.234;
double temp2 = 23.423;
double min_temp = 15.000000;
double max_temp = 25.00000;
float current = 0;

Scheduler userScheduler;//планировщик
painlessMesh mesh;//объект сети mesh
String readings;//сохраняем показания сюды

String part_msg = "70";
String allST = "00";

void sendMessage(); 

String getReadings(); // получение показаний датчика
Task taskSendMessage(TASK_SECOND * 10 , TASK_ONCE, &sendMessage);//каждые 10 сек ПОСТОЯННО передает сообщение

String getReadings () {

//Функция getReadings () получает показания температуры, влажности и давления от датчика 
// и объединяет всю информацию, включая номер ноды, в переменной jsonReadings.
  //uint32_t now = millis();

    JSONVar jsonReadings;
    jsonReadings["node"] = nodeNumber;
    jsonReadings["cmd"] = part_msg;
    if (allST.length() < 2){
        allST += "0";
    }
    jsonReadings["value"] = String(allST);
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
    int node = myObject["node"];
    unsigned char cmd = myObject["cmd"];
    switch(node){
        case NODE_DATA_1:{
            String cmd_h_s_s = myObject["cmd"];
            state = DATA_ANALYSIS_ST;
            if (cmd_h_s_s == "101"){
                temp1 = myObject["value"];
            }
            break;
        }
        case NODE_DATA_2:{
            String cmd_h_s_s = myObject["cmd"];
            state = DATA_ANALYSIS_ST;
            if (cmd_h_s_s == "101"){
                temp2 = myObject["value"];
            }
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
                case WIN_OPEN:{
                    state = WIN_OPEN_ST;
                    angle = myObject["value"];
                    break;
                }
                case WIN_CLOSE:{
                    Serial.println("ok");
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
    //part_msg = "77";
    //sendMessage();
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
    //part_msg = "78";
    //sendMessage();
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
    angle = 0;
    myServo.write(angle);
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
    pinMode(A0, INPUT);
    myServo.write(120);
    mesh_initiate();
}

uint32_t last_time1 = 0;
uint32_t last_time2 = 1000;
static uint32_t timeout1 = 7000;
static uint32_t timeout2 = 5500;
 
void loop() {
 // static uint32_t lastTime = 0;
 // uint32_t now = millis();
 // if (abs(int(now - lastTime)) >= TIMEOUT){
 //  angle = (angle + STEP_ANGLE) % MAX_ANGLE;
 //  myServo.write(angle);
 //  lastTime = millis();
 // }
    mesh.update();
    delay(700);
    part_msg = "105";
    allST = String(angle);
    sendMessage();
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
    uint32_t now1 = millis();
    if (abs(int(now1 - last_time1))>=timeout1){
      current = analogRead(A0);
      last_time1 = millis();
      Serial.println(current);
    }
    if (current > max_I){
        overload_flag = true;
        allST = "11";
    }
    else{
        overload_flag = false;
        allST = "00";
    }
    uint32_t now2 = millis();
    if (abs(int(now2 - last_time2))>=timeout2){
        part_msg = "104";
        sendMessage();
        last_time2 = millis();
    }
}
