# esp32-zigbee-dimmer-button

\# ESP32 Dual-Channel Wi-Fi Dimmer



This project transforms an ESP32 into a powerful, feature-rich, dual-channel smart dimmer for LED lights. It provides a responsive web interface for control and configuration, and integrates seamlessly with smart home systems via MQTT, including support for Home Assistant discovery.



Control your lights using physical buttons or through the web UI from any device on your network.



\## Features



\-   \*\*Dual-Channel Control:\*\* Independently control two separate LED channels.

\-   \*\*Web Interface:\*\* A beautiful, mobile-friendly web UI to control the lights and configure all device settings.

\-   \*\*Physical Button Control:\*\*

&nbsp;   -   1x Click: Toggle On/Off

&nbsp;   -   2x Click: Activate Preset 1

&nbsp;   -   3x Click: Activate Preset 2

&nbsp;   -   Hold: Adjust brightness

\-   \*\*MQTT Integration:\*\* Control the dimmer via MQTT for easy integration with Home Assistant, Node-RED, or any other smart home platform.

\-   \*\*Home Assistant Discovery:\*\* Enable for automatic detection and configuration in Home Assistant.

\-   \*\*Advanced Dimming Options:\*\* Configure dimming speed, fade transitions, gamma correction, and minimum/maximum brightness levels to suit your lights.

\-   \*\*Over-the-Air (OTA) Updates:\*\* Update the firmware wirelessly through the web interface.

\-   \*\*Configuration Management:\*\* Backup and restore your device configuration directly from the UI.

\-   \*\*Live Logs:\*\* View device logs in real-time through the web interface for easy debugging.



\## Installation and Flashing



Follow these steps to compile and upload the firmware to your ESP32 using the Arduino IDE.



\### 1. Prepare the Arduino IDE



1\.  Download and install the latest version of the \[Arduino IDE](https://www.arduino.cc/en/software).

2\.  Install the ESP32 board definitions:

&nbsp;   \*   Open \*\*File > Preferences\*\*.

&nbsp;   \*   In the "Additional Boards Manager URLs" field, add the following URL:

&nbsp;       ```

&nbsp;       https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package\_esp32\_index.json

&nbsp;       ```

&nbsp;   \*   Open \*\*Tools > Board > Boards Manager...\*\*.

&nbsp;   \*   Search for "esp32" and install the package by Espressif Systems.



\### 2. Install Required Libraries



This project requires several libraries. You can install them using the Arduino Library Manager. Open \*\*Tools > Manage Libraries...\*\* and search for and install each of the following:



\*   `ArduinoJson` by Benoit Blanchon

\*   `MQTT` by Joel Gaehwiler



The following libraries need to be installed manually, as they are not in the Arduino Library Manager.



1\.  \*\*AsyncTCP\*\*:

&nbsp;   \*   Go to the \[AsyncTCP GitHub page](https://github.com/me-no-dev/AsyncTCP).

&nbsp;   \*   Click \*\*Code > Download ZIP\*\*.

&nbsp;   \*   In the Arduino IDE, go to \*\*Sketch > Include Library > Add .ZIP Library...\*\* and select the downloaded file.



2\.  \*\*ESPAsyncWebServer\*\*:

&nbsp;   \*   Go to the \[ESPAsyncWebServer GitHub page](https://github.com/me-no-dev/ESPAsyncWebServer).

&nbsp;   \*   Download the ZIP file and install it the same way you installed AsyncTCP.



\### 3. Load the Project



1\.  Clone this repository or download the source code as a ZIP file.

2\.  Place all the project files (e.g., `web.cpp`, `config.h`, `mqtt.h`, etc.) into a single sketch folder in your Arduino sketchbook.

3\.  The main sketch file (e.g., `esp32-zigbee-dimmer-button.ino`) should be opened in the Arduino IDE.



\### 4. Configure and Upload



1\.  Connect your ESP32 to your computer via USB.

2\.  In the Arduino IDE, go to \*\*Tools > Board\*\* and select a generic "ESP32 Dev Module" or the specific model you are using.

3\.  Go to \*\*Tools > Port\*\* and select the correct COM port for your ESP32.

4\.  Click the \*\*Upload\*\* button (the arrow icon).



The IDE will compile the code and flash it to your ESP32. Once finished, the device will reboot. On its first run, it will create a Wi-Fi Access Point that you can connect to in order to configure its connection to your home network.

