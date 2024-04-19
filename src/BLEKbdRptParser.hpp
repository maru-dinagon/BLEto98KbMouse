#ifndef   BLEKBDRPTPARSER_HPP
#define   BLEKBDRPTPARSER_HPP

#include <Arduino.h>

#define NA_CODE 0xFF //98キーボードスキャンコードの未定義初期値

#define R_CODE 0x80 // 98キーコードのリリースの値

//98スキャンコード定義(必要なものだけ)
#define KEY98_SHIFT 0x70 //シフトキー
#define KEY98_CPSLK 0x71 //CapsLock
#define KEY98_KANA  0x72 //かなキー
#define KEY98_WIN   0x77 //Winキー
#define KEY98_GRPH  0x73 //GRPHキー
#define KEY98_CTRL  0x74 //CTRLキー

//USBキーボードスキャンコード定義(必要なものだけ)
#define USB_NUMLOCK        0x53 //NumLockキー
#define USB_HANKAKUZENKAKU 0x35 //半角全角漢字キー
#define USB_F12            0x45 //F12


//ブートモード定義
enum boot_mode {
  NORMAL,     //通常起動(何も押さない)
  SETUP_MENU, //「HELP」セットアップメニュー起動
  CRT_24,     //「GRPH」+「1(ヌ)」 DOSのCRT出力・水平周波数を24.8kHzにする
  CRT_31,     //「GRPH」+「2(フ)」 DOSのCRT出力を31kHzにする
  BIOS_REV,   //「ESC」+「HELP」+「1」ＢＩＯＳリビジョン表示
  CPU_MSG,    // [CTRL] + [CAPS] + [カナ] + [GRPH]イニシャルテストファームウェアメッセージ（CPU情報表示）
  NO_MEMCHK   // [STOP] メモリチェックなしで起動する
};

static uint8_t BOOTMODE_SETUP_MENU[2] = {0x3F,0xBF};
static uint8_t BOOTMODE_CRT_24[4]     = {0x01,0x73,0x81,0x01};
static uint8_t BOOTMODE_CRT_31[4]     = {0x02,0x73,0x82,0x02};
static uint8_t BOOTMODE_BIOS_REV[5]   = {0x01,0x3F,0x00,0x80,0x00};
static uint8_t BOOTMODE_CPU_MSG[5]    = {0x73,0x74,0x72,0x71,0xF4};
static uint8_t BOOTMODE_NO_MEMCHK[3]  = {0x60,0xE0,0x60};




//98側(miniDin8)ピン定義
#define RST 4 //リセット要求
#define RDY 34 //送信可能
#define RXD 22                                                                                                                                                                                                            //データ送信
#define RTY 39 //再送要求

//ボタンアレイ接続ピン定義
#define BUTTON_ARRAY_ADC_PIN 35

//LED接続ピン定義
#define KANA_LED 12
#define CAPS_LED 13
#define NUM_LED  15

//キーボードに対して送るコマンド定義
#define ACK              0xFA
#define NACK             0xFC

//#define KEY_B_DEBUG

//USB Host Shield 2.0 hidboot.hより構造体のみ使いたいので抜粋宣言
struct MODIFIERKEYS {
    uint8_t bmLeftCtrl : 1;
    uint8_t bmLeftShift : 1;
    uint8_t bmLeftAlt : 1;
    uint8_t bmLeftGUI : 1;
    uint8_t bmRightCtrl : 1;
    uint8_t bmRightShift : 1;
    uint8_t bmRightAlt : 1;
    uint8_t bmRightGUI : 1;
};

static  HardwareSerial mySerial(1);

struct KBDINFO {

        struct {
                uint8_t bmLeftCtrl : 1;
                uint8_t bmLeftShift : 1;
                uint8_t bmLeftAlt : 1;
                uint8_t bmLeftGUI : 1;
                uint8_t bmRightCtrl : 1;
                uint8_t bmRightShift : 1;
                uint8_t bmRightAlt : 1;
                uint8_t bmRightGUI : 1;
        };
        uint8_t bReserved;
        uint8_t Keys[6];
};

struct KBDLEDS {
        uint8_t bmNumLock : 1;
        uint8_t bmCapsLock : 1;
        uint8_t bmScrollLock : 1;
        uint8_t bmCompose : 1;
        uint8_t bmKana : 1;
        uint8_t bmReserved : 3;
};

class BLEKbdRptParser
{
  const uint8_t numKeys[10] PROGMEM = {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'};
  const uint8_t symKeysUp[12] PROGMEM = {'_', '+', '{', '}', '|', '~', ':', '"', '~', '<', '>', '?'};
  const uint8_t symKeysLo[12] PROGMEM = {'-', '=', '[', ']', '\\', ' ', ';', '\'', '`', ',', '.', '/'};
  const uint8_t padKeys[5] PROGMEM = {'/', '*', '-', '+', '\r'};

  protected:
    union {
            KBDINFO kbdInfo;
            uint8_t bInfo[sizeof (KBDINFO)];
    } prevState;

    union {
            KBDLEDS kbdLeds;
            uint8_t bLeds;
    } kbdLockingKeys;
  

  private:
    uint8_t prev  = 0x00; //直前の送信キーの保持
    int kana_f = 0, caps_f = 0,  num_f = 1;
    bool repeat_func = true; //リピートキー機能の有効・無効(NumLockしてない状態でF12で切り替え)
    int repeat_df = 0; // 0:リピート可 1:リピート不可 
    int repeat_delay_time = 500;    // リピートするまでの時間500msec
    int repeat_speed_time = 40;     // リピート間隔40msec
    int repeat_delay[4] = { 250, 500, 500, 1000,};
    int last_downkey = 0xFF; //最後に押された98キースキャンコード
    int downkey_c = 0; //同時に押されているキーの数
    unsigned long down_mi = 0; //最後に押されたキーの時間の保持
    unsigned long repeat_mi = 0; //リピート間隔を処理する時間の保持
    boot_mode bootmode = NORMAL; //ブートモードの指定
    
    uint8_t codeArray[0xFF];         //NumLock時のコード変換テーブル
    uint8_t codeArrayNotLk[0xFF];    //NumLock解除時のコード変換テーブル
    
    void pc98key_send(uint8_t data);
    void pc98key_command(void);
    void repeatKey_proc();
    boot_mode getBootMode();
    void SetLed();

   
    uint8_t get98Code(uint8_t key , bool num_f);
    void setCodeArray();

  public:
    void Parse(bool is_rpt_id __attribute__((unused)), uint8_t len __attribute__((unused)), uint8_t *buf);
    void setUp98Keyboard();
    void task();

  protected:
    void OnControlKeysChanged(uint8_t before, uint8_t after);
    void OnKeyDown  (uint8_t mod, uint8_t key);
    void OnKeyUp  (uint8_t mod, uint8_t key);
};

#endif // BLEKBDRPTPARSER_HPP


