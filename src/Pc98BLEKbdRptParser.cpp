#include "Pc98BLEKbdRptParser.hpp"

//パーサー パケ長8byte hidboot.cpp KeyboardReportParser::Parse部移植
void Pc98BLEKbdRptParser::Parse(uint8_t len __attribute__((unused)), uint8_t *buf) {
 
        // On error - return
        if (buf[2] == 1)
                return;

        //KBDINFO       *pki = (KBDINFO*)buf;

        // provide event for changed control key state
        if (prevState.bInfo[0x00] != buf[0x00]) {
                OnControlKeysChanged(prevState.bInfo[0x00], buf[0x00]);
        }

        for (uint8_t i = 2; i < 8; i++) {
                bool down = false;
                bool up = false;

                for (uint8_t j = 2; j < 8; j++) {
                        if (buf[i] == prevState.bInfo[j] && buf[i] != 1)
                                down = true;
                        if (buf[j] == prevState.bInfo[i] && prevState.bInfo[i] != 1)
                                up = true;
                }
                if (!down) {
                        //HandleLockingKeys(hid, buf[i]);
                        OnKeyDown(*buf, buf[i]);
                }
                if (!up)
                        OnKeyUp(prevState.bInfo[0], prevState.bInfo[i]);
        }
        for (uint8_t i = 0; i < 8; i++)
                prevState.bInfo[i] = buf[i];
};


boot_mode Pc98BLEKbdRptParser::getBootMode(){
  //アナログボタンアレイが接続されている電圧を計測する(5回サンプリング平均)
  int sample = 5;
  int vol = 0;
  for(int i = 0 ; i < sample ; i++){
    vol += analogReadMilliVolts(BUTTON_ARRAY_ADC_PIN);
  }
  vol = vol / sample;


#ifdef KEY_B_DEBUG
  //Serial.print(vol);
  //Serial.println(" mV");  
#endif

  if(vol > 3000){
    //押していない(実測3181mv)
    //Serial.println("No Push");
    return NORMAL;
  }else
  if(vol > 2500){
    //ボタン1(2880mv)
    //Serial.println("Button1 Pushed");
    return SETUP_MENU;
  }else
  if(vol > 2000){
    //ボタン2(2270mv)
    //Serial.println("Button2 Pushed");
    return CRT_24;
  }else
  if(vol > 1500){
    //ボタン3(1800mv)
    //Serial.println("Button3 Pushed");
    return CRT_31;
  }else
  if(vol > 1000){
    //ボタン4(1340mv)
    //Serial.println("Button4 Pushed");
    return BIOS_REV;
  }else
  if(vol > 500){
    //ボタン5(820mv)
    //Serial.println("Button5 Pushed");
    return CPU_MSG;
  }else
  if(vol > 200){
    //ボタン6(300mv)
    //Serial.println("Button6 Pushed");
    return NO_MEMCHK;
  }else{
    //ボタンアレイが接続されていないか接触不良(200mV以下)
    //Serial.println("Not Commect Button Array");
    return NORMAL;    
  }
  
}

void Pc98BLEKbdRptParser::SetLed(){
  digitalWrite(KANA_LED, kana_f ? HIGH : LOW);
  digitalWrite(CAPS_LED, caps_f ? HIGH : LOW);
  digitalWrite(NUM_LED , num_f  ? HIGH : LOW);
}

void Pc98BLEKbdRptParser::setUp98Keyboard(){

  //ブートモードの取得
  pinMode(BUTTON_ARRAY_ADC_PIN, ANALOG);  
  bootmode = getBootMode();

#ifdef KEY_B_DEBUG
  Serial.print("bootmode = ");
  Serial.println(bootmode);
#endif  

  //LEDの設定
  pinMode(KANA_LED,OUTPUT);
  digitalWrite(KANA_LED, LOW);
  pinMode(CAPS_LED,OUTPUT);
  digitalWrite(CAPS_LED, LOW);
  pinMode(NUM_LED,OUTPUT);
  digitalWrite(NUM_LED, LOW);

  setCodeArray();
  
  pinMode(RST, INPUT);
  pinMode(RDY, INPUT);
  //pinMode(RXD, OUTPUT);
  pinMode(RTY, INPUT);

  //ハードウェアシリアル初期化
  //19.2kbps 8ビット 奇数パリティ ストップビット1ビット
  mySerial.begin(19200, SERIAL_8O1,RST, RXD);
}

