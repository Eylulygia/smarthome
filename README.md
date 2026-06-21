# Smart Classroom System

Bu klasorde Arduino Uno icin tek dosyali bir otomasyon sketch'i vardir:

- `Smart_Classroom_System.ino`

Kodun icinde su ozellikler hazirdir:

- RFID kart yetkilendirmesi
- Yetkili kart okutulunca kapi servo acma
- Ilk giriste isik acma
- PIR ile 5 dakika hareketsizlikte sinifi bos kabul edip isik ve fani kapama
- DHT11 ile sicaklik yukselince fan acma
- LDR ile perdeyi otomatik acma / yari acma / kapama
- IR kumanda ile projeksiyon perdesi yukari / asagi hareketi

Gerekli Arduino kutuphaneleri:

- `MFRC522`
- `DHT sensor library`
- `IRremote`
- `Servo` ve `SPI` zaten Arduino IDE ile gelir
- Bu kod `IRremote` kutuphanesinin guncel API'si (`IRremote.hpp`) ile yazildi

Yuklemeden once mutlaka bunlari duzenle:

1. `Smart_Classroom_System.ino` icindeki pin tanimlari
2. `AUTHORIZED_CARDS` dizisindeki RFID UID degerleri
3. `IR_CODE_SCREEN_UP` ve `IR_CODE_SCREEN_DOWN` degerleri
4. Servo acilari
5. `LDR_BRIGHT_THRESHOLD` ve `LDR_DARK_THRESHOLD`

RFID UID nasil ogrenilir:

1. Kodu yukle.
2. Serial Monitor'u `115200` baud ile ac.
3. Kart okut.
4. Yetkisiz kartta Serial Monitor'de UID yazacak.
5. O UID'yi `AUTHORIZED_CARDS` icine ekle.

IR kodu nasil ogrenilir:

1. Kodu yukle.
2. Serial Monitor'u ac.
3. Kumandadan istedigin tusa bas.
4. `IR raw code: 0x...` seklinde kod gorunecek.
5. Yukari ve asagi tuslari icin bu degerleri sketch'te degistir.

Onemli notlar:

- Birden fazla servo ve motor kullanirken harici 5V besleme kullanman daha dogru olur.
- DC fan veya role modulu kullanmiyorsan, fani Arduino pinine direkt baglama; transistor veya role uzerinden sur.
- LDR devresi ters okuma yapiyorsa `LDR_BRIGHT_THRESHOLD` mantigini ters cevirmen gerekebilir.
- Role modulun aktif-low ise `LIGHT_ON_LEVEL`, `LIGHT_OFF_LEVEL`, `FAN_ON_LEVEL`, `FAN_OFF_LEVEL` kisimlarini tersle.
