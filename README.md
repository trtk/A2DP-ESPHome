# A2DP ESPHome — KALLSUP csengő

Bluetooth Classic A2DP **hangküldő** külső komponens a klasszikus ESP32-höz. A Home Assistantból indított, beépített „ding-dong” csengőhangot egy IKEA KALLSUP vagy más A2DP Bluetooth-hangszórón játssza le.

**2.0.0 • tesztelt fordítás: ESPHome 2026.10.1 + ESP-IDF 5.5.6 • NodeMCU ESP-32S v1.1, 4 MB flash, 160 MHz, PSRAM nélkül.**

## Funkciók

- Natív ESPHome / Home Assistant **Csengetés** gomb; MQTT nem szükséges.
- 0–100%-os, újraindítás után visszaálló hangerő. 0% némít; a korábbi csengő-firmware exponenciális hangerőgörbéjét használja.
- **Bluetooth kapcsolat** és **Csengetesre kesz** állapotjelzők.
- Kapcsolható kék LED a NodeMCU GPIO2 lábán. A piros tápjelző nem vezérelhető szoftverből.
- Folyamatos Bluetooth-kapcsolat és néma PCM-hangfolyam csengetések között is; nincs időzített bontás vagy szüneteltetés.
- Beépített 2,4 másodperces hang, hálózati hangletöltés, SD-kártya és külső DAC nélkül.
- ESPHome API titkosítás, naplózás és OTA-frissítés.

A komponens a normál ESPHome-hoz adható hozzá. Nem igényel módosított ESPHome-forkot vagy az Arduino A2DP-könyvtárát: közvetlenül az ESP-IDF A2DP Source API-ját használja. Kifejezetten csengő; nem általános `media_player`, MP3- vagy TTS-lejátszó.

## Telepítés az ESPHome Device Builderben

