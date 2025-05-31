
On Arch Linux:

Make sure your project folder is correctly placed under:
"esp-zigbee-sdk/examples/esp_zigbee_customized_devices/"

Make sure it’s installed. If not, install from: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
cd ~/esp/esp-idf
source export.sh
esp-idf-eclipse

Open a terminal in Espressif IDE (or use idf.py terminal):
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
