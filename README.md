# YAESA
**Y**et **A**nother **E**nvironment **S**ensor **A**ggregator<br>

Remote wireless temperature sensors and remote wireless weather station with temperature/humidity/rain/wind sensors.<br>
Local sensor with temperature/humidity/pressure and another local'ish sensor with a temperature sensor at the end of a long wire.

**Updates 2021/08/22 -**
<br>
- Builds for Pimoroni's Tiny 2040 boards (and hopefuly still for pico)
- Having gone to that expense (for less pins !), added some PWM LED control (_**led_control.c**_ and _**h**_)
- To evaluate the PWM LEDs at runtime, added override with control received from the uart.
- Added non-blocking stdin to do just the same as the uart receiver (renaming source files _**uart_io.c**_ and _**h**_ to _**serial_io**_)

**Using RP2040 and PIOs -** 
<br>
* Two PIO state machines to detect bit pulse streams from the raw incoming data provided by two RF AM receivers, both with OOK modulation but one Manchester encoded and the other PWM encoded. Receiving data from
  - five remote F007T temperature sensors transmiting at 433 MHz
  - one remote WH1080 weather station transmiting temperature, humidity, rain, wind data and date/time at 868 MHz.
* One PIO state machine to provide a simple very blocking implementation of the one-wire protocol to a parasiticaly powered DS18B20 on a long'ish wire (only had 8.5 m available).
* Also a local BME280 with temperature, humidity and pressure currently using a version of the Raspberry PI (Trading) Ltd.'s BME280 SPI example code.

### Why do it?
I had a fridge that kept freezing vegetables. It was interesting to see how bad the overshoot and undershoot of the controller was.
<br>
And why not monitor the freezer too. It had a much tighter controller.
<br>
The attic was getting extra loft insulation and I wondered how cold the water tank in the attic now got. It's only gone as low as zero so far.
<br>
There is also a water softener in the garage that didn't want to get too cold.
<br>
<br>
So I got 5 Ambient Weather F007T sensors with base station.
The base station was rubbish, invariably displaying 'HH', impossible temperatures and push button switches from the 1960's.
<br>
Now two of the five F007T sensors regularly fail - one likes to send a negative version of the temperature, the other likes 111.7 C a lot (a suspiciously raw temperature value of 0xAAA).
Fortunately its fairly easy to create replacement ones using cheap 8 bit micros, DS18B20s and 433 MHz transmitters. 
<br>
<br>
Then my youngest son inherited a Fine Offset WH1080 weather station from his grandfather ...

