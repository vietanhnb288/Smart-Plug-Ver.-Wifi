#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
//#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "sp_ade7753.h"
#include "math.h"

spi_device_handle_t spi;
const uint8_t readingsNum = 10;
/* Init spi */
void init_spiADE7753() {
	esp_err_t ret;
	int8_t pinMISO, pinMOSI, pinCLK, pinCS;
		pinMISO = PIN_SPI_MISO;
		pinMOSI = PIN_SPI_MOSI;
		pinCLK = PIN_SPI_CLK;
		pinCS = PIN_SPI_CS;
	

	printf("SPI pin %d, %d, %d, %d\n", pinMISO, pinMOSI, pinCLK, pinCS);

	// Config bus
	spi_bus_config_t buscfg = { .mosi_io_num = pinMOSI, .miso_io_num = pinMISO,
			.sclk_io_num = pinCLK, .quadhd_io_num = -1, .quadwp_io_num = -1,
			.max_transfer_sz = 10 };

	// Config slave interface
	spi_device_interface_config_t devcfg =
			{ .command_bits = 0, .address_bits = 0, .dummy_bits = 0,
					.queue_size = 2, .clock_speed_hz = SPI_CLK,
					.duty_cycle_pos = 128, .mode = 1, .spics_io_num = pinCS,
					.flags = 0 };

	// Init spi bus
	ret = spi_bus_initialize(VSPI_HOST, &buscfg, DMA_CHAN);

	printf("spi_bus_initialize = %d\n", ret);

	// Add slave to spi bus
	ret = spi_bus_add_device(VSPI_HOST, &devcfg, &spi);

	printf("spi_bus_add_device = %d\n", ret);

//	assert(ret==ESP_OK);
	if (ret != ESP_OK) {
		return false;
	}
	//Pullup pin SPI
	 gpio_set_pull_mode(PIN_SPI_MISO, GPIO_PULLUP_ONLY);
	 gpio_set_pull_mode(PIN_SPI_CLK, GPIO_PULLUP_ONLY);
	 gpio_set_pull_mode(PIN_SPI_CS, GPIO_PULLUP_ONLY);

	return true;
}

/* Write SPI */

void ADE7753_enable()
{
	gpio_set_level(PIN_SPI_CS, 0);
}

void ADE7753_disable()
{
	gpio_set_level(PIN_SPI_CS, 1);
}

void spi_write8(uint8_t address, uint8_t data) {
	esp_err_t ret;
	ADE7753_enable();

	vTaskDelay(10 / portTICK_PERIOD_MS);
	
	uint8_t cmd_hdr;
	uint8_t sendPkt[2];
	uint8_t recvPkt;

	cmd_hdr = address | 0x80;   // 0x80 = 10000000   WRITE
	sendPkt[0] = cmd_hdr;   // address reg tat ca dang set là 1 byte
	sendPkt[1] = data;
	memset(&recvPkt, 0, sizeof(recvPkt));

	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	trs.length = 8;
	trs.tx_buffer = &sendPkt[0];
	trs.rx_buffer = NULL;
	ret = spi_device_polling_transmit(spi, &trs);

	printf("spi_device_polling_transmit address = %d\r\n", ret);

	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	trs.length = 8;
	trs.tx_buffer = &sendPkt[1];
	trs.rx_buffer = NULL;
	ret = spi_device_polling_transmit(spi, &trs);

	printf("spi_device_polling_transmit data = %d\r\n", ret);

	vTaskDelay(5 / portTICK_PERIOD_MS);

	ADE7753_disable();
}

