# yaesa
* _**Y**_et _**A**_nother _**E**_nvironment _**Sensor _**A**_ggregator - remote temperature sensors, remote weather station temperature/humidity/rain/wind, local temperature/humidity/pressure and local'ish temperature.
<br>
Using RP2040 - 
<br>
with PIO to detect bit pulse streams from two RF receivers - both OOK but one manchester and the other PWM encoded
<br>
to receive from five remote F007T temperature sensors transmiting at 433 MHz
<br>
and one WH1080 transmiting temperature, humidity, rain, wind data and date/time at 866 MHz.
<br>
Also a local BME280 with temperature, humidity and pressure currently using a version of the Raspberry PI (Trading) Ltd.'s SPI example code.
<br>
And a parasiticaly powered DS18B20 on a long'ish wire using a simple and very blocking PIO implementation. 
### Why do it?
I had a fridge that kept freezing vegetables. It was interesting to see how bad the overshoot and undershoot of the controller was.
<br>
And why not monitor the freezer too. It had a much tighter controller.
<br>
The attic was getting extra loft insulation and I wondered how cold the water tank now got.
<br>
There is a water softener in the garage that didn't want to get too cold.
<br>
So I got 5 Ambient Weather F007T sensors with base station.
The base station was rubbish, invariably displaying 'HH', impossible temperatures and push button switches from the 1960's.
<br>
Now two of the five F007T sensors regularly fail - one likes to send a negative version of the temperature, the other likes 111.7 C a lot.
Fortunately its fairly easy to create replacement ones using a cheap 8 bit micro, DS18B20 and a 433 MHz transmitter. 
<br>
<br>
Then my youngest son inherited a Fine Offset WH1080 weather station from his grandfather ...
### **Overview**
![overview](https://github.com/peter-g-wilson/PICO_PIO/blob/main/images/overview.png)
### **Code**
* _**yaesa_rp2040.c**_  has the CPU core 0 main entry point that calls the WH1080 and F007T timer and PIO initialisations and also has the core 1 entry point to handle the BME280 connected by spi, the DS10B20 one-wire protocol and the 2nd UART to output the data that's been received.
* _**manchWithDelay.pio**_ and _**PWMpulseBits.pio**_ have two state machine programs feeding their FIFOs with data bits for the F007T and WH1080 decoders respectively
* _**ds10b20_1w.pio**_ has the state machine that implements the one-wire commands for reset and the read and write bit
* _**wh1080_decode_bits.c**_ and _**f007t_decode_bits.c**_ use repeating timer callbacks to read the PIO FIFOs and "parse" the data bits looking for their respective messages
* _**bme280_spi.c**_ reads the BME280 over spi.
* _**ds18b20_1w,c**_ reads the DS18B20 using PIO control of the one-wire protocol 
* _**queues_for_msgs_and_bits.c**_ has support routines for message and bit queues
* _**uart_IO.c**_ has support routines for the 2nd UART where the message data is sent out over RS232
* _**output_format.c**_ prints to std output (1st UART) debug and statistics
### **More details and performance results are in -**
![PICO_PIO_OOK_Manchester_and_PWM.pdf](https://github.com/peter-g-wilson/PICO_PIO/blob/main/pdf/PICO_PIO_OOK_Manchester_and_PWM.pdf)
<br>
