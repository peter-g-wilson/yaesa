# yaesa
**Y**et **A**nother **E**nvironment **S**ensor **A**ggregator - Remote wireless temperature sensors and remote wireless weather station with temperature/humidity/rain/wind sensors. Local sensor with temperature/humidity/pressure and another local'ish sensor with a temperature sensor at the end of a long wire.

**Using a RP2040 and PIOs -** 
<br>
* Two PIO state machines to detect bit pulse streams from the raw incoming data provided by two RF AM receivers, both with OOK modulation but one Manchester encoded and the other PWM encoded. Receiving data from
  - five remote F007T temperature sensors transmiting at 433 MHz
  - one remote WH1080 weather station transmiting temperature, humidity, rain, wind data and date/time at 866 MHz.
* One PIO state machine to provide a simple very blocking implementation of the one-wire protocol to a parasiticaly powered DS18B20 on a long'ish wire.
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
Now two of the five F007T sensors regularly fail - one likes to send a negative version of the temperature, the other likes 111.7 C a lot (suspiciousli raw temperature value of 0xAAA).
Fortunately its fairly easy to create replacement ones using cheap 8 bit micros, DS18B20s and 433 MHz transmitters. 
<br>
<br>
Then my youngest son inherited a Fine Offset WH1080 weather station from his grandfather ...
### **Overview**
![overview](https://github.com/peter-g-wilson/PICO_PIO/blob/main/images/overview.png)
### **Code**
* _**yaesa_rp2040.c**_  
  - has the CPU core 0 main entry point that calls the WH1080 and F007T timer and PIO initialisations and also has the core 1 entry point to handle the BME280 connected by SPI, the DS18B20 one-wire protocol and the 2nd UART to output the data that's been received.
  - on core 0 the "main" loop does nothing. Its the timer callbacks that empty the PIO FIFOs and that "parse" the bit streams looking for recognisable messages, adding them to the message queues.
  - on core 1 the liesurely "main" loop periodically polls the meassage queues being populated by core 0 and formats them.
  - core 1 also periodically polls the BME280 and DS18B20 sensors.
  - core 1 sends all the decoded data out on the UART. 
* _**f007t_manchwithdelay.pio**_ (9 instructions)
* _**wh1080_pwmpulsebits.pio**_ (16 instructions)
  - have state machine programs feeding their FIFOs with data bits for the F007T and WH1080 decoders respectively.
  - they provide limited detection for valid data bits being received.
  - the FIFOs are configured as 32 bits wide and would block on messages that aren't multiples of 32 bits. However the RF AM receivers with automatic gain control generate noise pulses between "good" messages. These sperious bits push the real bits through the FIFOs fairly quickly.  
* _**wh1080_decode_bits.c**_
* _**f007t_decode_bits.c**_
  - use repeating timer callbacks on core 0 to empty the PIO FIFOs and stores the resulting bit stream to bit queues i.e. extending the queue depth from the FIFO limitation. 
  - repeating timer callbacks on core 0 read from the bit queues, "parse" the data bits looking for their respective messages and queue the results to message queues.
  - on core 0 the timer callbacks for the message and bit queues both interact with the bit queues. However they currently run at the same priority so the integrity of the bit queue is naturally protected.
  - The core 1 main loop and the message queue timer callbacks on core 0 both access the message queue so locks are used to ensure integrity of the queue.
  - Functions called from the main loop on core 1 decodes the messages ready for their transmision out through the UART. 
* _**ds10b20_1w.pio**_ (15 instructions)
  - has the state machine that implements the one-wire sequences for reset and bit read / write.
  - the state machine idles on a blocking PULL for the bit write sequence.
  - the reset and bit read sequences are entered by injecting JMP instructions. These sequences both finish at the blocking PULL at the start of the bit write 
* _**ds18b20_1w.c**_
  - reads the DS18B20 using mostly PIO control of the one-wire protocol's time critical parts but with the none critical sequences using injected instructions.
  - it tests for the data line erroneously shorted low and for a device presence pulse.
  - the CPU code detemines that the PIO's state machine is idle by checking the TX FIFO stalled status.
  - default 12 bit conversion is used and, if the device had reported no power, then the data line is driven high during the conversion time.
  - it is assumed that there is only one device on the bus (because that's all there is!). 
* _**bme280_spi.c**_
  - reads the BME280 over SPI. Apart from an awful cludge, the code is largely the published example SPI program unmodified
* _**queues_for_msgs_and_bits.c**_
  - has support routines for message and bit queues. Statistics are collected to measure the performance of the queues and FIFOs and the incoming rate of data bits. 
* _**uart_io.c**_
  - has support routines for the 2nd UART where the message data is sent out over RS232
* _**output_format.c**_
  - defines common format information and prints debug and statistics to std output
* _**proj_board.h**_
  - has the specifics for the particular gpio/PIO/SM/SPI/UART's used on the board
* other _**\*.h**_ files
  - each _**\*.c**_ file except _**yaesa_rp2040.c**_ has a corresponding _**.h**_ file that provides declarations for the functions and data that they define. 
### **To Do**
Using 4 wire SPI is a bit generous for the BME280 so will probably switch to I2C
<br>

### **More details and performance results are in -**
![PICO_PIO_OOK_Manchester_and_PWM.pdf](https://github.com/peter-g-wilson/PICO_PIO/blob/main/pdf/PICO_PIO_OOK_Manchester_and_PWM.pdf)
<br>
