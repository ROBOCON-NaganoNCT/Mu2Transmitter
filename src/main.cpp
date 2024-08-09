#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <ButtonManager.h>
#include "pin.h"
#include <MUwrapper.hpp>
#include <wiiClassic.h>
#include <controller.h>
#include <buzzer.h>
#include <buzzer_score.h>


#define DATA_VERSION "DATA1.0"

TaskHandle_t Main_Handle = NULL;
TaskHandle_t Mu_Handle = NULL;
TaskHandle_t Display_Handle = NULL;

QueueHandle_t controller_TO_mainQueue = NULL;
QueueHandle_t controller1_TO_mainQueue = NULL;
QueueHandle_t config_TO_mainQueue = NULL;
QueueHandle_t main_TO_MuQueue = NULL;



controller::ControllerData controller_main[2];
Packetizer packetizer;

struct QueueData{
  uint8_t Mudata[12];
  uint8_t len;
  int8_t config[8];

};
enum mu_config_items{
  UserID,
  GroupID,
  DeviceID,
  TargetID,
  Channel ,
  Mode,
  Frequency,
  Initial
};

struct ConfigData{
  int8_t configdata[8];
};

struct EEPROM_data{
  int8_t config[8];
  char check[10];
};

int16_t frequency = 20;

uint8_t generate_mudata(uint8_t *buf, bool emergency,int mode){//最大１２バイト

  if(emergency){  //非常停止aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    buf[0] = 'E';
    return 1;
  }else{
    packet_t p;
    packetizer.init(p);
    p=controller_main[0].packetize(packetizer);
    memcpy(buf,p.data,5);
    buf[5] = 0x0;
    if (mode == 12){
      packetizer.init(p);
      p=controller_main[1].packetize(packetizer);
      memcpy(buf+6,p.data,5);
      buf[11] = 0x0;
      return 12;
    }

    return 5;
  }
}


//Mu2にシリアルで文字を送る関数Muwrapperのコールバックを受けて実行される
void SendData(MUEvent event, uint8_t *data, uint8_t len){
  if (event == MU_EVENT_ERROR){
    Serial.write("MU_EVENT_ERROR");
  }
  if (event == MU_EVENT_SEND_REQUEST){
      Serial1.write(data,len);
        
  }
}



//↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓タスク↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓

//mainタスク
void main_task(void *pvParameters) {
  Serial.begin(9600);

  ButtonManager btn2;
  btn2.add(Emergency, 15);

  uint8_t Mudata[12];
  
  QueueData queue_send_data;
  ConfigData result_config;
  char inchar;

  int32_t lasttime = 0;
    
  while (1){
    if (millis() - lasttime > 15){
      btn2.update();

      xQueueReceive(controller_TO_mainQueue, &controller_main[0],1);
      xQueueReceive(controller1_TO_mainQueue, &controller_main[1],1);
      xQueueReceive(config_TO_mainQueue, &result_config,1);

      queue_send_data.len = generate_mudata(Mudata, !btn2.isHold(0),result_config.configdata[Mode]);
      memcpy(&queue_send_data.config, &result_config.configdata, sizeof(result_config.configdata));
      memcpy(&queue_send_data.Mudata, &Mudata, sizeof(Mudata));
      

      //出力
      xQueueSend(main_TO_MuQueue,&queue_send_data,0);
      lasttime = millis();
    }

  if (Serial1.available()) {

    inchar = Serial1.read();
    Serial.printf("%c",inchar);

  }
  vTaskDelay(1);
  }
}





//Mu2のタスク
void Mu(void *pvParameters){
  Serial1.begin(19200,SERIAL_8N1,Mu_TXD,Mu_RXD);

  int32_t lasttime = 0;
  int8_t lastconfig[8]={0,0,0,0,0,0,0,0};

  MUWrapper mu(SendData);

  mu.init(8);

  QueueData queue_data;
  vTaskDelay(500);

  while (1){

    if (millis() - lasttime > frequency){
      
      xQueueReceive(main_TO_MuQueue,&queue_data,0);

      // for (int i = 0; i < queue_data.len; i++){
      //   Serial.printf("%d ",queue_data.Mudata[i]);
      // }
      // Serial.print("\n");

      //送信
      mu.send(queue_data.Mudata,queue_data.len);
      if (memcmp(lastconfig,queue_data.config,sizeof(queue_data.config)) != 0){
        mu.setParams(queue_data.config[GroupID],queue_data.config[Channel],queue_data.config[TargetID],queue_data.config[DeviceID]);
        memcpy(&lastconfig, &queue_data.config, sizeof(queue_data.config));
      }
      
      lasttime = millis();
    }
    
    vTaskDelay(1);

  }

}

