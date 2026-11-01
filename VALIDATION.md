# ESPHome 2.0.0 ellenőrzés — 2026-11-01

Tesztelt környezet: normál **ESPHome 2026.10.1**, **ESP-IDF 5.5.6**, klasszikus ESP32 / `nodemcu-32s`, 160 MHz, 4 MB flash, PSRAM nélkül.

## Sikeres ellenőrzések

- `esphome config kallsup.yaml`: érvényes konfiguráció a helyi komponenssel.
- Teljes firmware-fordítás és linkelés: sikeres. A végső forrásfájlokat összevetettük a ténylegesen lefordított komponensmásolatokkal.
- Flash: 1 415 679 / 1 835 008 bájt (77,1%). A factory és OTA binárisok is elkészültek, az alkalmazás az OTA-partícióba belefér.
- A fordítás sdkconfigja: Classic-only vezérlő, Bluedroid, A2DP, belső SBC-kódoló, Wi-Fi/Bluetooth szoftveres együttműködés engedélyezve.
- Natív C++ PCM-tesztek: csend üresjáratban, egyidejű csengetési kérések kezelése, sztereó kimenet, előjeles minták, hang vége és megszakítás.
- Natív C++ hangerő-/címtesztek: teljes hangerő, némítás, pozitív és negatív minták skálázása, monoton exponenciális hangerőgörbe, 47%-os korábbi hangerővel összemérhető erősítés, érvényes/hibás MAC-címek.
- Konfigurációs negatív tesztek: ESP32-C3, Arduino framework és egyidejű ESPHome BLE-stack használata elutasítva.

A fordításhoz kizárólag eldobható tesztadatok és helyben generált API-kulcs volt használva. A `secrets.yaml`, buildkönyvtár és binárisok nem kerülnek a repositoryba.

## Reprodukálható tesztek

A repository gyökeréből, a megadott ESPHome-verziót tartalmazó Python-környezettel:

```bash
python tests/validate_configs.py
g++ -std=c++11 -Wall -Wextra -Werror -Icomponents/kallsup_chime tests/audio_math_test.cpp -o /tmp/audio-test
/tmp/audio-test
g++ -std=c++11 -Wall -Wextra -Werror -Icomponents/kallsup_chime tests/chime_player_test.cpp -o /tmp/chime-test
/tmp/chime-test
```

## Hardveren még ellenőrizendő

Ebben a környezetben nincs fizikailag csatlakoztatott ESP32/KALLSUP. A párosítás, hallható lejátszás, hosszú üresjárat utáni viselkedés, hangerő megjegyzése, valós BT-visszacsatlakozás, GPIO2 LED, OTA és hőmérséklet nem lett hardveren tesztelve. A sikeres fordítás ezeket nem igazolja.

A komponens saját A2DP Source megvalósítás, nem gyári ESPHome audio platform; kizárólag a leírt chime-funkcióra készült. Nem szolgáltat általános `media_player` entitást vagy tetszőleges MP3/TTS lejátszást.

## Nyilvános GitHub-forrás ellenőrzése

Az `examples/kallsup.yaml` a `f015844101867281244a53ffbd5a935054dc315b` komponens-commithoz van rögzítve. Az ESPHome a nyilvános `trtk/A2DP-ESPHome` repositoryból, `external_components` Git-forráson keresztül ténylegesen letöltötte a komponenst. Az így betöltött példakonfiguráció teljes fordítása és linkelése is sikeres volt; a factory és OTA bináris újra elkészült. A Wi-Fi/API tesztadatok és a generált binárisok nem kerültek a repositoryba.