void Pc98BLEKbdRptParser::task(){
  //受信コマンドの処理
  pc98key_command();
  //キーリピート処理
  if(repeat_func) repeatKey_proc();
  //ブートモードの更新
  bootmode = getBootMode();

  //LED更新
  SetLed(); 
}

void Pc98BLEKbdRptParser::pc98key_send(uint8_t data){

  while(digitalRead(RDY) == HIGH) delayMicroseconds(1); //送信不可より待機
  
  //if(digitalRead(RST) == LOW) delayMicroseconds(30);  //リセット要求
  
  if(digitalRead(RTY) == LOW){ // RTYオンなら直前のキーを再送信する
      mySerial.write(prev);
  }else{           //  現在キーを直前のキーに保存
      mySerial.write(data);
      prev = data;

#ifdef KEY_B_DEBUG
      Serial.print("->Send : ");
      Serial.print(data,HEX);
      Serial.println("");
#endif    
  }

  delayMicroseconds(500);
}

void Pc98BLEKbdRptParser::pc98key_command(void){

  uint8_t c,up_c; //cの上位4bit保持用

  while(mySerial.available()>0){

    c = mySerial.read();
    up_c = c & 0xf0; //上位4bit

#ifdef KEY_B_DEBUG
      Serial.print("Read : ");
      Serial.print(c,HEX);
      Serial.println("");
#endif

    //ブートモード・コマンドの実行
    if(c == 0xFC && bootmode != NORMAL){
      uint8_t *command;
      int c_size = 0;        
      switch(bootmode){
        case SETUP_MENU:
          command = BOOTMODE_SETUP_MENU;
          c_size  = sizeof(BOOTMODE_SETUP_MENU) / sizeof(BOOTMODE_SETUP_MENU[0]);
          break;

        case CRT_24:
          command = BOOTMODE_CRT_24;
          c_size  = sizeof(BOOTMODE_CRT_24) / sizeof(BOOTMODE_CRT_24[0]);
          break;
          
        case CRT_31:
          command = BOOTMODE_CRT_31;
          c_size  = sizeof(BOOTMODE_CRT_31) / sizeof(BOOTMODE_CRT_31[0]);
          break;

        case BIOS_REV:
          command = BOOTMODE_BIOS_REV;
          c_size  = sizeof(BOOTMODE_BIOS_REV) / sizeof(BOOTMODE_BIOS_REV[0]);
          break;
        
        case CPU_MSG:
          command = BOOTMODE_CPU_MSG;
          c_size  = sizeof(BOOTMODE_CPU_MSG) / sizeof(BOOTMODE_CPU_MSG[0]);
          break;

        case NO_MEMCHK:
          command = BOOTMODE_NO_MEMCHK;
          c_size  = sizeof(BOOTMODE_NO_MEMCHK) / sizeof(BOOTMODE_NO_MEMCHK[0]);
          break;

      }
      //起動時コマンドの送信
      for(int i = 0 ; i < c_size ; i++){
        pc98key_send(command[i]);
        delayMicroseconds(500);
      }
      continue; 
     }

    //コマンドが0x9_以外はリセットかける。 ログでは0xFF,0x7_などが起動時に流れているが委細不明
    if(up_c != 0x90){
      kana_f = 0;
      caps_f = 0;
      //num_f = 0;

      repeat_df = 0;
      repeat_delay_time = 500;
      repeat_speed_time = 40;
      continue;   
    }

    // コマンド処理
    switch(c){
    case 0x95: // Windows,Appliキー制御
        pc98key_send(ACK);

        while(!mySerial.available());
        c = mySerial.read();
        // c=0x00 通常
        // c=0x03 Windowsキー,Applicationキー有効
        pc98key_send(ACK);
        break;
        
    case 0x96: // モード識別　取り敢えず通常モードを送信しておく.
        pc98key_send(ACK);
        delayMicroseconds(500);
        pc98key_send(0xA0);
        delayMicroseconds(500);
        pc98key_send(0x85);   // 0x85=通常変換モード, 0x86=自動変換モード
        break;
        
    case 0x9C:{ // キーボードリピート制御
        pc98key_send(ACK);

        while(!mySerial.available());
        c = mySerial.read();

        //上位4bitと下位4bitを入れ替え
        uint8_t t1 = c;
        uint8_t t2 = c;
        t1 <<= 4;
        t2 >>= 4;
        uint8_t d = t1 | t2;

        //リピートスピード
        uint8_t r_s = (d & 0b11111000) >> 3;
        if(r_s == 0){
          repeat_df = 1;
          repeat_delay_time = 0;
          repeat_speed_time = 0;
        }else{
           repeat_df = 0;
           repeat_delay_time = repeat_delay[(d & 0b00000110) >> 1];
           repeat_speed_time = 20 * r_s;           
        }
        
        /*
        if((c & 0b11111)==0){ // キーリピート禁止
           repeat_df=1;
        }else{
           repeat_df=0;
           repeat_delay_time = repeat_delay[(c & 0b01100000) >> 5];
           repeat_speed_time = 60 * (c&0b11111);           
        }
        */

#ifdef KEY_B_DEBUG
      Serial.print("9C-DATA : ");
      Serial.print(c,HEX);
      Serial.println("");
      Serial.print("repeat_df = ");
      Serial.println(repeat_df);
      Serial.print("repeat_delay_time  = ");
      Serial.println(repeat_delay_time );
      Serial.print("repeat_speed_time  = ");
      Serial.println(repeat_speed_time );
#endif    
        
        pc98key_send(ACK);
        break;
    }

    case 0x9D:   // LED 制御
        pc98key_send(ACK);
        while(!mySerial.available());
        c = mySerial.read();
        up_c = c & 0xf0; //上位4bit

#ifdef KEY_B_DEBUG
        Serial.print("9D-DATA : ");
        Serial.print(c,HEX);
        Serial.println("");
        Serial.print("9D-UP_DATA : ");
        Serial.print(up_c,HEX);
        Serial.println("");
#endif  

        if(up_c == 0x70){ //LED状態の通知
            //状態の記録
            //num_f  = ((c & 0x01) == 0x01);
            caps_f = ((c & 0x04) == 0x04);
            kana_f = ((c & 0x08) == 0x08);
            
            pc98key_send(ACK);
        
        }else if(up_c == 0x60){ // LED状態読み出し
            c = 0;
            //if(num_f)  c |= 0x01;
            if(caps_f) c |= 0x04;
            if(kana_f) c |= 0x08;
            pc98key_send(c);
        
        }else{
            pc98key_send(NACK);
        }
        break;
        
    case 0x9F:  // check keyboard type
        pc98key_send(ACK);
        delayMicroseconds(500);
        pc98key_send(0xA0);
        delayMicroseconds(500);
        pc98key_send(0x80);  // 新キーボードを送信
        break;

    default:
        // 不明コマンドは NACK で応答
        pc98key_send(NACK);
        break;            
    }    
    return;
  }
}

