#ifndef   PC98BLEMOUSEREPORTPARSER_HPP
#define   PC98BLEMOUSEREPORTPARSER_HPP

#include <Arduino.h>

#define XA 27
#define XB 14
#define YA 32
#define YB 33
#define LB 25
#define RB 26

//#define MOUSE_DEBUG

//USBマウスの生データを出力するかどうか
//#define OUTPUT_MOUSE_DATA

struct MOUSEINFO_EX {
    uint8_t bmLeftButton = 0;
    uint8_t bmRightButton = 0;
    uint8_t bmMiddleButton = 0;
    int8_t wheel = 0; // 0:動きなし 1:UP -1:DOWN
    int8_t dX = 0;
    int8_t dY = 0;
};

class Pc98BLEMouseReportParser
{
  
  private:
    struct MOUSEINFO_EX	prev_mInfo;

    int stateB = 0; //L,R,Midボタンの状態
    
    //98マウスの現在のパルスの状態を保持する
    int X_state = 0;
    int Y_state = 0;

    void updateMouseBtn();
    //98マウスのX方向への移動(1パルス分 di=trueで右方向)
    void MoveXPc98Mouse(bool di);
    //98マウスのY方向への移動(1パルス分 di=trueで下方向)
    void MoveYPc98Mouse(bool di);
  
  
  public:

    void Parse(uint8_t len, uint8_t *buf);
    void setUpBusMouse();
    //BLEマウスのデータ解析関数
    bool ParseMouseData(MOUSEINFO_EX &pmi,uint32_t len, uint8_t *buf);

  protected:
    //イベント
    void OnMouseMove(MOUSEINFO_EX *mi);
    void OnLeftButtonUp(MOUSEINFO_EX *mi);
    void OnLeftButtonDown(MOUSEINFO_EX *mi);
    void OnRightButtonUp(MOUSEINFO_EX *mi);
    void OnRightButtonDown(MOUSEINFO_EX *mi);
    void OnMiddleButtonUp(MOUSEINFO_EX *mi);
    void OnMiddleButtonDown(MOUSEINFO_EX *mi);
    void OnScrollDown(MOUSEINFO_EX *mi);
    void OnScrollUp(MOUSEINFO_EX *mi);
  
  };

#endif // PC98BLEMOUSEREPORTPARSER_HPP