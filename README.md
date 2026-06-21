

- `Smart_Classroom_System.ino`

Kodun icinde su ozellikler hazirdir:

- RFID kart yetkilendirmesi
- Yetkili kart okutulunca kapi servo acma
- Ilk giriste isik acma
- PIR ile 5 dakika hareketsizlikte sinifi bos kabul edip isik ve fani kapama
- DHT11 ile sicaklik yukselince fan acma
- LDR ile perdeyi otomatik acma / yari acma / kapama
- IR kumanda ile projeksiyon perdesi yukari / asagi hareketi


Onemli notlar:

- Birden fazla servo ve motor kullanirken harici 5V besleme kullanman daha dogru olur.
- DC fan veya role modulu kullanmiyorsan, fani Arduino pinine direkt baglama; transistor veya role uzerinden sur.
- LDR devresi ters okuma yapiyorsa `LDR_BRIGHT_THRESHOLD` mantigini ters cevirmen gerekebilir.
- Role modulun aktif-low ise `LIGHT_ON_LEVEL`, `LIGHT_OFF_LEVEL`, `FAN_ON_LEVEL`, `FAN_OFF_LEVEL` kisimlarini tersle.