void Pc98BLEKbdRptParser::repeatKey_proc(){

  if(last_downkey == 0xFF || repeat_df) {
    repeat_mi = 0;
    last_downkey = 0xFF;
    return;
  }

  if(millis() - down_mi >= repeat_delay_time){
    if(millis() - repeat_mi >= repeat_speed_time){

#ifdef KEY_B_DEBUG
      Serial.print("Repeat key :");
      Serial.print(last_downkey,HEX);
      Serial.println("");
#endif
      pc98key_send(last_downkey);
      repeat_mi = millis();
    }
  }
}

void Pc98BLEKbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
#ifdef KEY_B_DEBUG
      Serial.print("OnKeyDown : ");
      Serial.print(key,HEX);
      Serial.println("");
#endif   

  //NumLock処理(98にはNumLock機能がないのでここでキー変換処理)
  if(key == USB_NUMLOCK){
    if(!num_f){
      //NumLockがONの時はテンキーの数字をそのまま出力
      num_f = true;
    }else{
      //NumLockがOFFの時は出力
      num_f = false;
    }
    return;
  }

  //半角・全角キーはIMEオンオフ
  if(key == USB_HANKAKUZENKAKU){
    pc98key_send(0x74);
    pc98key_send(0x35);
    pc98key_send(0x35 | 0x80);
    pc98key_send(0x74 | 0x80);
    return;
  }

  //NumLock OFFでF12キーが押された場合、キーリピート切り替え
  if(!num_f && key == USB_F12) repeat_func = !repeat_func;

  
  uint8_t c = get98Code(key,num_f);
  if(c == 0xFF) return;

  //CapsLock処理
  if(c == KEY98_CPSLK){
    if(!caps_f){
      caps_f = true;    
    }else{
      c = KEY98_CPSLK | R_CODE;
      caps_f = false;
    }
  }

  //かなキー処理
  if(c == KEY98_KANA){
    if(!kana_f){
      kana_f = true;    
    }else{
      c = KEY98_KANA | R_CODE;
      kana_f = false;
    }
  }
  
  pc98key_send(c);

    
  //キーリピート処理
  downkey_c ++;
  last_downkey = c;
  down_mi = millis();

}

