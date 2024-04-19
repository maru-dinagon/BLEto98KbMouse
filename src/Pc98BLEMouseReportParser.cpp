#include "Pc98BLEMouseReportParser.hpp"

void Pc98BLEMouseReportParser::Parse(uint8_t len, uint8_t *buf){
  
  MOUSEINFO_EX pmi;
  bool f = ParseMouseData(pmi,len,buf);
  if(!f || OUTPUT_MOUSE_DATA){
    if (len && buf)  {
      Serial.print(F("MouseData"));
      Serial.print(F(": "));
      for (uint8_t i = 0; i < len; i++) {
          if (buf[i] < 16) Serial.print(F("0"));
          Serial.print(buf[i], HEX);
          Serial.print(F(" "));
      }
      Serial.println("");
    }
  }

  if(!f) return;


	if (prev_mInfo.bmLeftButton == 0 && pmi.bmLeftButton == 1)    
		OnLeftButtonDown(&pmi);

	if (prev_mInfo.bmLeftButton == 1 && pmi.bmLeftButton == 0)
		OnLeftButtonUp(&pmi);

	if (prev_mInfo.bmRightButton == 0 && pmi.bmRightButton == 1)
		OnRightButtonDown(&pmi);

	if (prev_mInfo.bmRightButton == 1 && pmi.bmRightButton == 0)
		OnRightButtonUp(&pmi);

	if (prev_mInfo.bmMiddleButton == 0 && pmi.bmMiddleButton == 1)
		OnMiddleButtonDown(&pmi);

	if (prev_mInfo.bmMiddleButton == 1 && pmi.bmMiddleButton == 0)
		OnMiddleButtonUp(&pmi);

	if(pmi.wheel != 0){
    ( pmi.wheel > 0 ) ? OnScrollUp(&pmi) : OnScrollDown(&pmi); 
  }
  
  if (pmi.dX != 0 || pmi.dY != 0)
		OnMouseMove(&pmi);

  prev_mInfo = pmi;
 
}

bool Pc98BLEMouseReportParser::ParseMouseData(MOUSEINFO_EX &pmi,uint32_t len, uint8_t *buf){
  //データ長 8byte
  if(len != 8) return false;

  //ボタン
  uint8_t btn = buf[0];
  pmi.bmLeftButton    = ((btn & 0x01) == 0x01 ) ? 1:0;
  pmi.bmRightButton   = ((btn & 0x02) == 0x02 ) ? 1:0;
  pmi.bmMiddleButton  = ((btn & 0x04) == 0x04 ) ? 1:0;


  //X方向の移動
  uint8_t x_val = buf[1];
  bool x_dire = (buf[2] > 0 ) ? true : false;  //右向きならtrue;
  if(x_dire){
    pmi.dX = x_val;  
  }else{
    pmi.dX = - (256 - x_val);
  }

  //Y方向の移動
  uint8_t y_val = buf[3];
  bool y_dire = (buf[4] > 0 ) ? true : false;  //下向きならtrue;
  if(y_dire){
    pmi.dY = y_val;  
  }else{
    pmi.dY = - (256 - y_val);
  }


/*  
  //スクロール
  uint8_t sc = buf[6];
  if(sc == 0x01){
    pmi.wheel = 1;
  }else if(sc == 0xFF){
    pmi.wheel = -1;
  }else{
    pmi.wheel = 0;
  }
*/  
  pmi.wheel = 0;
  
  return true; 
}


//マウスボタンデータアップデート
void Pc98BLEMouseReportParser::updateMouseBtn(){

#ifdef MOUSE_DEBUG  
  Serial.print("stateB = ");
  Serial.println(stateB,HEX);
#endif
// Button
    switch (stateB) {
      case 0x00:
        pinMode(LB, INPUT);
        pinMode(RB, INPUT);
        break;
      case 0x01:
        pinMode(LB, OUTPUT);
        pinMode(RB, INPUT);
        break;
      case 0x02:
        pinMode(LB, INPUT);
        pinMode(RB, OUTPUT);
        break;
      case 0x03:
        pinMode(LB, OUTPUT);
        pinMode(RB, OUTPUT);
        break;
    }
}

//98バスマウス通信線の初期化
void Pc98BLEMouseReportParser::setUpBusMouse(){
  pinMode(XA, INPUT);
  pinMode(XB, INPUT);
  pinMode(YA, INPUT);
  pinMode(YB, INPUT);
  pinMode(LB, INPUT);
  pinMode(RB, INPUT);
  digitalWrite(XA, LOW);
  digitalWrite(XB, LOW);
  digitalWrite(YA, LOW);
  digitalWrite(YB, LOW);
  digitalWrite(LB, LOW);
  digitalWrite(RB, LOW);
}

