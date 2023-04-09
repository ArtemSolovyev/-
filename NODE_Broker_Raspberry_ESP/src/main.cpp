#include <Arduino.h>

#include "painlessMesh.h"
#include <Arduino_JSON.h>

#define MESH_PREFIX "RNTMESH" // имя MESH
#define MESH_PASSWORD "MESHpassword" // пароль
#define MESH_PORT 5555 // порт по умолчанию

#define LED_PIN LED_BUILTIN

//nodes
#define NODE_DATA_1 1
#define NODE_DATA_2 2
#define NODE_HUM 3

int temp1 = 22;
int temp2 = 22;
int soil1 = 60;
int soil2 = 60;
int hum1 = 78;
int hum2 = 78;
char cmd_received;
String str_received;
//char* lol = const_cast<char*>(cmd_msg.c_str());
int nodeNumber = 2;//уникальный номер ноды платы
  
Scheduler userScheduler;//планировщик
painlessMesh  mesh;//объект сети mesh
String readings;//сохраняем показания сюды

void sendMessage(); // благодаря ему PlatformIO будет работать
 
String getReadings(); // получение показаний датчика
Task taskSendMessage(TASK_SECOND * 10 , TASK_FOREVER, &sendMessage);//каждые 10 сек ПОСТОЯННО передает сообщение
String getReadings () {//Функция getReadings () получает показания температуры, влажности и давления от датчика 
// и объединяет всю информацию, включая номер ноды, в переменной jsonReadings.
  //uint32_t now = millis();
  readings = str_received;//значения переменной jsonReadings преобразуется в строку JSON
    //и сохраняется в переменной readings.
  return readings;
}
void sendMessage (){//отправляет строку JSON с показаниями и номером ноды (getReadings ()) ВСЕМ нодам в сети.
  String msg = getReadings();
  mesh.sendBroadcast(msg);
}
// Нужно для работы библиотеки painlessMesh
 
void receivedCallback( uint32_t from, String &msg ) {//выводит отправителя сообщения и содержимое сообщения (msg.c_str ())
  //Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());
  Serial.println(JSON.stringify(myObject));
}

void newConnectionCallback(uint32_t nodeId) {//работает,когда к сети подключается новая нода
  //Serial.printf("New Connection, nodeId = %u\n", nodeId);
}
 
 
 
void changedConnectionCallback() {//когда соединение изменяется в сети (когда узел присоединяется к сети или покидает ее).
  //Serial.printf("Changed connections\n");
}
 
 
 
void nodeTimeAdjustedCallback(int32_t offset) {//запускается, когда сеть регулирует время,
// так что все узлы синхронизируются. Печатает смещение.
  //Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void broker_read(){
  if (Serial.available()){
    cmd_received = Serial.read();
    String char_received(1, cmd_received);
    str_received = char_received;
    sendMessage();
  }
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

void setup(){
  Serial.begin(115200);
   //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // выбираем типы  mesh.setDebugMsgTypes( ERROR | STARTUP );  // установите перед функцией init() чтобы выдавались приветственные сообщения
  mesh_init();
}

void loop(){
  mesh.update();
  broker_read();
}
