# How to Test COAP Client Example



## Step 1: Prepare software

The following serial terminal program and libcoap are required for COAP Client example test, download and install from below links.

- [**Tera Term**][link-tera_term]
- [**libcoap**][link-libcoap]



## Step 2: Prepare hardware

If you are using W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2, you can skip '1. Combine...'

1. Combine WIZnet Ethernet HAT with Raspberry Pi Pico.

2. Connect ethernet cable to WIZnet Ethernet HAT, W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2 ethernet port.

3. Connect Raspberry Pi Pico, W5100S-EVB-Pico or W5500-EVB-Pico to desktop or laptop using 5 pin micro USB cable. W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2 require a USB Type-C cable.



## Step 3: Setup COAP Client Example

To test the COAP Client example, minor settings shall be done in code.

1. Setup SPI port and pin in 'w5x00_spi.h' in ''WIZnet-PICO-COAP_C/port/ioLibrary_Driver/' directory.

Setup the SPI interface you use.
- If you use the W5100S-EVB-Pico, W5500-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2,

```cpp
/* SPI */
#define SPI_PORT spi0

#define PIN_SCK 18
#define PIN_MOSI 19
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_RST 20
```

If you want to test with the COAP Client example using SPI DMA, uncomment USE_SPI_DMA.

```cpp
/* Use SPI DMA */
//#define USE_SPI_DMA // if you want to use SPI DMA, uncomment.
```
- If you use the W55RP20-EVB-Pico,
```cpp
/* SPI */
#define USE_SPI_PIO

#define PIN_SCK 21
#define PIN_MOSI 23
#define PIN_MISO 22
#define PIN_CS 20
#define PIN_RST 25
```

2. Setup network configuration such as IP in 'w5x00_coap_client.c' which is the COAP Client example in 'WIZnet-PICO-COAP_C/examples/coap_client/' directory.



Setup IP and other network settings to suit your network environment.

```cpp
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 11, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};
```
```cpp
uint8_t payload[] = ""; 
uint8_t uri_path[] = ".well-known/core";
```

3. Setup COAP configuration in 'coapClient.c' in 'WIZnet-PICO-COAP_C/libraries/coapLibrary/coapClient/' directory.

In the COAP configuration, the server IP is the IP of your desktop or laptop where COAP Server will be created.

```cpp
static uint8_t destip[4] = {192, 168, 11, 3};  
static uint16_t destport = 5683;
```

## Step 4: Setup COAP Server program
1. Download libcoap program
```cpp
$ git clone https://github.com/obgm/libcoap.git
$ cd libcoap
$ git checkout main
```

2. Build libcoap program
```cpp
$ cmake -E remove_directory build
$ cmake -E make_directory build
$ cd build
$ cmake .. -DENABLE_TESTS=ON
$ cmake --build .
$ cd Debug
```


If the build is successful, the programs coap-server.exe and coap-client.exe will appear as shown below.

![1-1](https://github.com/user-attachments/assets/22446f1c-0754-4bf7-bf21-5a1921986933)

3. Execute libcoap server program and open COAP server.

![2-1](https://github.com/user-attachments/assets/05802127-9a43-4d7b-b70a-702c37cc96e6)



## Step 5: Build

1. After completing the COAP Client example configuration, click 'build' in the status bar at the bottom of Visual Studio Code or press the 'F7' button on the keyboard to build.

2. When the build is completed, 'w5x00_coap_client.uf2' is generated in 'WIZnet-PICO-COAP_C/build/examples/coap_client/' directory.


## Step 6: Upload and Run

1. While pressing the BOOTSEL button of Raspberry Pi Pico, W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2 power on the board, the USB mass storage 'RPI-RP2' is automatically mounted.

![][link-raspberry_pi_pico_usb_mass_storage]

2. Drag and drop 'w5x00_coap_client.uf2' onto the USB mass storage device 'RPI-RP2'.

3. Connect to the serial COM port of Raspberry Pi Pico, W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2 with Tera Term.

![][link-connect_to_serial_com_port]

4. Reset your board.

   
5. If the COAP Client example works normally on Raspberry Pi Pico, W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2, you can see the network information of Raspberry Pi Pico, W5100S-EVB-Pico, W5500-EVB-Pico, W55RP20-EVB-Pico, W5100S-EVB-Pico2 or W5500-EVB-Pico2 and the list of resources provided by the CoAP server every second.

![image](https://github.com/user-attachments/assets/5285b40d-b45f-4516-93e5-20e8e9f120fb)

6. Wireshark packet capture.

![image](https://github.com/user-attachments/assets/2ab6600c-4eb4-4008-ab99-403d37f5269c)



<!--
Link
-->

[link-tera_term]: https://osdn.net/projects/ttssh2/releases/
[link-libcoap]: https://github.com/obgm/libcoap
[link-raspberry_pi_pico_usb_mass_storage]: https://github.com/Wiznet/RP2040-HAT-C/blob/main/static/images/mqtt/publish/raspberry_pi_pico_usb_mass_storage.png
[link-connect_to_serial_com_port]: https://github.com/Wiznet/RP2040-HAT-C/blob/main/static/images/mqtt/publish/connect_to_serial_com_port.png