void spi_write16(uint8_t address, uint16_t data) {
	esp_err_t ret;
	ADE7753_enable();
	vTaskDelay(5 / portTICK_PERIOD_MS);

	uint8_t cmd_hdr;
	uint8_t sendPkt[3];
	uint8_t recvPkt;

	cmd_hdr = address | 0x80;   // 0x80 = 10000000   WRITE
	sendPkt[0] = cmd_hdr;
	sendPkt[1] = (data >> 8) & 0xFF;
	sendPkt[2] = (data >> 0) & 0xFF;
	memset(&recvPkt, 0, sizeof(recvPkt));

	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	trs.length = 8;
	trs.tx_buffer = &sendPkt[0];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	trs.length = 8;
	trs.tx_buffer = &sendPkt[1];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	trs.length = 8;
	trs.tx_buffer = &sendPkt[2];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	ADE7753_disable();
}

void spi_write24(uint8_t address, uint32_t data) {
	esp_err_t ret;
	ADE7753_enable();
	vTaskDelay(5 / portTICK_PERIOD_MS);

	uint8_t cmd_hdr;
	uint8_t sendPkt[4];
	uint8_t recvPkt;

	cmd_hdr = address | 0x80;   // 0x80 = 10000000   WRITE
	sendPkt[0] = cmd_hdr;
	sendPkt[1] = (data >> 16)& 0xFF;
	sendPkt[2] = (data >> 8) & 0xFF;
	sendPkt[3] = (data >> 0) & 0xFF;
	memset(&recvPkt, 0, sizeof(recvPkt));

	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt[0];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	memset(&recvPkt, 0, sizeof(recvPkt));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt[1];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	memset(&recvPkt, 0, sizeof(recvPkt));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt[2];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);
	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	memset(&recvPkt, 0, sizeof(recvPkt));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt[3];
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);

	ADE7753_disable();
}

/* Read SPI */
// Format: ||ADDRESS|R|xxx||DATA-16BIT||
uint8_t spi_read8(uint8_t address) {
	esp_err_t ret;
	ADE7753_enable();  // B1: Enable
	vTaskDelay(5 / portTICK_PERIOD_MS);
	uint8_t sendPkt;
	uint8_t recvPkt;
	sendPkt = address;
	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	memset(&recvPkt, 0, sizeof(recvPkt));

	trs.length = 8; 
	trs.tx_buffer = &sendPkt;
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);   //B2: transfer address reg

	memset(&trs, 0, sizeof(trs));
	memset(&sendPkt, 0, sizeof(sendPkt));
	
	vTaskDelay(5 / portTICK_PERIOD_MS);

	trs.length = 8; 
	trs.tx_buffer = &sendPkt;
	trs.rx_buffer = &recvPkt;
	ret = spi_device_polling_transmit(spi, &trs);   //B3: read data from reg

	ADE7753_disable();
	return recvPkt;
	
}


uint16_t spi_read16(uint8_t address) {
	esp_err_t ret;
	uint8_t sendPkt;
	uint8_t recvPkt[2];
	uint8_t dummy; 
	dummy = 0x00;
	ADE7753_enable();  
	vTaskDelay(5 / portTICK_PERIOD_MS);

	sendPkt = address;

	uint16_t res;
	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt;
	trs.rx_buffer = &dummy;
	ret = spi_device_polling_transmit(spi, &trs);

	printf("spi_device_polling_transmit read16 address = %d\r\n", ret);

	vTaskDelay(5 / portTICK_PERIOD_MS);	

	
	memset(&trs, 0, sizeof(trs));
	memset(&recvPkt, 0, sizeof(recvPkt));
	memset(&dummy, 0, sizeof(dummy));
	trs.rxlength = 8;
	trs.length = 8; 
	trs.tx_buffer = &dummy;
	trs.rx_buffer = &recvPkt[0];
	ret = spi_device_polling_transmit(spi, &trs);

	printf("spi_device_polling_transmit read16 read data 1 = %d\r\n", ret);

	vTaskDelay(5 / portTICK_PERIOD_MS);	

	
	memset(&trs, 0, sizeof(trs));
	memset(&dummy, 0, sizeof(dummy));
	trs.rxlength = 8;
	trs.length = 8; 
	trs.tx_buffer = &dummy;
	trs.rx_buffer = &recvPkt[1];
	ret = spi_device_polling_transmit(spi, &trs);

	printf("spi_device_polling_transmit read16 read data 2 = %d\r\n", ret);

	res = (recvPkt[0] << 8) | (recvPkt[1] << 0);      // ((uint16_t)recvPkt[1] << 8) | (recvPkt[2] << 0); ep kieu

	ADE7753_disable();

	return res;


}

