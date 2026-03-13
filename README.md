## **arduino-esp8266-amplifier-controller**  

Hybrid analog/digital amplifier controller with IR learning, motorized ALPS potentiometer feedback and ESP8266 web API. 



# **Arduino + ESP8266 Amplifier Controller**  

Tento projekt je komplexní řídicí systém pro klasický analogový zesilovač postavený na Arduino Nano a  ESP8266. Cílem bylo zachovat původní analogový charakter zesilovače (včetně manuálního ovládání hlasitosti), ale doplnit jej o moderní funkce – IR ovládání, webové rozhraní, vzdálený panel na terase a síťové API. 

Modul je koncipován jako upgrade pro předchozí systém postavený na mikrokontroléru CD4017N. PCB je připojena do patice na místo CD4017N.



### **Architektura**

Systém je rozdělen na dvě části:  

#### **Arduino Nano**  

- Řízení motorizovaného potenciometru ALPS (hlasitost)  

- Snímání aktuální polohy otočného ovladače hlasitosti přes analogový vstup (přidaný trimr pro zpětnou vazbu)  

- IR přijímač s možností učení až 10 kódů na každou funkci (uloženo v EEPROM)  

- Ovládání relé vstupů a reproduktorových větví  

- RGB LED indikace aktuálního vstupu  

   

#### **ESP8266**

- Web server s konfiguračním a ovládacím rozhraním  

- HTTP API pro externí řízení  

- Sériová komunikace s Arduinem 
  
  

 Obě části fungují nezávisle – výpadek WiFi neovlivní základní funkčnost zesilovače.  



### **Funkce**

#### **IR učení (EEPROM)**  

Každé funkci lze přiřadit až 10 různých IR kódů:  

- Volume +  
- Volume -  
- Power  
- Input  
- Speakers  
- Media Play/Pause  
- Media Next  
- Media Previous  
  

To umožňuje používat více dálkových ovladačů současně.  



#### **Hlasitost – hybridní řešení**  

Použit je motorizovaný potenciometr ALPS.  

   Bylo doplněno analogové snímání polohy, takže:  
- hlasitost lze měnit motorem (IR, web, HW tlačítka)  
- lze ji měnit i ručně otočením knobu  
- Arduino vždy zná aktuální fyzickou polohu
  

 Zachována je tedy plná analogová funkčnost, ale s digitální kontrolou.  

#### **Audio vstupy**  

- 6 vstupů  
- RGB LED indikuje aktivní vstup  
- Na vstupu 3 je interní Bluetooth modul  
- Na vstupu 4 je externí Bluetooth modul umístěný v terasovém panelu  
  

 Pro každý vstup lze nastavit individuální maximální limit hlasitosti, aby nedošlo k přetížení reproduktorů při různých úrovních signálu.  

#### **Přepínání reproduktorů**  

Funkce **Speakers** přepíná mezi:  

- Reproduktory v obýváku  
- Reproduktory na terase  
  
    

#### **Terasový ovládací panel**  

Externí panel obsahuje:  

- HW tlačítka pro všechny funkce  
- Otočný ovladač hlasitosti  
- IR přijímač  
  

 Zesilovač je tedy plně ovladatelný i mimo hlavní místnost.  

#### **Webové rozhraní (ESP8266)**  

ESP8266 provozuje jednoduchý web server, který umožňuje:  

- Ovládání všech funkcí zesilovače  
- Učení a mazání IR kódů  
- Nastavení maximální hlasitosti pro jednotlivé vstupy  
- Zobrazení aktuálního stavu zařízení  
  
    

#### **HTTP API**  

Server přijímá příkazy ve formátu:  

SET_POWER_ON / SET_POWER_OFF  

SET_VOLUME_25  

SET_INPUT_2  

SET_SPEAKER_A / SET_SPEAKER_B



PULSE_VOLUP  

PULSE_VOLDOWN  



SET_AUTO_OFF_<hodnota_v_sekundách> / SET_AUTO_OFF_0 – zruší časovač 



IR_LEARN_POWER (VOLUP, VOLDOWN, INPUT, SPEAKERS, PLAY, NEXT, PREV)  

IR_REMOVE_POWER (VOLUP, VOLDOWN, INPUT, SPEAKERS, PLAY, NEXT, PREV )  

IR_REMOVE_POWER_ALL  



SET_MAX_VOL_INPUT__<hodnota_volume>   (nastaví zadanou hodnotu hlasitosti jako limit)

SET_CURRENT_MAX_VOLUME  (nastaví současnou hlasitost jako limit)

WIPE_EEPROM (vymaže paměť EEPROM)

​    

 Každý požadavek vrací odpověď obsahující kompletní aktuální stav zařízení.  

 Toto API lze snadno integrovat do jiných systémů – například do aplikace **Tasker**, kde je vytvořena jednoduchá ovládací aplikace využívající HTTP příkazy  

​    