void Pc98BLEKbdRptParser::OnKeyUp(uint8_t mod, uint8_t key)
{
#ifdef KEY_B_DEBUG
      Serial.print("OnKeyUp : ");
      Serial.print(key,HEX);
      Serial.println("");
#endif   

  uint8_t c = get98Code(key,num_f);
  if(c == 0xFF) return;

  //キーリピート処理
  downkey_c --;
  if(downkey_c <= 0){
    last_downkey = 0xFF; 
    downkey_c = 0;
  }else{
    down_mi = millis();
  }
  
  //CapsLock,かなキーはOnKeyUpでコマンド送らない
  if(c == KEY98_CPSLK || c == KEY98_KANA) return;
  
  pc98key_send(c | R_CODE);

}


void Pc98BLEKbdRptParser::OnControlKeysChanged(uint8_t before, uint8_t after) {

  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;

  //左Ctrl
  if (beforeMod.bmLeftCtrl != afterMod.bmLeftCtrl) {
    last_downkey = 0xFF;
    if(afterMod.bmLeftCtrl){
#ifdef KEY_B_DEBUG
      Serial.println("LeftCtrl push");
#endif
      pc98key_send(KEY98_CTRL);  
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("LeftCtrl relase");
#endif
      pc98key_send(KEY98_CTRL | R_CODE);
    }
  }

  //左シフト
  if (beforeMod.bmLeftShift != afterMod.bmLeftShift) {
    if(afterMod.bmLeftShift){
#ifdef KEY_B_DEBUG
      Serial.println("LeftShift push");
#endif
      pc98key_send(KEY98_SHIFT);  
      last_downkey = 0xFF;
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("LeftShift relase");
#endif
      pc98key_send(KEY98_SHIFT | R_CODE);
      down_mi = millis();
    }
  }

  //左ALT -> GRPH
  if (beforeMod.bmLeftAlt != afterMod.bmLeftAlt) {
    last_downkey = 0xFF;
    if(afterMod.bmLeftAlt){
#ifdef KEY_B_DEBUG
      Serial.println("LeftAlt push");
#endif
      pc98key_send(KEY98_GRPH);  
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("LeftAlt relase");
#endif
      pc98key_send(KEY98_GRPH | R_CODE);
    }
  }
  
  //左Windowsボタン
  if (beforeMod.bmLeftGUI != afterMod.bmLeftGUI) {
    last_downkey = 0xFF;
    if(afterMod.bmLeftGUI){
#ifdef KEY_B_DEBUG
      Serial.println("LeftGUI push");
#endif
      pc98key_send(KEY98_WIN);  
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("LeftGUI relase");
#endif
      pc98key_send(KEY98_WIN | R_CODE);
    }
  }

  //右Ctrl
  if (beforeMod.bmRightCtrl != afterMod.bmRightCtrl) {
    last_downkey = 0xFF;
    if(afterMod.bmRightCtrl){
#ifdef KEY_B_DEBUG
      Serial.println("RightCtrl push");
#endif
      pc98key_send(KEY98_CTRL);  
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("RightCtrl relase");
#endif
      pc98key_send(KEY98_CTRL | R_CODE);
    }
  }
  
  //右シフト
  if (beforeMod.bmRightShift != afterMod.bmRightShift) {
    if(afterMod.bmRightShift){
#ifdef KEY_B_DEBUG
      Serial.println("RightShift push");
#endif
      pc98key_send(KEY98_SHIFT);  
      last_downkey = 0xFF;
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("RightShift relase");
#endif
      pc98key_send(KEY98_SHIFT | R_CODE);
      down_mi = millis();  
    }
  }

  //右ALT -> GRPH
  if (beforeMod.bmRightAlt != afterMod.bmRightAlt) {
    last_downkey = 0xFF;
    if(afterMod.bmRightAlt){
#ifdef KEY_B_DEBUG
      Serial.println("RightAlt push");
#endif
      pc98key_send(KEY98_GRPH);  
    }else{
#ifdef KEY_B_DEBUG
      Serial.println("RightAlt relase");
#endif
      pc98key_send(KEY98_GRPH | R_CODE);
    }
  }

  //右Windowsボタン
  if (beforeMod.bmRightGUI != afterMod.bmRightGUI) {
    last_downkey = 0xFF;
    if(afterMod.bmRightGUI){
#ifdef KEY_B_DEBUG
      Serial.println("RightGUI push");
#endif
      pc98key_send(KEY98_WIN);  
    }else{
#ifdef KEY_B_DEBUG      
      Serial.println("RightGUI relase");
#endif
      pc98key_send(KEY98_WIN | R_CODE);
    }
  }

}