// Format: ||ADDRESS|R|xxx||DATA-32BIT||
uint32_t spi_read24(uint8_t address) {
	esp_err_t ret;
	uint8_t dummy; 
	dummy = 0x00;
	ADE7753_enable();
	vTaskDelay(5 / portTICK_PERIOD_MS);

	uint8_t sendPkt;
	uint8_t recvPkt[3];
	uint32_t res;

	sendPkt = address;
	spi_transaction_t trs;
	memset(&trs, 0, sizeof(trs));
	memset(recvPkt, 0, sizeof(recvPkt));
	trs.length = 8; 
	trs.tx_buffer = &sendPkt;
	trs.rx_buffer = &dummy;
	ret = spi_device_polling_transmit(spi, &trs);

	vTaskDelay(5 / portTICK_PERIOD_MS);
	memset(&dummy, 0, sizeof(dummy));
	memset(&trs, 0, sizeof(trs));
	trs.rxlength = 8;
	trs.length = 8; 
	trs.tx_buffer = &dummy;
	trs.rx_buffer = &recvPkt[0];
	ret = spi_device_polling_transmit(spi, &trs);

	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	memset(&dummy, 0, sizeof(dummy));
	trs.rxlength = 8;
	trs.length = 8; 
	trs.tx_buffer = &dummy;
	trs.rx_buffer = &recvPkt[1];
	ret = spi_device_polling_transmit(spi, &trs);

	vTaskDelay(5 / portTICK_PERIOD_MS);

	memset(&trs, 0, sizeof(trs));
	memset(&dummy, 0, sizeof(dummy));
	trs.rxlength = 8;
	trs.length = 8; 
	trs.tx_buffer = &dummy;
	trs.rx_buffer = &recvPkt[2];
	ret = spi_device_polling_transmit(spi, &trs);

	vTaskDelay(5 / portTICK_PERIOD_MS);
	
	res = (recvPkt[0] << 16) | (recvPkt[1] << 8)
			| (recvPkt[2] << 0);



	ADE7753_disable();
	return res;
	
}


///////// 
void ADE7753_gainSetup( uint8_t pgas){
spi_write8(GAIN,pgas);//write GAIN register, format is |3 bits PGA2 gain|2 bits full scale|3 bits PGA1 gain
}

uint8_t ADE7753_getVersion(){
return spi_read8(DIEREV);
}

void ADE7753_setMode(uint16_t m){
    spi_write16(MODE, m);
}
uint16_t ADE7753_getMode(){
    return spi_read16(MODE);
}

/**	getStatus()/resetStatus()/getInterrupts()/setInterrupts(int i)
INTERRUPT STATUS REGISTER (0x0B), RESET INTERRUPT STATUS REGISTER (0x0C), INTERRUPT ENABLE REGISTER (0x0A)
The status register is used by the MCU to determine the source of an interrupt request (IRQ).
When an interrupt event occurs in the ADE7753, the corresponding flag in the interrupt status register is set to logic high.
If the enable bit for this flag is Logic 1 in the interrupt enable register, the IRQ logic output goes active low.
When the MCU services the interrupt, it must first carry out a read from the interrupt status register to determine the source of the interrupt.

*/


uint16_t ADE7753_getInterrupts(void){
    return spi_read16(IRQEN);
}
void ADE7753_setInterrupts(uint16_t i){
    spi_write16(IRQEN,i);
}
uint16_t ADE7753_getStatus(void){
    return spi_read16(STATUSR);
}
uint16_t ADE7753_resetStatus(void){
    return spi_read16(RSTSTATUS);
}