void Pc98BLEMouseReportParser::OnMouseMove(MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.print("dx=");
  Serial.print(mi->dX, DEC);
  Serial.print(" dy=");
  Serial.println(mi->dY, DEC);
#endif
  bool X_prev = (mi->dX >= 0); //現在カウントしているX軸方向(true:+ false:-)
  bool Y_prev = (mi->dY >= 0); //現在カウントしているY軸方向(true:+ false:-)

  uint8_t X_count = abs(mi->dX);  //X軸方向の移動量のカウンター
  uint8_t Y_count = abs(mi->dY);  //Y軸方向の移動量のカウンター

#ifdef MOUSE_DEBUG  
  Serial.print("X_count = ");
  Serial.print(X_count);  
  Serial.print(" Y_count = ");
  Serial.println(Y_count);  
#endif

  for(int i = 0 ; i < X_count ; i++) MoveXPc98Mouse(X_prev);
  for(int i = 0 ; i < Y_count ; i++) MoveYPc98Mouse(Y_prev);

};

// Cursor
// state      0 1 3 2
//          ___     ___
// A pulse |   |___|   |___
//            ___     ___
// B pulse   |   |___|   |___
//
// declease <--        --> increase
//
// For XA,XB the increasing pulse move the cursor rightward. (Positive for PS/2)
// For YA,YB the increasing pulse move the cursor downward. (Negative for PS/2)

void Pc98BLEMouseReportParser::MoveXPc98Mouse(bool di){
  if(di){
    //+方向(右方向) 0->1->3->2
    if(X_state == 0){
      X_state = 1;
      pinMode(XA, OUTPUT);
      pinMode(XB, INPUT);
    }else if(X_state == 1){
      X_state = 3;
      pinMode(XA, OUTPUT);
      pinMode(XB, OUTPUT);
    }else if(X_state == 3){
      X_state = 2;
      pinMode(XA, INPUT);
      pinMode(XB, OUTPUT);
    }else if(X_state == 2){
      X_state = 0;
      pinMode(XA, INPUT);
      pinMode(XB, INPUT);
    }else{
      X_state = 0;
    }
  }else{
    //-方向(左方向) 2->3->1->0
    if(X_state == 2){
      X_state = 3;
      pinMode(XA, OUTPUT);
      pinMode(XB, OUTPUT);
    }else if(X_state == 3){
      X_state = 1;
      pinMode(XA, OUTPUT);
      pinMode(XB, INPUT);
    }else if(X_state == 1){
      X_state = 0;
      pinMode(XA, INPUT);
      pinMode(XB, INPUT);
    }else if(X_state == 0){
      X_state = 2;
      pinMode(XA, INPUT);
      pinMode(XB, OUTPUT);
    }else{
      X_state = 0;
    }
  }
  delayMicroseconds(150);
}

void Pc98BLEMouseReportParser::MoveYPc98Mouse(bool di){
  if(di){
    //+方向(下方向) 0->1->3->2
    if(Y_state == 0){
      Y_state = 1;
      pinMode(YA, OUTPUT);
      pinMode(YB, INPUT);
    }else if(Y_state == 1){
      Y_state = 3;
      pinMode(YA, OUTPUT);
      pinMode(YB, OUTPUT);
    }else if(Y_state == 3){
      Y_state = 2;
      pinMode(YA, INPUT);
      pinMode(YB, OUTPUT);
    }else if(Y_state == 2){
      Y_state = 0;
      pinMode(YA, INPUT);
      pinMode(YB, INPUT);
    }else{
      Y_state = 0;
    }
  }else{
    //-方向(上方向) 2->3->1->0
    if(Y_state == 2){
      Y_state = 3;
      pinMode(YA, OUTPUT);
      pinMode(YB, OUTPUT);
    }else if(Y_state == 3){
      Y_state = 1;
      pinMode(YA, OUTPUT);
      pinMode(YB, INPUT);
    }else if(Y_state == 1){
      Y_state = 0;
      pinMode(YA, INPUT);
      pinMode(YB, INPUT);
    }else if(Y_state == 0){
      Y_state = 2;
      pinMode(YA, INPUT);
      pinMode(YB, OUTPUT);
    }else{
      Y_state = 0;
    }
  }
  delayMicroseconds(150);
}

void Pc98BLEMouseReportParser::OnLeftButtonUp  (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("L Butt Up");
#endif
  stateB = stateB & 0xFE;
  updateMouseBtn();
};

void Pc98BLEMouseReportParser::OnLeftButtonDown (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("L Butt Dn");
#endif
  stateB = stateB | 0x01;
  updateMouseBtn();
};

void Pc98BLEMouseReportParser::OnRightButtonUp  (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("R Butt Up");
#endif
  stateB = stateB & 0xFD;
  updateMouseBtn();
};

void Pc98BLEMouseReportParser::OnRightButtonDown  (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("R Butt Dn");
#endif
  stateB = stateB | 0x02;
  updateMouseBtn();
};

void Pc98BLEMouseReportParser::OnMiddleButtonUp (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("M Butt Up");
#endif
};

void Pc98BLEMouseReportParser::OnMiddleButtonDown (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("M Butt Dn");
#endif
};

void Pc98BLEMouseReportParser::OnScrollUp (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("ScrollUp");
#endif
};

void Pc98BLEMouseReportParser::OnScrollDown (MOUSEINFO_EX *mi){
#ifdef MOUSE_DEBUG  
  Serial.println("ScrollDown");
#endif
};