1. Frissítsd az ESPHome Device Buildert legalább **2026.10.1** verzióra.
2. Másold az [examples/kallsup.yaml](examples/kallsup.yaml) teljes tartalmát a saját ESPHome-eszközöd YAML-konfigurációjába.
3. Az [examples/secrets.example.yaml](examples/secrets.example.yaml) mintája alapján add meg a Wi-Fi-adatokat, saját API-kulcsot és OTA-jelszót a HA `/config/esphome/secrets.yaml` fájljában. Ha már létezik, a meglévő kulcsokat őrizd meg. Az API-kulcs helyére valódi, Base64-kódolt 32 bájtos véletlen kulcs szükséges, például `openssl rand -base64 32` kimenete; a mintaszöveg nem használható kulcsként.
4. Fordítsd le a konfigurációt. Az ESPHome automatikusan letölti ezt a komponenst a nyilvános repositoryból; GitHub-token vagy komponensmappa kézi másolása nem szükséges.
5. Az első feltöltést USB-n végezd. A korábbi saját Arduino/MQTT-firmware-ről ESPHome OTA-val nem lehet áttérni. Ha az ESP32 a számítógéphez csatlakozik, a Builderből letöltött factory firmware az [ESPHome Web](https://web.esphome.io/) felületével is telepíthető. A további ESPHome-frissítések már OTA-val mennek.
6. A KALLSUP legyen bekapcsolva és folyamatos USB-tápon. Első alkalommal állítsd párosítási módba; a telefon vagy számítógép ne foglalja el a hangszórót.
7. Add hozzá a HA-ban megjelenő eszközt az **ESPHome** integrációhoz a saját API-kulcsoddal. Ha az automatikus felismerés nem működik, az ESP32 IP-címével kézzel is hozzáadható.

A külső komponens betöltése a teljes példában:

```yaml
external_components:
  - source: github://trtk/A2DP-ESPHome@main
    components: [kallsup_chime]
```

A példakonfigurációt stabil, kipróbált komponens-commithoz rögzítjük; a fenti `@main` forma a legfrissebb változatot tölti le. Hosszú távú telepítésnél a `main` helyett konkrét commit SHA is használható.

## Hangszóró kiválasztása

Alapból `KALLSUP` néven keres, majd megjegyzi az első megtalált hangszóró MAC-címét. Több KALLSUP esetén használd az explicit címet:

```yaml
kallsup_chime:
  id: chime
  speaker_name: KALLSUP
  address: !secret kallsup_mac
  connected:
    name: Bluetooth kapcsolat
  audio_ready:
    name: Csengetesre kesz
```

A `speaker_name` más A2DP-hangszóró nevére is módosítható, de a név megváltoztatása nem írja felül a már megjegyzett MAC-címet. Másik hangszóróhoz adj meg explicit címet. Más modellek kompatibilitása nincs hardveren tesztelve.

## Hangerő, LED és állapot

A **Hangero** szám entitás 0–100%-os csúszka. Az ESPHome preference kezeli a mentést, 10 másodperces flashírási időközzel; állítás után várj legalább 10 másodpercet az áramtalanítással. A korábbi Arduino-firmware hangerőmentése nem importálódik automatikusan, az első kezdőérték 47%.

A **Kek LED** a GPIO2 kék LED-et kapcsolja, induláskor kikapcsolt. A NodeMCU-32S gyári piros LED-je közvetlen tápjelző, nincs hozzá szoftveres kapcsoló.

A **Bluetooth kapcsolat** a létrejött A2DP-kapcsolatot mutatja. A **Csengetesre kesz** ezen felül legalább egy másodperc stabil hangfolyamot és friss PCM-adatkéréseket is megkövetel. Ha nincs kész, a gombnyomást naplózza és eldobja; nem játssza le váratlanul később. Csengetés közben az újabb gombnyomásokat is eldobja.

Normál üresjáratban a BT-kapcsolat és a néma hangfolyam folyamatos. Csak egy hibás, 20 másodpercnél tovább elakadt kapcsolódási kísérlet megszakításához van helyreállító bontás; egy egészséges kapcsolatot nem bont. A hangszóró ki/be kapcsolása után a mentett címre újracsatlakozást próbál. A 160 MHz-es órajel mellett a tényleges melegedés csökkenése nem lett hardveren mérve.

## Zigbee-gomb és átállás MQTT-ről

A Zigbee-gomb kiváltó oka maradhat a meglévő ZHA/Zigbee2MQTT esemény. A műveletben az új **natív ESPHome Csengetés gombot** válaszd ki; minta: [examples/ha-action.yaml](examples/ha-action.yaml). A tényleges entity ID-t a HA-ból másold ki, mert a korábbi MQTT-entitások miatt számozott utótagot kaphat.

Az új firmware nem törli a broker korábbi retained discovery üzeneteit. Miután az új csengőt és automatizálást kipróbáltad, az [opcionális MQTT-takarítás](examples/ha-mqtt-discovery-cleanup.yaml) törölheti a régi négy KALLSUP-entitás discovery definícióját. Egyedi korábbi `DEVICE_ID` esetén előbb módosítsd a topicokat. Más MQTT-adatot nem érint.

## Kompatibilitás

- Csak a klasszikus ESP32 támogatott. ESP32-S3/C3/C6 nem tudja ezt a Bluetooth Classic kapcsolatot.
- A tesztelt konfiguráció `framework: type: esp-idf`, verzió **5.5.6**. Arduino frameworkkel nem használható.
- A komponens saját maga kezeli a Bluetooth-stacket; BLE proxyval, BLE trackerrel vagy más ESPHome BLE-komponenssel egy konfigurációban nem használható.
- A KALLSUP maradjon USB-tápon. Áramkimaradás utáni automatikus bekapcsolását a saját hangszóródon ellenőrizd; a firmware nem tudja fizikailag megnyomni a bekapcsológombját.

## Fejlesztés és ellenőrzés

Az [ellenőrzési jegyzőkönyv](VALIDATION.md) tartalmazza a konfigurációvalidálás, a natív C++ tesztek és a teljes firmware-fordítás eredményét. Fizikai ESP32/KALLSUP ebben a környezetben nem állt rendelkezésre, ezért a párosítás, hallható lejátszás, hosszú üresjárat utáni működés, visszacsatlakozás és OTA hardveres tesztje még szükséges.

Helyi fejlesztéshez a YAML `external_components` forrása cserélhető `type: local`, `path: ../components` értékre, ha az `examples/kallsup.yaml` fájlt fordítod. Telepített ESPHome 2026.10.1 környezetből a tesztek:

```bash
python tests/validate_configs.py
g++ -std=c++11 -Wall -Wextra -Werror -Icomponents/kallsup_chime tests/audio_math_test.cpp -o /tmp/audio-test
/tmp/audio-test
g++ -std=c++11 -Wall -Wextra -Werror -Icomponents/kallsup_chime tests/chime_player_test.cpp -o /tmp/chime-test
/tmp/chime-test
```

A `tools/generate_chime.py` újragenerálja a beépített PCM-adatot és egy helyi `dingdong-preview.wav` meghallgatható előnézetet. A Wi-Fi- és API-adatok csak a saját `secrets.yaml` fájlodban legyenek; azt és a buildkönyvtárakat a repository kizárja.
