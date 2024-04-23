# BLEto98KbMouse
 エレコムのBLEマウスと3COINのBLEキーボードをESP32を用いて、 98用キーボード・バスマウスに変換<br><br>
[エレコム マウス ワイヤレスマウス SHELLPHA Bluetooth 静音 抗菌 5ボタン ブラック M-SH20BBSKBK](https://amzn.to/4aNvktV)
[3COINS(スリーコインズ)Bluetoothキーボード](https://www.palcloset.jp/display/item/2008-KP-003-000/?cl=11&b=3coins&ss=)

## 必要な機材
 ・ESP32 Dev Board(ESP-WROOM-32)<br>
 ・エレコム マウス ワイヤレスマウス SHELLPHA<br>
 ・3COINS(スリーコインズ)Bluetoothキーボード<br>
 ※必ず上のメーカーの機材というわけではなくコード修正でなんとかなると思います

## 開発環境
 VSCode + PlatformIO<br>
 [ESP32でのPlatformIO IDEの使い方と環境構築](https://qiita.com/nextfp/items/f54b216212f08280d4e0)で環境を整えました<br>
 以前までArduino IDE 2.0を利用してましたがビルトが激遅なのでVSCode + PlatformIOに変えたらストレスフリーです。是非おすすめです

## 必要なライブラリ
[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

## 紹介と仕様解説ブログ
[[PC-98][ESP32] USBマウス・キーボードをESP32でPC-98につなげる①](https://androiphone.uvs.jp/?p=4136)<br>
[[PC-98][ESP32] USBマウス・キーボードをESP32でPC-98につなげる②](https://androiphone.uvs.jp/?p=4157)

## 参考サイト・謝辞
このコードを作るにあたり、とても参考にさせていただきました。この場を借りてお礼申し上げます。<br>
[https://qiita.com/coppercele/items/4ce0e8858a92410c81e3](https://qiita.com/coppercele/items/4ce0e8858a92410c81e3)<br>
[https://github.com/tyama501/ps2busmouse98](https://github.com/tyama501/ps2busmouse98)<br>
[http://miha.jugem.cc/?eid=114](http://miha.jugem.cc/?eid=114)
 
