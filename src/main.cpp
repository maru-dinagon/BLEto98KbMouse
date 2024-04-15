#include <Arduino.h>
#include <NimBLEDevice.h>

// UUID HID
static NimBLEUUID serviceUUID("1812");
// UUID Report Charcteristic
static NimBLEUUID charUUID("2a4d");

static NimBLEAdvertisedDevice* advDevice;

static bool doConnect = false;
static uint32_t scanTime = 0; // 0でずっとスキャン

// スキャン終了時に呼ばれるコールバック
void scanEndedCB(NimBLEScanResults results){
    Serial.println("Scan Ended");
}

// これらはデフォルトでライブラリによって処理されるため、いずれも必要ない。 必要に応じて削除してください。 
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        //接続後、高速な応答時間が必要ない場合は、パラメータを変更する必要があります。
        //これらの設定は、インターバル150ms、レイテンシ0、タイムアウト450ms
        //タイムアウトはインターバルの倍数で、最小は100msです。
        //タイムアウトはインターバルの3～5倍が素早いレスポンスと再接続に最適です。
        //最小間隔：120 * 1.25ms = 150、最大間隔：120 * 1.25ms = 150、待ち時間0、タイムアウト60 * 10ms = 600ms
        //pClient->updateConnParams(120,120,0,60);
        pClient->updateConnParams(30,30,0,60);
    };

    void onDisconnect(NimBLEClient* pClient) {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(scanTime, scanEndedCB);
    };

    //ペリフェラルが接続パラメータの変更を要求したときに呼び出される。
    //受諾して適用する場合はtrueを返し、拒否して維持する場合はfalseを返します。
    //デフォルトはtrueを返します。
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        Serial.println("Call onConnParamsUpdateRequest");
        Serial.printf("itvl_min= %u, itvl_max= %u, latency= %u, supervision_timeout= %u",params->itvl_min,params->itvl_max,params->latency,params->supervision_timeout);
        Serial.println("");
        pClient->updateConnParams(params->itvl_min,params->itvl_max,params->latency,params->supervision_timeout);

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
        if(!desc->sec_state.encrypted) {
            Serial.println("Encrypt connection failed - disconnecting");
            //descで指定された接続ハンドルを持つクライアントを検索し切断
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};
//ClientCallbacksのインスタンス
static ClientCallbacks clientCB;


//アドバタイズ情報の取得時コールバック
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {

  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    Serial.print("Advertised Device found: ");
    Serial.printf("name:%s,address:%s", advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
    Serial.printf(" UUID:%s\n", advertisedDevice->haveServiceUUID() ? advertisedDevice->getServiceUUID().toString().c_str() : "none");

    //adressが一致するかどうかdd:77:54:67:9b:76
    if((advertisedDevice->getAddress().toString().compare("dc:00:a8:2c:37:00") == 0) ||
      (advertisedDevice->getAddress().toString().compare("1d:77:54:69:9d:0d") == 0))
    {
      Serial.println("アドレス一致");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
      /** Ready to connect now */
      doConnect = true;
    }
    
       
    // HID UUIDかチェックして正しければスキャンをストップ
    if (advertisedDevice->isAdvertisingService(serviceUUID)) {
      Serial.println("Found Our Service");
      // stop scan before connecting
      NimBLEDevice::getScan()->stop();
      // Save the device reference in a global for the client to use
      advDevice = advertisedDevice;
      // Ready to connect now
      doConnect = true;
    }
    
  }
};



//通知(Notify)／表示(Indicate)の受信時コールバック
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify){
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
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
      // 複数のCharacterisiticsを取得(リフレッシュtrue)
      pChrs = pSvc->getCharacteristics(true);
    }

    if (pChrs) {
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
          //if (!pChrs->at(i)->registerForNotify(notifyCB)) {
          if(!pChrs->at(i)->subscribe(true, notifyCB)) {
            //コールバック登録(サブスクライブ)失敗した場合は切断する
            pClient->disconnect();
            return false;
          }
        } else
        //Indicate属性(Noitfyとの違いは接続元[ESP32]からの応答を要求する)
        if (pChrs->at(i)->canIndicate()) {
          if(!pChrs->at(i)->subscribe(false, notifyCB)) {
            pClient->disconnect();
            return false;
          }        
        }
      }
    }

    Serial.println("Done with this device!");
    return true;

}


void setup (){
    Serial.begin(115200);
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
    while(!doConnect){
        delay(1);
    }

    doConnect = false;

    //接続先があれば接続する
    if(connectToServer()) {
        Serial.println("Success! we should now be getting notifications, scanning for more!");
    } else {
        Serial.println("Failed to connect, starting scan");
    }

    //スキャン再開
    NimBLEDevice::getScan()->start(scanTime,scanEndedCB);}