//HIDキーボードスキャンコードから98スキャンコードに変換(NumLock状態でテーブル切り替え)
uint8_t Pc98BLEKbdRptParser::get98Code(uint8_t key , bool num_f){
  if(num_f){
    return(codeArray[key]);
  }else{
    return(codeArrayNotLk[key]);
  }
}

//テーブル初期化
void Pc98BLEKbdRptParser::setCodeArray()
{
  for(int i = 0 ; i < 0xFF ; i++){
    codeArray[i]      = NA_CODE;
    codeArrayNotLk[i] = NA_CODE;
  }

codeArray[0x04] = 0x1D;    // A   
codeArray[0x05] = 0x2D;    // B   
codeArray[0x06] = 0x2B;    // C  
codeArray[0x07] = 0x1F;    // D  
codeArray[0x08] = 0x12;    // E  
codeArray[0x09] = 0x20;    // F  
codeArray[0x0A] = 0x21;    // G  
codeArray[0x0B] = 0x22;    // H  
codeArray[0x0C] = 0x17;    // I  
codeArray[0x0D] = 0x23;    // J  
codeArray[0x0E] = 0x24;    // K  
codeArray[0x0F] = 0x25;    // L  
codeArray[0x10] = 0x2F;    // M  
codeArray[0x11] = 0x2E;    // N  
codeArray[0x12] = 0x18;    // O  
codeArray[0x13] = 0x19;    // P  
codeArray[0x14] = 0x10;    // Q  
codeArray[0x15] = 0x13;    // R  
codeArray[0x16] = 0x1E;    // S  
codeArray[0x17] = 0x14;    // T  
codeArray[0x18] = 0x16;    // U  
codeArray[0x19] = 0x2C;    // V  
codeArray[0x1A] = 0x11;    // W  
codeArray[0x1B] = 0x2A;    // X  
codeArray[0x1C] = 0x15;    // Y  
codeArray[0x1D] = 0x29;    // Z  
codeArray[0x1E] = 0x01;    // 1  
codeArray[0x1F] = 0x02;    // 2  
codeArray[0x20] = 0x03;    // 3  
codeArray[0x21] = 0x04;    // 4  
codeArray[0x22] = 0x05;    // 5   
codeArray[0x23] = 0x06;    // 6  
codeArray[0x24] = 0x07;    // 7  
codeArray[0x25] = 0x08;    // 8  
codeArray[0x26] = 0x09;    // 9  
codeArray[0x27] = 0x0A;    // 0  
codeArray[0x28] = 0x1C;    // CR -> ENTER  
codeArray[0x29] = 0x00;    // ESC  
codeArray[0x2A] = 0x0E;    // BS  
codeArray[0x2B] = 0x0F;    // TAB  
codeArray[0x2C] = 0x34;    // SPC  
codeArray[0x2D] = 0x0B;    // -  
codeArray[0x2E] = 0x0C;    // ^  
codeArray[0x2F] = 0x1A;    // @  
codeArray[0x30] = 0x1B;    // [  
codeArray[0x31] = 0xFF;    // ??  
codeArray[0x32] = 0x28;    // ]  
codeArray[0x33] = 0x26;    // ;  
codeArray[0x34] = 0x27;    // :  
codeArray[0x35] = 0xFF;    // KNJ -> CTRL+XFERを出力する（漢字入力モード)
codeArray[0x36] = 0x30;    // ,  
codeArray[0x37] = 0x31;    // .  
codeArray[0x38] = 0x32;    // /  
codeArray[0x39] = 0x71;    // CpsLk  
codeArray[0x3A] = 0x62;    // F1  
codeArray[0x3B] = 0x63;    // F2  
codeArray[0x3C] = 0x64;    // F3  
codeArray[0x3D] = 0x65;    // F4  
codeArray[0x3E] = 0x66;    // F5  
codeArray[0x3F] = 0x67;    // F6  
codeArray[0x40] = 0x68;    // F7   
codeArray[0x41] = 0x69;    // F8 
codeArray[0x42] = 0x6A;    // F9  
codeArray[0x43] = 0x6B;    // F10  
codeArray[0x44] = 0x52;    // F11 -> VF1  
codeArray[0x45] = 0x53;    // F12 -> VF2 
codeArray[0x46] = 0x61;    // PrtScr -> COPY  
codeArray[0x47] = 0xFF;    // ScrL  
codeArray[0x48] = 0x60;    // BRK -> STOP 
codeArray[0x49] = 0x38;    // INS  
codeArray[0x4A] = 0x3E;    // HOME -> HOMECLR 
codeArray[0x4B] = 0x36;    // PgUP  
codeArray[0x4C] = 0x39;    // DEL  
codeArray[0x4D] = 0x3F;    // END -> HELP 
codeArray[0x4E] = 0x37;    // PgDN  
codeArray[0x4F] = 0x3C;    // RIGHT  
codeArray[0x50] = 0x3B;    // LEFT  
codeArray[0x51] = 0x3D;    // DOWN  
codeArray[0x52] = 0x3A;    // UP  
codeArray[0x53] = 0xFF;    // NmLK  NumLKでパッドのデータを変換する
codeArray[0x54] = 0x41;    // /  
codeArray[0x55] = 0x45;    // *  
codeArray[0x56] = 0x40;    // -  
codeArray[0x57] = 0x49;    // +  
codeArray[0x58] = 0x1C;    // CR  
codeArray[0x59] = 0x4A;    // 1  
codeArray[0x5A] = 0x4B;    // 2  
codeArray[0x5B] = 0x4C;    // 3  
codeArray[0x5C] = 0x46;    // 4  
codeArray[0x5D] = 0x47;    // 5  
codeArray[0x5E] = 0x48;    // 6   
codeArray[0x5F] = 0x42;    // 7  
codeArray[0x60] = 0x43;    // 8  
codeArray[0x61] = 0x44;    // 9  
codeArray[0x62] = 0x4E;    // 0  
codeArray[0x63] = 0x50;    // .  
codeArray[0x64] = 0xFF;    //   
codeArray[0x65] = 0x79;    // Apl  
codeArray[0x87] = 0x33;    // ﾛ  
codeArray[0x88] = 0x72;    // KANA  
codeArray[0x89] = 0x0D;    // YEN  
codeArray[0x8A] = 0x35;    // CONV(変換) -> XFER  
codeArray[0x8B] = 0x51;    // NCONV(無変換) -> NFER  
codeArray[0xE0] = 0x74;    // LCTRL  
codeArray[0xE1] = 0x70;    // LSHIFT  
codeArray[0xE2] = 0x73;    // LALT -> GRAH 
codeArray[0xE3] = 0x77;    // LWIN  
codeArray[0xE4] = 0x74;    // RCTRL  
codeArray[0xE5] = 0x70;    // RSHFT  
codeArray[0xE6] = 0x73;    // RALT -> GRPH 
codeArray[0xE7] = 0x78;    //  RWIN 
codeArray[0xE8] = 0x52;    //  コンシューマコントロール Mute        -> VF1 
codeArray[0xE9] = 0x53;    //  コンシューマコントロール Volume Up   -> VF2 
codeArray[0xEA] = 0x54;    //  コンシューマコントロール Volume Down -> VF3 
codeArray[0xEB] = 0x55;    //  コンシューマコントロール Next Track  -> VF4 
codeArray[0xEC] = 0x56;    //  コンシューマコントロール Prev Track  -> VF5 
    
//---------------------------------------------
// NumLock解除時の変換キーコード
memcpy(codeArrayNotLk, codeArray, sizeof(codeArray));

codeArrayNotLk[0x59] = 0x3F;    // 1 -> HELP
codeArrayNotLk[0x5A] = 0x3D;    // 2 -> DOWN 
codeArrayNotLk[0x5B] = 0x37;    // 3 -> PgDn  
codeArrayNotLk[0x5C] = 0x3B;    // 4 -> LEFT 
codeArrayNotLk[0x5D] = 0xFF;    // 5 -> N/A  
codeArrayNotLk[0x5E] = 0x3C;    // 6 -> RIGHT 
codeArrayNotLk[0x5F] = 0x3E;    // 7 -> HOMECLR  
codeArrayNotLk[0x60] = 0x3A;    // 8 -> UP 
codeArrayNotLk[0x61] = 0x36;    // 9 -> PgUP   
codeArrayNotLk[0x62] = 0x38;    // 0 -> INS 
codeArrayNotLk[0x63] = 0x39;    // . -> DEL 

codeArrayNotLk[0x3A] = 0x52;    // F1 -> Vf1 
codeArrayNotLk[0x3B] = 0x53;    // F2 -> Vf2 
codeArrayNotLk[0x3C] = 0x54;    // F3 -> Vf3 
codeArrayNotLk[0x3D] = 0x55;    // F4 -> Vf4 
codeArrayNotLk[0x3E] = 0x56;    // F5 -> Vf5 

/*
codeArrayNumLk[0x] = 0x;    //   
codeArray[0x0] = 0x;    //   
codeArray[0x0] = 0x;    //   
*/
}
