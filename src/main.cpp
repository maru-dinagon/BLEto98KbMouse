#include <Arduino.h>
#include <NimBLEDevice.h>
#include "LittleFS.h"

#include "Pc98BLEMouseReportParser.hpp"
#include "Pc98BLEKbdRptParser.hpp"

//通知デバック用
//#define NOTIFY_DEBUG 
//アドバタイズデバック用
//#define ADVERTISE_DEV_DEBUG

#define DO_NOT_CONNECT   0
#define DO_CONNECT_MOUSE 1
#define DO_CONNECT_KB    2
static int doConnect = DO_NOT_CONNECT;

//アドバタイジングされ、接続に利用する為のデバイスの保持
static NimBLEAdvertisedDevice* advDevice;

//キーボード・マウスパーサー
static Pc98BLEMouseReportParser mouse_rpt_parser;
static Pc98BLEKbdRptParser kb_rpt_parser;

// UUID HID
static NimBLEUUID serviceUUID("1812");
// UUID Report Charcteristic
static NimBLEUUID charUUID("2a4d");

//現在接続しているアドレスの保持
NimBLEAddress Mouse_Ad,Kb_Ad;

//ペアリング時に通知されるマウス名(一部でもOK) 要事前調査
const char* MOUSE_NAME_WORD = "shellpha";
//ペアリング時に通知されるキーボード名(一部でもOK) 要事前調査
const char* KB_NAME_WORD = "BT FullKey";

//LittleFSが使えるかどうかの保持
static bool isEnable_fs;
//ペアリング後デバイスアドレスをLittleFSに保存するファイル名
static String MOUSE_AD_FILE = "/mouse.txt";
static String KB_AD_FILE = "/kb.txt";

//updateConnParams通知内容保存用定義(xxxx_F + xxx_EXTのファイル名でint値の保存)
static String ITVL_MIN_F = "/itvl_min";
static String ITVL_MAX_F = "/itvl_max";
static String LATENCY_F  = "/latency";
static String TIMEOUT_F  = "/timeout";

static String MOUSE_EXT  = ".mouse";
static String KB_EXT     = ".kb";


//マウスとキーボードの接続状態
static bool ConnectMouse = false;
static bool ConnectKB    = false;

static uint32_t scanTime = 0; // 0でずっとスキャン

//LittleFSへのint値の保存
void saveValue(String F_Name,int value){
    if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return;
    }
    File dataFile = LittleFS.open(F_Name, "w");
    dataFile.println(String(value).c_str());
    dataFile.close();  
}

//LittleFSへのint値の読み出し
int loadValue(String F_Name,int def_value){
    if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return def_value;
    }
    if(!LittleFS.exists(F_Name)) return def_value;
    File dataFile = LittleFS.open(F_Name, "r");
    String buf = dataFile.readStringUntil('\n');
    dataFile.close();
    buf.trim(); //改行除去
    if(buf.compareTo("0")==0){
      return 0;
    }else{
      int ret = buf.toInt();
      if(ret==0){
        return def_value;
      }else{
        return ret;
      }
    }
}

//LittleFSへのマウスアドレスの保存・ロード
void saveMouseAd(NimBLEAddress ad){
      if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return;
    }
    File dataFile = LittleFS.open(MOUSE_AD_FILE, "w");
    dataFile.println(ad.toString().c_str());
    dataFile.close();
}

NimBLEAddress getMouseAd(){
    if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return NimBLEAddress();
    }
    if(!LittleFS.exists(MOUSE_AD_FILE)) return NimBLEAddress();
    File dataFile = LittleFS.open(MOUSE_AD_FILE, "r");
    String buf = dataFile.readStringUntil('\n');
    dataFile.close();
    buf.trim(); //改行除去
    //Serial.println("buf=" + buf);
    return NimBLEAddress(buf.c_str());
}