uint32_t ADE7753_getIRMS() {
	uint32_t lastupdate = 0;
	uint8_t t_of = 0;
	spi_write16(0x0A, 0x0010);  // Zerocrossing interrupt enable
	ADE7753_resetStatus();
	lastupdate = esp_timer_get_time();

	while(!(ADE7753_getStatus()&ZX))   {    //ZX=0x0010 = 00010000 : set bit ZX flag
	   if ((esp_timer_get_time() - lastupdate) > 20){
		t_of = 1;
		break;
		}
	}
	if (t_of){
		return 0;
	}else{
		return spi_read24(IRMS_REG);
	}
}

/*
bool ADE7753::CheckandResetZeroCrossing(){
    unsigned long value = Read16(STATUS_U);
    bool check = value & ZX;
    Write16(STATUS_U, value & ~ZX);
    return check;
}

int ADE7753::WaitZeroCross(){  //returns 0 when ZeroCross
    boolean value = 0;
    unsigned long conta_millis = millis();
    CheckandResetZeroCrossing();
    while (!CheckandResetZeroCrossing()) {
        if (millis() > (conta_millis+100)){
            Serial.println("ZX ERROR");
            value = 1;
            delay(100);
            break;
        }    
    }
    return value;
}
*/
uint32_t ADE7753_getVRMS() {
	uint32_t lastupdate = 0;
	uint8_t t_of = 0;
	spi_write16(0x0A, 0x0010);  // Zerocrossing interrupt enable
	ADE7753_resetStatus();
	lastupdate = esp_timer_get_time();

	while(!(ADE7753_getStatus()&ZX))   {    //ZX=0x0010 = 00010000 : set bit ZX flag
	   if ((esp_timer_get_time() - lastupdate) > 20){
		t_of = 1;
		break;
		}
	}
	if (t_of){
		return 0;
	}else{
		return spi_read24(VRMS_REG);
	}
}

float ADE7753_irms()
{
	uint8_t c=0;
	uint32_t i=0;
	if(ADE7753_getIRMS())
	{
		for(c=0;c<readingsNum;c++){
		i+=ADE7753_getIRMS();
	}
	return ((float)i/(float)readingsNum)*0.5/(0.004*1868467*sqrt(2.0));
	}else{
	return 0;
	}
	

}

float ADE7753_vrms()
{
	uint8_t c=0;
	uint32_t v=0;
	if(ADE7753_getVRMS())
	{
		for(c=0;c<readingsNum;c++){
		v+=ADE7753_getVRMS();
	}
	return ((float)v/(float)readingsNum)*0.5*1001/(1561400.0*sqrt(2.0));
	}else{
	return 0;
	}
}


uint32_t ADE7753_getWatt(){
return spi_read24(LAENERGY);
}
uint32_t ADE7753_getVar(){
return spi_read24(LVARENERGY);
}
uint32_t ADE7753_getVa(){
return spi_read24(LVAENERGY);
}

///// khoi tao cac bien de luu thong so
/*
uint32_t IRMS1, IRMS2, IRMSOS, Aconst

void ADE7753_calibIRMS(){
	spi_write16(0x0A, 0x0010);  // Zerocrossing interrupt enable
	while(!(getStatus()&ZX))   {    //ZX=0x0010 = 00010000 : set bit ZX flag
	   // do nothing
	}
	ADE7753_resetStatus();
	while(!(getStatus()&ZX))   {    //ZX=0x0010 = 00010000 : set bit ZX flag
	   // do nothing
	}
	uint32_t IRMS1 = spi_read24(IRMS);
	// lam lan 2 de doc IRMS2
	IRMSOS = 1/32768*(...)
	Aconst = 

}
*/


