#include "M5CoreS3.h"
#include "WiFi.h"
#include "string.h"
#include <WebSocketsClient.h>
#include <DFRobot_PAJ7620U2.h>
#include <ArduinoJson.h>
#include "stb_image_resize.h"
#include <Wire.h>

int disp_w; // 画面幅格納用
int disp_h; // 画面高さ格納用
int cur_value = 1; //現在のボタンの状態
int last_value = 1; //一つ前のボタンの状態
bool send_data =false; //画像データを送るかのフラグ
DFRobot_PAJ7620U2 sensor; //RAJ7620U2
const char ssid[] = "ssid"; //WiFiのSSID
const char pass[] = "pass"; //WiFiのpassword
WebSocketsClient webSocket; //webSocketCLient

/*
 * PAJ7620U2で取得したジェスチャをWebSocketで送信する
 * message: 取得したジェスチャの内容
 */
void sendDynamiGestureData(const String& message){
  StaticJsonDocument<JSON_OBJECT_SIZE(1)> jsonDoc;
  jsonDoc["grove_gesture"] = message;
  String jsonString;
  serializeJson(jsonDoc,jsonString);
  Serial.println(jsonString);
  webSocket.sendTXT(jsonString);
}

/*
 * WiFiに接続するための関数
 */
void connectWiFi(){
  WiFi.begin(ssid,pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    CoreS3.Display.print(".");
  }
  CoreS3.Display.println("WiFi connected");
  CoreS3.Display.print("IP address = ");
  CoreS3.Display.println(WiFi.localIP());
}

/*
 * gestureを取得してWebSocketで送信する関数
 */
void getGesture(){
  DFRobot_PAJ7620U2::eGesture_t gesture = sensor.getGesture();
  if (gesture != sensor.eGestureNone){
    String description = sensor.gestureDescription(gesture);
    Serial.println(description);
    sendDynamiGestureData(description);
    if (WiFi.status() == WL_DISCONNECTED){
      connectWiFi();
    }
  }
}

/*
 * webSocket通信のEvent関数 
 * type: イベントの種類
 * payload: 受信したデータのポインタ
 * length: 受信したデータの長さ
 */
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WSc] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            Serial.printf("[WSc] Connected to url: %s\n", payload); 
            break;
        case WStype_TEXT:
            Serial.printf("[WSc] get text: %s\n", payload);
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

void setup() {
  // 本体初期化
  auto cfg = M5.config();
  CoreS3.begin(cfg);

  Serial.begin(115200); 

  // 画面の縦横幅取得
  disp_w = CoreS3.Display.width();
  disp_h = CoreS3.Display.height();

  // カメラ初期化
  CoreS3.Camera.begin();

  // 8番ピンを入力に設定
  pinMode(8,INPUT); 

  Wire.begin();

  //ジェスチャセンサの初期化
  Serial.println("PAJ7620U2 Init");
  while (sensor.begin() != 0) {
    Serial.print("initial PAJ7620U2 failure!");
    delay(500);
  }
  sensor.setGestureHighRate(true);
  Serial.println("PAJ7620U2 init succeed.");

  //WiFiに接続
  connectWiFi();

  //webSocketの設定
  webSocket.begin("websocket-host", 1880,"/ws/m5CoreS3");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(500);
  
}

void loop() {
  //映像データ送信をボタンで制御
  cur_value = digitalRead(8);
  if(cur_value==0 && last_value==1){
    send_data = !send_data;
  }

  //画像データのポインタと長さ
  uint8_t* out_jpg;
  size_t out_len;
  
  //画像を取得してwebSocketで送信
  if (CoreS3.Camera.get()) {
    if(send_data){
      frame2jpg(CoreS3.Camera.fb,40,&out_jpg,&out_len);
      webSocket.sendBIN(out_jpg,out_len);
      free(out_jpg);
    }
    CoreS3.Display.pushImage(0, 0, disp_w, disp_h,(uint16_t*)CoreS3.Camera.fb->buf); // QVGA表示 (x, y, w, h, *data)
    CoreS3.Camera.free(); // 取得したフレームを解放
  }

  //ジェスチャセンサで取得したデータを送信
  getGesture();

  webSocket.loop();
  last_value = cur_value;
}