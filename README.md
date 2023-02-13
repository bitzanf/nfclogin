# PŘIHLAŠOVÁNÍ POMOCÍ NFC
**tento projekt vznikl jako ročníková práce ve škole**  

---
## Návod k nasazení
Projekt se skládá z několina částí:
 - linux-pam
 - linux-userconf
 - NFCLoginApp
 - arduino_comms

### linux-pam
*PAM modul pro Linux*  (**C++20**)  
konfigurace se provádí v `/etc/pam.d/` v jednotlivých souborech služeb  
do souboru se uvede řádek  
```
auth required /cesta/k/pam_nfc.so tty=/dev/ttyACM0 baudrate=9600 parallel=login
```  
| symbol | vysvětlení
| ---- | ---- |
| `auth` | modul slouží k autentifikaci |
| `required` | modul je vyžadován |
| `/cesta/k/pam_nfc.so` | cesta k modulu |
| `tty=/dev/ttyACM0` | cesta k sériovému portu, na kterém je připojené Arduino |
| `baudrate=9600` | rychlost sériové linky: 9600 bitů/s |
| `parallel=login` | souběžně s NFC se spustí i služba `login` |
  
kompilace:
```
.../linux-pam/ $ cmake --build $(pwd)/build
```  
závislostí:
 - libpam0g-dev
 - libcrypt-dev
 - libssl-dev
 - libsqlite3-dev

### linux-userconf
*konfigurační nástroj pro Linux*
kompilace obdobně jako `linux-pam`  
**NEJDŘÍVE JE TŘEBA ZKOMPILOVAT** `linux-pam`, **VYUŽÍVAJÍ SE JIŽ HOTOVÉ** .o **SOUBORY**

### NFCLoginApp
*aplikace pro Android* (**C#**; VS2022)  
veškeré závislosti by měly být obsažené v souborech VS...  

návod / popis aplikace:  
po otevření aplikace se zobrazí výpis registrovaných zařízení  
po rozkliknutí zařízení se zobrazí detaily včetně možnosti jeho odstranění  
vpravo nahoře (na hlavní obrazovce) tlačítko **Add** umožňuje registrovat nové zařízení  
na běžný provoz stačí aplikace nainstalovaná a zaplnutá funkce NFC, Android spustí aplikaci při přihlašování v pozadí sám

### arduino_comms
*Arduino sketch* (**C++11 / Arduino**)
kompilace pomocí `arduino-cli` a skriptu `deploy.sh`  

závislosti:  
 - **PN532** knihovna
 - knihovna pro desku **Arduino 101**