//LittleFSへのキーボードアドレスの保存・ロード
void saveKbAd(NimBLEAddress ad){
      if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return;
    }
    File dataFile = LittleFS.open(KB_AD_FILE, "w");
    dataFile.println(ad.toString().c_str());
    dataFile.close();
}

NimBLEAddress getKbAd(){
    if(!isEnable_fs){
      Serial.println("LittleFS is unable");
      return NimBLEAddress();
    }
    if(!LittleFS.exists(KB_AD_FILE)) return NimBLEAddress();
    File dataFile = LittleFS.open(KB_AD_FILE, "r");
    String buf = dataFile.readStringUntil('\n');
    dataFile.close();
    buf.trim(); //改行除去
    //Serial.println("buf=" + buf);
    return NimBLEAddress(buf.c_str());;
}

// スキャン終了時に呼ばれるコールバック
void scanEndedCB(NimBLEScanResults results){
    Serial.println("Scan Ended");
}

// クライアント(ペリフェラル)からのコールバック 
class ClientCallbacks : public NimBLEClientCallbacks {
    
    void onConnect(NimBLEClient* pClient) {
      if(pClient->getPeerAddress().equals(getMouseAd())){
        //登録済マウスと接続
        Serial.println("Known Mouse Connected");
        int itvl_min = loadValue(ITVL_MIN_F + MOUSE_EXT,-1);
        int itvl_max = loadValue(ITVL_MAX_F + MOUSE_EXT,-1);
        int latency  = loadValue(LATENCY_F  + MOUSE_EXT,-1);
        int time_out = loadValue(TIMEOUT_F  + MOUSE_EXT,-1);        
        Serial.printf("itvl_min = %d, itvl_max = %d, latency = %d, timeout = %d\n",itvl_min,itvl_max,latency,time_out);
        if(itvl_min>=0 && itvl_max>=0 && latency>=0 && time_out>=0){
          Serial.println("updateConnParams called : Save Value");
          pClient->updateConnParams(itvl_min,itvl_max,latency,time_out);
        }else{
          Serial.println("updateConnParams called : Def Value");
          pClient->updateConnParams(120,120,0,60);
        }
        Mouse_Ad = pClient->getPeerAddress();
        
      }else
      if(pClient->getPeerAddress().equals(getKbAd())){
        //登録済キーボードと接続
        Serial.println("Known Keyboard Connected");
        int itvl_min = loadValue(ITVL_MIN_F + KB_EXT,-1);
        int itvl_max = loadValue(ITVL_MAX_F + KB_EXT,-1);
        int latency  = loadValue(LATENCY_F  + KB_EXT,-1);
        int time_out = loadValue(TIMEOUT_F  + KB_EXT,-1);        
        Serial.printf("itvl_min = %d, itvl_max = %d, latency = %d, timeout = %d\n",itvl_min,itvl_max,latency,time_out);
        if(itvl_min>=0 && itvl_max>=0 && latency>=0 && time_out>=0){
          Serial.println("updateConnParams called : Save Value");
          pClient->updateConnParams(itvl_min,itvl_max,latency,time_out);
        }else{
          Serial.println("updateConnParams called : Def Value");
          pClient->updateConnParams(120,120,0,60);
        }
        Kb_Ad = pClient->getPeerAddress();
      }else{
        //新規デバイスと接続
        if(doConnect == DO_CONNECT_MOUSE){
          //新規マウスと接続(初回ペアリング)
          Serial.println("New Mouse Connected");
          //pClient->updateConnParams(6,6,23,200); //事前調査値
          Mouse_Ad = pClient->getPeerAddress();
          saveMouseAd(Mouse_Ad); //正常接続したマウスのアドレスの保持

        }else
        if(doConnect == DO_CONNECT_KB){
          //新規キーボードと接続(初回ペアリング)
          Serial.println("New Keyboard Connected");
          //pClient->updateConnParams(7,7,0,300); //事前調査値
          Kb_Ad = pClient->getPeerAddress();
          saveKbAd(Kb_Ad); //正常接続したキーボードのアドレスの保持

        }else{
          //不明デバイスと接続
          Serial.println("Unkown Device Connected");
          pClient->updateConnParams(120,120,0,60);
        }
      }

      //接続後、高速な応答時間が必要ない場合は、パラメータを変更する必要があります。
      //これらの設定は、インターバル150ms、レイテンシ0、タイムアウト450ms
      //タイムアウトはインターバルの倍数で、最小は100msです。
      //タイムアウトはインターバルの3～5倍が素早いレスポンスと再接続に最適です。
      //最小間隔：120 * 1.25ms = 150、最大間隔：120 * 1.25ms = 150、待ち時間0、タイムアウト60 * 10ms = 600ms
      pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
      Serial.print(pClient->getPeerAddress().toString().c_str());
      Serial.println(" Disconnected");
      if(pClient->getPeerAddress().equals(getMouseAd())){
        //マウスとの接続が切断
        ConnectMouse = false;
        Serial.println("Disconnected Mouse Device - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
      }
      if(pClient->getPeerAddress().equals(getKbAd())){
        //キーボードとの接続が切断
        ConnectKB = false;
        Serial.println("Disconnected Keyboard Device - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
      }
    };

    //ペリフェラルが接続パラメータの変更を要求したときに呼び出される。
    //受諾して適用する場合はtrueを返し、拒否して維持する場合はfalseを返します。
    //デフォルトはtrueを返します。
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        Serial.println("Call onConnParamsUpdateRequest");
        Serial.printf("itvl_min= %u, itvl_max= %u, latency= %u, supervision_timeout= %u",params->itvl_min,params->itvl_max,params->latency,params->supervision_timeout);
        Serial.println("");
        pClient->updateConnParams(params->itvl_min,params->itvl_max,params->latency,params->supervision_timeout);

        if(pClient->getPeerAddress().equals(getMouseAd())){
          //マウスからの要求を保存
          saveValue(ITVL_MIN_F + MOUSE_EXT,params->itvl_min);
          saveValue(ITVL_MAX_F + MOUSE_EXT,params->itvl_max);
          saveValue(LATENCY_F  + MOUSE_EXT,params->latency);
          saveValue(TIMEOUT_F  + MOUSE_EXT,params->supervision_timeout);
        }
        if(pClient->getPeerAddress().equals(getKbAd())){
          //キーボードからの要求を保存
          saveValue(ITVL_MIN_F + KB_EXT,params->itvl_min);
          saveValue(ITVL_MAX_F + KB_EXT,params->itvl_max);
          saveValue(LATENCY_F  + KB_EXT,params->latency);
          saveValue(TIMEOUT_F  + KB_EXT,params->supervision_timeout);
        }

        /*
        if(params->itvl_min < 24) { //1.25ms単位
            return false;
        } else if(params->itvl_max > 40) { //1.25ms単位
            return false;
        } else if(params->latency > 2) { //スキップ可能なインターバルの数
            return false;
        } else if(params->supervision_timeout > 100) { //10ms単位
            return false;
        }
        */

        return true;
    };

    //********************* セキュリティはここで対応 **********************
    //****** 注：これらはデフォルトと同じ戻り値です。 ********
    uint32_t onPassKeyRequest(){
        Serial.println("Client Passkey Request");
        //サーバーに送信するパスキーを返す
        return 123456;
    };

    bool onConfirmPIN(uint32_t pass_key){
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
        //パスキーが一致しない場合はfalseを返す
        return true;
    };

    //ペアリングが完了し、ble_gap_conn_descで結果を確認可能
    void onAuthenticationComplete(ble_gap_conn_desc* desc){
        Serial.println("Call onAuthenticationComplete");
        if(!desc->sec_state.encrypted) {
            Serial.println("Encrypt connection failed - disconnecting");
            //descで指定された接続ハンドルを持つクライアントを検索し切断
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
        /*
        //ここでのitvl,latency,timeoutはonConnParamsUpdateRequestと異なることがある…
        Serial.printf("ble_gap_conn_desc : itvl = %d, latency = %d, timeout = %d\n ",desc->conn_itvl,desc->conn_latency,desc->supervision_timeout);
        */

    };
};
//ClientCallbacksのインスタンス
static ClientCallbacks clientCB;


//アドバタイズ情報の取得時コールバック
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
#ifdef ADVERTISE_DEV_DEBUG    
    Serial.print("Advertised Device found: ");
    Serial.printf("name:%s,address:%s", advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
    Serial.printf(" UUID:%s\n", advertisedDevice->haveServiceUUID() ? advertisedDevice->getServiceUUID().toString().c_str() : "none");
#endif
    //ペアリング済みマウスとaddressが一致するかどうか
    if(advertisedDevice->getAddress().equals(getMouseAd())){
      Serial.println("Match Known Mouse Address");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = DO_CONNECT_MOUSE;
      return;
    }
    
    //ペアリング済みキーボードとaddressが一致するかどうか
    if(advertisedDevice->getAddress().equals(getKbAd())){
      Serial.println("Match Konwn Keyboard Address");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = DO_CONNECT_KB;
      return;
    }
       
    // 新規のマウス・キーボードのペアリング HID UUIDかチェックしてペアリングの検出
    if (advertisedDevice->isAdvertisingService(serviceUUID)) {
      Serial.println("Found HID Service");
      
      //デバイス名でマウス・キーボードの接続をコントロール
      const char *ad_name = advertisedDevice->getName().c_str();

      //新規マウスからのペアリング要求
      char *mouse_f = strstr(ad_name, MOUSE_NAME_WORD);
      if(mouse_f != nullptr){
        Serial.println("Found Mouse Name as Unkown Address");
        //saveMouseAd(advertisedDevice->getAddress());
        //スキャンの停止
        NimBLEDevice::getScan()->stop();
        //グローバルインスタンスへ渡す
        advDevice = advertisedDevice;
        // Ready to connect now
        doConnect = DO_CONNECT_MOUSE;
        return;
      }

      //新規キーボードからのペアリング要求
      char *kb_f = strstr(ad_name, KB_NAME_WORD);
      if(kb_f != nullptr){
        Serial.println("Found Keyboard Name as as Unkown Address");
        //saveKbAd(advertisedDevice->getAddress());
        //スキャンの停止
        NimBLEDevice::getScan()->stop();
        //グローバルインスタンスへ渡す
        advDevice = advertisedDevice;
        // Ready to connect now
        doConnect = DO_CONNECT_KB;
        return;
      }

      Serial.println("UnKown HID Device! Refuse Connection");
      /*
      // stop scan before connecting
      NimBLEDevice::getScan()->stop();
      // Save the device reference in a global for the client to use
      advDevice = advertisedDevice;
      // Ready to connect now
      doConnect = true;
      */
    }
    
  }
};



//通知(Notify)／表示(Indicate)の受信時コールバック
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){

#ifdef NOTIFY_DEBUG
  //ここでは文字列処理・ログ出力などの重い処理は遅延の原因となるため注意
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    // NimBLEAddress and NimBLEUUID have std::string operators
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    char st[20];
    sprintf(st, ",length = %zd",length);
    str += std::string(st);
    //str += ", Value = " + std::string((char*)pData, length);
    
    //Hex出力
    std::string buf = "";
    for(int i=0; i< length ; i++){
      sprintf(st,"%02x ",pData[i]);
      buf += std::string(st);   
    }
    str += ", Value = " + buf;
    Serial.println(str.c_str());
#endif

    //Notifyのキャラクテリスティクを処理
    if(pRemoteCharacteristic->getUUID().equals(charUUID)){
      
      //マウスからの通知
      if(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().equals(Mouse_Ad)){
        mouse_rpt_parser.Parse(length,pData);
        return;
      }

      //キーボードからの通知
      if(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress().equals(Kb_Ad)){
        kb_rpt_parser.Parse(length,pData);
        return;
      }

    }

}