# Overview
![overview](https://github.com/peter-g-wilson/yaesa/blob/main/docs/overview.png)

### Remote Sensors
#### **F007T**
  - The particular F007T temperature sensors being used transmit 433 MHz OOK Manchester encoded messages. Example sensors are Oregon Scientific WMR86, Ambient Weather F007 and Froggit FT007. No doubt all are manufactured in China and just re-badged. Be aware that the encoding method may change between different versions of sensors.
  - For this F007T, the RF receiver can be any one that does 433 MHz OOK e.g. RF Solutions AMRX9-433P or RadioControlli RCRX-434-L The message protocol is a bit limiting for the sophisticated receivers like HopeRF RFM65.
  - There are also decodes for SDR. e.g. ambient_weather.c at https://github.com/merbanan/rtl_433
  - Its been done on Arduino’s - Rob Ward (and the people he credits), https://github.com/robwlakes/ArduinoWeatherOS using a decoding delay algorithm. Ron Lewis, https://eclecticmusingsofachaoticmind.wordpress.com/2015/01/21/home-automationtemperature-sensors/ for the checksum algorithm?

#### **WH1080**
  - The WH1080 weather station being used transmits 868 MHz OOK PWM encoded messages. Again probably re-badged and the encoding method may change between different versions of sensors.
  - Simple RF OOK receivers that are low voltage are harder to come by - RadioControlli do the RCRX-868-L.
  - Again there are SDR. decodes given at https://github.com/merbanan/rtl_433 by Benjamin Larsson et al.
  - The sensors on this weather station have outdoor temperature and humidity, rain and wind speed and direction. It also reports date/time information a couple of times over a day - received from Mainflingen near Frankfurt in Germany.

Both RF AM receivers with automatic gain control generate spurious data between real messages but the PIO algorithms are not very defensive.<br>
The WH1080 and F007T both have checksums for message integrity checking but these aren’t strong enough for all types of noise.<br>
Both WH1080 and F007T repeat the message more than once in some transmissions and so if the checksum passes then the message can be compared to the previous one. If two consecutive messages pass the checksum and the messages are identical then the message could be accepted as good.<br>
The WH1080 PWM OOK with 10 byte message and 8 bit CRC appears to be particularly susceptible with a lot of ‘1’s in the noise.

# Code
* _**yaesa_rp2040.c**_  
  - has the CPU core 0 main entry point that calls the WH1080 and F007T timer and PIO initialisations and also has the core 1 entry point to handle the BME280 connected by SPI, the DS18B20 one-wire protocol and the 2nd UART to output the data that's been received.
  - on core 0 the "main" loop does nothing. Its the timer callbacks that empty the PIO FIFOs and that "parse" the bit streams looking for recognisable messages, adding them to the message queues.
  - on core 1 the liesurely "main" loop periodically polls the meassage queues being populated by core 0 and formats them.
  - core 1 also liesurely polls the BME280 and DS18B20 sensors.
  - core 1 sends all the decoded data out on the UART. 
* _**f007t_manchwithdelay.pio**_ in 9 instructions and
* _**wh1080_pwmpulsebits.pio**_ in 16 instructions
  - have state machine programs feeding their FIFOs with data bits for the F007T and WH1080 decoders respectively.
  - they provide limited detection for valid data bits being received.
  - the FIFOs are configured as 32 bits wide and would block on messages that aren't multiples of 32 bits. However the RF AM receivers with automatic gain control generate noise pulses between "good" messages. These spurious bits push the real bits through the FIFOs fairly quickly.  
* _**wh1080_decode_bits.c**_ and
* _**f007t_decode_bits.c**_
  - repeating timer callbacks on core 0 empty the PIO FIFOs and stores the resulting bit stream to bit queues i.e. extending the queue depth from the FIFO limitation. 
  - repeating timer callbacks on core 0 read from the bit queues, "parse" the data bits looking for their respective messages and queue the results to message queues.
  - on core 0 the timer callbacks for the message and bit queues both interact with the bit queues. However they are on the same CPU and currently run at the same priority so the integrity of the bit queue is naturally protected.
  - Functions called from the main loop on core 1 decodes the messages ready for their transmision out through the UART. 
  - The functions called on core 1 main loop and the message queue timer callbacks on core 0 both access the message queue so spin locks are used to ensure integrity of the queue.
* _**ds10b20_1w.pio**_ in 15 instructions
  - has the state machine that implements the one-wire sequences for reset and bit read / write.
  - the state machine idles on a blocking PULL for the bit write sequence.
  - the reset and bit read sequences are entered by injecting JMP instructions. These sequences both finish at the blocking PULL at the start of the bit write 
* _**ds18b20_1w.c**_
  - reads the DS18B20 using mostly PIO control of the one-wire protocol's time critical parts but with the noncritical sequences using injected instructions.
  - tests for the data line erroneously shorted low and requires the device presence pulse to be seen.
  - CPU code detemines that the PIO's state machine is idle by checking the PIO's TX FIFO stalled status.
  - default 12 bit conversion is used and, if the device had reported no power, then the data line is driven high to provide some "parasitic" power during the conversion time (pin drive strength at default) 
  - it is assumed that there is only one device on the bus (because that's all there currently is!). 
* _**bme280_spi.c**_
  - reads the BME280 over SPI. Apart from an awful cludge, the code is largely the published example SPI program unmodified
* _**queues_for_msgs_and_bits.c**_
  - has support routines for message and bit queues. Statistics are collected to measure the performance of the queues and FIFOs and the incoming rate of data bits. 
* _**serial_io.c**_
  - has support routines for the 2nd UART where the message data is sent out over RS232 and data input can be received
  - provide for non-blocking input from stdin
* _**led_control.c**_
  - PWM for the Tiny 2040's three LEDs. The runtime overrides allow
    - the affect of PWM values to be evaluated and 
    - allow current settings table to be cycled through easily.
* _**output_format.c**_
  - defines common format information and prints debug and statistics to std output
* _**proj_board.h**_
  - has the specifics for the particular GPIO/PIO/SM/SPI/UART's used on the board
* other _**\*.h**_ files
  - each _**\*.c**_ file except _**yaesa_rp2040.c**_ has a corresponding _**.h**_ file that provides declarations for the functions and data that they define and share. 

### Remote Sensor Processing Performance Measurement 
- **What to measure**
    - Rate of bits per second through the PIO state machine and the proportion of bits that have
the value ‘1’. This is a rough proportion as it will vary with message content.
    - Rate of message headers per minute with proportion of headers with valid checksum.
    - Rate of messages with valid checksum and that have had two identical consecutive
messages and the proportion of these against all those messages with valid checksums.
    - The delta time between valid messages. Its easy to measure and should, on average, be the
equivalent to the rate.
    - Queue “high water marks”
      - max number of unread 32 bit words found in a FIFO
      - max number of unread 32 bit words found in a bit queue
      - max number of unread messages found in a message queue

The algorithm that looks for identical consecutive messages only allows for a maximum of two and
the F007T transmission which has message in triplicate only counts as one message duplicated and
not two.
- **“Ideal” (no noise) Rates and Proportions**
  - **F007T 433 OOK Manchester**
\- 5 transmitters, each transmitting every 60s, each transmission has the message in triplicate, a triplicated message is 195 bits (87 one's)
    - 5 * 195 = 975 bits per min (435 1's) => 16.25 bits per second (44.6% one's)
    - 5 * 3 headers per 60 seconds => 15 headers per minute with 100% checksum passed
    - verified messages rate for each transmitter is 1 per minute (33.3% consecutive identical seen)
  - **WH1080 868 OOK PWM**
\- 1 transmitter with transmission rate of 48s with every other transmission having the message duplicated. 3 messages headers per 96s; one transmission of 88 bits (44 one's) and one of 176 bits (88 one's)
     - 264 bits per 96 seconds (with 132 one's) => 2.5625 bits per second (50% one's)
     - 3 headers per 96 seconds => 3*60/96 = 1.875 headers per minute with 100% checksum passed
     - verified messages rate 1/3 the header rate => 0.625 per minute (33.3% consecutive identical seen)

- **Results**

The code was instrumented to log the header and message statistics over 30 minute sample periods and the bit rates were recorded every minute. The delta times between valid messages from each individual transmitter were logged as they occurred. The statistics were recorded over multiple 24 hour periods.<br>
The high water marks are not reported in the following tables as the repeating timer callbacks are currently set to quite a high frequency. The maximum number of words found unread in FIFO was 2, the bit queue word max was 4 and the message queue was 2.<br>
The amount of ones for the WH1080 is very high and will inevitably fool the checksum.<br>
Some of the F007T transmitters (TX ID’s 3, 4 and 5) are further away and some are also reporting
low battery.

- **WH1080 868 OOK PWM**
<pre>
|          |                  |  Ideal |   Avg  |   Max  |   Min  |
|:--------:|:----------------:|:------:|:------:|:------:|:------:|
|     Bits | Rate / sec       |   2.56 | 151.70 | 167.40 | 102.40 |
|          | % ones           |  50.00 |  87.78 |  93.40 |  56.80 |
|  Headers | Rate / min       |   1.87 |  16.62 |  19.18 |  13.14 |
|          | % checksum pass  | 100.00 |  10.04 |  12.90 |   7.80 |
| Messages | Rate / min       |   0.62 |   0.50 |   0.60 |   0.30 |
|          | % identical pass |  33.33 |  29.78 |  36.00 |  20.90 |
|          | Delta sec        |  96.00 | 121.19 | 480.10 |  95.40 |
</pre>

- **F007T 433 OOK Manchester**
<pre>
|                 |                  |  Ideal |   Avg  |   Max   |   Min  |
|:---------------:|:----------------:|:------:|:------:|:-------:|:------:|
|            Bits | Rate / sec       |  16.25 | 675.59 |  692.30 | 638.50 |
|                 | % ones           |  44.60 |  46.92 |   48.20 |  45.20 |
|         Headers | Rate / min       |  15.00 |  13.61 |   14.48 |  10.81 |
|                 | % checksum pass  | 100.00 |  92.59 |   98.10 |  87.40 |
| MessagesTX ID 1 | Rate / min       |   1.00 |   1.13 |    1.13 |   1.09 |
|                 | % identical pass |  33.33 |  33.50 |   34.70 |  32.70 |
|                 | Delta sec        |  60.00 |  53.07 |  106.10 |  52.60 |
| MessagesTX ID 2 | Rate / min       |   1.00 |   1.04 |    1.07 |   0.97 |
|                 | % identical pass |  33.33 |  38.03 |   44.10 |  34.40 |
|                 | Delta sec        |  60.00 |  57.65 |  171.00 |  56.70 |
| MessagesTX ID 3 | Rate / min       |   1.00 |   1.00 |    1.03 |   0.17 |
|                 | % identical pass |  33.33 |  35.89 |   46.00 |  33.00 |
|                 | Delta sec        |  60.00 |  59.95 | 1534.00 |  58.60 |
| MessagesTX ID 4 | Rate / min       |   1.00 |   0.97 |    1.00 |   0.90 |
|                 | % identical pass |  33.33 |  43.61 |   48.40 |  38.90 |
|                 | Delta sec        |  60.00 |  61.92 |  183.00 |  60.70 |
| MessagesTX ID 5 | Rate / min       |   1.00 |   0.65 |    0.90 |   0.00 |
|                 | % identical pass |  33.33 |  41.31 |   49.10 |   0.00 |
|                 | Delta sec        |  60.00 |  94.88 | 4287.80 |  66.70 |
</pre>
### To Do
Using 4 wire SPI is a bit generous for the BME280 so will probably switch to I2C
<br>