//画面タスク
void Display(void *pvParameters){

  #define SCREEN_I2C_ADDR 0x3c // or 0x3C
  #define OLED_RST_PIN -1      // Reset pin (-1 if not available)
  
  bool menu = false;
  int32_t buzzer_last = 0;
  int32_t lasttime = 0;
  int32_t wiilasttime = 0;
  int8_t page = 0;

  buzzer::init();
  uint8_t player_index = 255;
  
  String menu_items[8] = {
    "userid",
    "groupid",
    "deviceid",
    "targetid",
    "channel" ,
    "mode",
    "frequency",
    "initial"
  };
  int16_t config_items[8][45] = {
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44},
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46},
    {5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5, 12, 5},
    {10, 20, 40, 60, 80, 100, 1000, 10, 20, 40, 60, 80, 100, 1000, 10, 20, 40, 60, 80, 100, 1000, 10, 20, 40, 60, 80, 100, 1000, 10, 20, 40, 60, 80, 100, 1000, 10, 20, 40, 60, 80, 100, 1000, 10, 20, 40},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}
};



  ConfigData result_config;
  int8_t default_config[8] ={0,3,0,1,0,0,0,0};
  int8_t config[8];
  EEPROM_data eeprom_data;
  EEPROM.begin(sizeof(EEPROM)+20U);
  EEPROM.get(0, eeprom_data);
  
  // バージョンをチェック
  if (strncmp(eeprom_data.check, DATA_VERSION, 8)) {
    // 保存データが無い場合デフォルトを設定
    memcpy(eeprom_data.config, default_config, sizeof(default_config));
    strncpy(eeprom_data.check, DATA_VERSION, 8); // バージョン情報を設定
    EEPROM.put(0, eeprom_data); // デフォルトデータをEEPROMに保存
  }

  // 設定データをconfigにコピー
  memcpy(config, eeprom_data.config, sizeof(config));
  Serial.printf("UI = %02x\nGI = %02x\nEI = %02x\nDI = %02x\nCH = %02x\nMODE = %02x\nFrequency = %02x\n",config_items[UserID][config[0]],config_items[GroupID][config[1]],config_items[DeviceID][config[2]]
        ,config_items[TargetID][config[3]],config_items[Channel][config[4]],config_items[Mode][config[5]],config_items[Frequency][config[6]]);
  
  for (int i = 0;i < 8;i++){
    result_config.configdata[i] = config_items[i][config[i]];
  }

  frequency = config_items[Frequency][config[Frequency]];
        
  xQueueOverwrite(config_TO_mainQueue,&result_config);

  int8_t select_menu_count = 0;
  
  //コントローラー
  controller::ControllerData controllerdata[2] ;
  WiiClassic wii(Wire);
  WiiClassic wii1(Wire1);
  
  Adafruit_SSD1306 display(128, 64, &Wire, OLED_RST_PIN);
  Wire.setPins(OLED_SDA, OLED_SCL); 
  Wire1.setPins(P2_SDA,P2_SCL);
  Wire.begin();
  Wire1.begin();
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR);
  

  wii.init();
  wii1.init();

  ButtonManager btn;
  btn.add(SW7,20);
  btn.add(SW6,20);
  btn.add(SW5,20);
  btn.add(SW4,20);
  btn.add(Emergency,20);

  enum btn_name{
    FRONT_BTNA = 0,//SW7
    FRONT_BTNB = 1,//SW6
    FRONT_BTNC = 2,//SW5
    FRONT_BTND = 3,//SW4
    EMERGENCY_BTN = 4
  };


  while (1){
    btn.update();
    buzzer::update();
    
    if (millis() - lasttime  > 100){
      display.clearDisplay();
      if (btn.isHold(EMERGENCY_BTN) == true){
        buzzer::stopPlayer(player_index);
        player_index = 255;

        if (menu == false){
          display.setTextSize(2);               //フォントサイズは2(番目に小さい)
          display.setTextColor(SSD1306_WHITE);  //色指定はできないが必要
          display.setCursor(0, 0);            //テキストの表示開始位置
          display.print("CH");         //表示文字列
          display.setCursor(0, 25);
          display.printf("%02d",config_items[Channel][config[4]]);

        }else if (menu == true){


          display.setTextSize(1);               //フォントサイズは2(番目に小さい)
          display.setTextColor(SSD1306_WHITE);  //色指定はできないが必要
          display.setCursor(0, 0); 
          display.print("MENU");
          display.drawLine(0,10,128,10,WHITE);

          for (int i =page*5; i < 8; i++){

            display.setCursor(2, 10*(i-(page*5)+1)+1);

            if (i == select_menu_count){
              display.fillRect(0, 10*(i-(page*5)+1), 52, 10, WHITE);
              display.setTextColor(INVERSE);  //色指定はできないが必要
              display.print(menu_items[i]);

            }else{
              display.setTextColor(SSD1306_WHITE);  
              display.print(menu_items[i]);
              
            }

            display.fillRect(98, 10*(i-(page*5)+1), 28, 10, WHITE);
            display.setTextColor(SSD1306_INVERSE);
            display.setCursor(100, 10*(i-(page*5)+1)+1);
            display.printf("%02d",config_items[i][config[i]]);

            display.drawLine(0,10*(i-(page*5)+2),128,10*(i-(page*5)+2),WHITE);
          }
        }
      }else{
        
        
        if (player_index == 255){
          player_index = buzzer::play(buzzer_score::potato,3,10,true);
        }
        display.setTextSize(2);     
        display.setCursor(10, 15);
        display.fillRect(0, 0, 128, 64, WHITE);
        display.setTextColor(SSD1306_INVERSE);
        display.printf("EMERGENCY\n    BTN");
      }

      

      lasttime = millis();
      display.display();
    }

    if (millis() - wiilasttime  > 15){
      if (wii.update(controllerdata[0]) == true){
        xQueueOverwrite(controller_TO_mainQueue,&controllerdata[0]);
      }

      // if (wii1.update(controllerdata[1]) == true){
      //   xQueueOverwrite(controller1_TO_mainQueue,&controllerdata[1]);
      // }

      wiilasttime = millis();
    }

    
    

    if (btn.isPressed(FRONT_BTNA)){
      buzzer::buzz(442,100,255U);
      menu = !menu;
      if (menu == false){

        if(config_items[Initial][config[Initial]] == 1){
          memcpy(config, default_config, sizeof(default_config));
        }

        // Serial.printf("UI = %02x\nGI = %02x\nEI = %02x\nDI = %02x\nCH = %02x\nMODE = %02x\nFrequency = %02x\n",config_items[UserID][config[0]],config_items[GroupID][config[1]],config_items[DeviceID][config[2]]
        // ,config_items[TargetID][config[3]],config_items[Channel][config[4]],config_items[Mode][config[5]],config_items[Frequency][config[6]]);


        // EEPROMに設定を保存する。
        strcpy(eeprom_data.check, DATA_VERSION);
        memcpy(eeprom_data.config, config, sizeof(config));
        EEPROM.put(0, eeprom_data);
        EEPROM.commit();   //EEPROMに書き込み

        for (int i = 0;i < 8;i++){
          result_config.configdata[i] = config_items[i][config[i]];
        }

        frequency = config_items[Frequency][config[Frequency]];
        
        xQueueOverwrite(config_TO_mainQueue,&result_config);
      }


    }
    if (menu == true){
      if (btn.isPressed(FRONT_BTNB)){
        buzzer::buzz(442,100,255U);
        select_menu_count++;
        select_menu_count = select_menu_count % 8;
        page = select_menu_count/5;

      }
      if (btn.isPressed(FRONT_BTNC)){
        buzzer::buzz(442,100,255U);
        config[select_menu_count]--;
        if (config[select_menu_count] < 0){
          config[select_menu_count] = 44;
        }

      }
      if (btn.isPressed(FRONT_BTND)){
        buzzer::buzz(442,100,255U);
        config[select_menu_count]++;
        config[select_menu_count] = config[select_menu_count] % 45;
      }
    }

    
    btn.release();
    vTaskDelay(1);

  }
}

void setup() {
  
//Queueを作ってからタスクを召喚する
  controller_TO_mainQueue = xQueueCreate(1,sizeof(controller::ControllerData));
  controller1_TO_mainQueue = xQueueCreate(1,sizeof(controller::ControllerData));
  config_TO_mainQueue = xQueueCreate(1,sizeof(ConfigData));
  main_TO_MuQueue = xQueueCreate(1,sizeof(QueueData));


  xTaskCreateUniversal(main_task,"main", 8192, NULL, 2, &Main_Handle, CONFIG_ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(Mu,"Mu", 8192, NULL, 2, &Mu_Handle, CONFIG_ARDUINO_RUNNING_CORE);
  xTaskCreateUniversal(Display,"Display", 8192, NULL, 2, &Display_Handle, CONFIG_ARDUINO_RUNNING_CORE);

}

void loop() {
}