//クライアントのプロビジョニングを処理とサーバーとの接続/インターフェースを行う。
bool connectToServer() {
    NimBLEClient* pClient = nullptr;

    //再利用できるクライアントの検索
    if(NimBLEDevice::getClientListSize()) {
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if(pClient){
          //このデバイスをすでに知っている場合は、connect()の第2引数にfalseセットすることで
          //サービスデータベースを更新しないようにする。これにより、かなりの時間と電力を節約できる
          if(!pClient->connect(advDevice, false)) {
              Serial.println("Reconnect failed");
              return false;
          }
          Serial.println("Reconnected client");
        }else {
          //このデバイスを知っているクライアントがまだない場合、使用できる切断中のクライアントを調べる
          pClient = NimBLEDevice::getDisconnectedClient();
        }
    }

    //再利用するクライアントがない場合、新しいクライアントを作成する
    if(!pClient) {
        if(NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.println("New client created");

        pClient->setClientCallbacks(&clientCB, false);
        
        //初期接続パラメータの設定： これらの設定は、15ms間隔、0ms待ち時間、120msタイムアウト
        //これらの設定は、3クライアントが確実に接続できる安全な設定です。タイムアウトはインターバルの倍数で、最小は100ms
        //最小間隔：12 * 1.25ms = 15ms、最大間隔：12 * 1.25ms = 15ms、待ち時間 0ms、タイムアウト51 * 10ms = 510ms
        pClient->setConnectionParams(12,12,0,51);

        //接続が完了するまでの待機時間（秒）を設定。デフォルトは30。
        pClient->setConnectTimeout(5);


        if (!pClient->connect(advDevice)) {
            //クライアントを作成したが、接続に失敗
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }

    if(!pClient->isConnected()) {
        if (!pClient->connect(advDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }

    //接続完了
    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());


    NimBLERemoteService *pSvc = nullptr;
    
    //複数扱うためにvectorを使う
    std::vector<NimBLERemoteCharacteristic*> *pChrs = nullptr;
    NimBLERemoteDescriptor *pDsc = nullptr;

    //HIDサービスを取得する
    pSvc = pClient->getService(serviceUUID);
    if (pSvc) { 
      //Serial.println("pSvc : HIDサービス取得成功");
      // 複数のCharacterisiticsを取得(リフレッシュtrue)
      pChrs = pSvc->getCharacteristics(true);
    }else{
      //Serial.println("pSvc : HIDサービス取得失敗");
    }

    if (pChrs) {
      //Serial.println("pChrs : Characterisitics取得成功");
      // 複数のReport Characterisiticsの中からNotify属性を持っているものをCallbackに登録する
      for (int i = 0; i < pChrs->size(); i++) {
        /*  
        if(pChrs->at(i)) {
          //Read属性
          if(pChrs->at(i)->canRead()) {
              Serial.print("Read : ");
              Serial.print(pChrs->at(i)->getUUID().toString().c_str());
              //Serial.print(" Value: ");

              //Hex出力
              char st[100];
              std::string buf = "";
              for(int i=0; i< strlen(pChrs->at(i)->readValue().c_str()) ; i++){
                sprintf(st,"%02x ",pChrs->at(i)->readValue().c_str()[i]);
                buf += std::string(st);   
              }
              Serial.print(" Value = ");
              Serial.println(buf.c_str());
              //Serial.println(pChrs->at(i)->readValue().c_str());
          }

          //Write属性
          if(pChrs->at(i)->canWrite()) {
              Serial.println("Write : ");
              
              if(pChrs->at(i)->writeValue("Tasty")) {
                  Serial.print("Wrote new value to: ");
                  Serial.println(pChrs->at(i)->getUUID().toString().c_str());
              }else {
                  pClient->disconnect();
                  return false;
              }
              if(pChrs->at(i)->canRead()) {
                  Serial.print("The value of: ");
                  Serial.print(pChrs->at(i)->getUUID().toString().c_str());
                  Serial.print(" is now: ");
                  Serial.println(pChrs->at(i)->readValue().c_str());
              }
              
          }
        }
        */
        //registerForNotify()は非推奨となり、subscribe()/unsubscribe()に置き換えられた
        // Subscribeパラメータのデフォルトは、notifications=true、notifyCallback=nullptr、response=false
        //接続機器からのcharacteristic変更を通知する属性はNotifyとIndicateのみでコールバックを登録する
        
        //Notify属性
        if (pChrs->at(i)->canNotify()) {
          //Serial.printf("SetNotify UUID: %s\n",pChrs->at(i)->getUUID().toString().c_str());
          //if (!pChrs->at(i)->registerForNotify(notifyCB)) {
          if(!pChrs->at(i)->subscribe(true, notifyCB,true)) {
            //コールバック登録(サブスクライブ)失敗した場合は切断する
            pClient->disconnect();
            return false;
          }
        } else
        //Indicate属性(Noitfyとの違いは接続元[ESP32]からの応答を要求する)
        if (pChrs->at(i)->canIndicate()) {
          if(!pChrs->at(i)->subscribe(false, notifyCB,true)) {
            pClient->disconnect();
            return false;
          }        
        }
      }
    }else{
      //Serial.println("pChrs : Characterisitics取得失敗");
    }

    Serial.println("Done with this device!");
    return true;

}

//キーボードタスク用スレッド
TaskHandle_t thp[1];
void Core1_KbdTask(void *args) {
  while (1) {
    delay(1);
    kb_rpt_parser.task();
  }
}

void setup (){
    Serial.begin(115200);

    //キーボードパーサー初期化
    kb_rpt_parser.setUp98Keyboard();

    //キーボート通信処理用タスク(起動直後のパケット送受信に間に合わせるためCore1でマルチタスク化)
    //Core0はBLEで使っておりnotifyCBが激重になるのでダメ
    xTaskCreatePinnedToCore(Core1_KbdTask, "Core1_KbdTask", 4096, NULL, 24, &thp[0], 1); 
   
    //マウスパーサー初期化
    mouse_rpt_parser.setUpBusMouse();
 
    //ファイルシステム初期化
    if(LittleFS.begin()){
      Serial.println("LittleFS was started");
      isEnable_fs = true;
    }else{
      Serial.println("LittleFS was NOT started");
      isEnable_fs = false;
    }

    //NimBLE初期化とアドバタイズ機器の検索開始
    Serial.println("Starting NimBLE Client");
    NimBLEDevice::init("");
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_SC);

    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

    NimBLEScan* pScan = NimBLEDevice::getScan();

    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setInterval(45);
    pScan->setWindow(15);
    pScan->setActiveScan(true);
    pScan->start(scanTime, scanEndedCB);
}


void loop() {
    //アドバタイズコールバックで接続先がなければループ
    if(doConnect == DO_NOT_CONNECT){
      delay(1);
      return; 
    }

    //接続先があれば接続する
    if(connectToServer()) {
        Serial.println("Success! we should now be getting notifications");
        if(doConnect == DO_CONNECT_MOUSE){
          ConnectMouse = true;
        }
        if(doConnect == DO_CONNECT_KB){
          ConnectKB = true;
        }
    } else {
        Serial.println("Failed to connect, starting scan");
        if(doConnect == DO_CONNECT_MOUSE){
          ConnectMouse = false;
        }
        if(doConnect == DO_CONNECT_KB){
          ConnectKB = false;
        }
    }
    
    doConnect = DO_NOT_CONNECT;

    //マウスかキーボードが接続されてなければアドバタイズ機器のスキャン再開
    if((!ConnectMouse) || (!ConnectKB))
      NimBLEDevice::getScan()->start(scanTime,scanEndedCB);
    }

