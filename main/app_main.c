#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h" 
#include "esp_netif.h"
//#include "protocol_examples_common.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "cJSON.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "sp_ade7753.h"

#include "tcpip_adapter.h"
#include "math.h"

//include "esp_mesh.h"
//#include "esp_mesh_internal.h"


#define CONFIG_BROKER_HOST "45.77.243.33"
#define CONFIG_BROKER_PORT 1883
#define CONFIG_BROKER_USERNAME "5kudauhJkNUfgZ2K4rTK"

#define CONFIG_WIFI_SSID "Fullhouse2"
#define CONFIG_WIFI_PASSWORD "1133557799"

// #define CONFIG_WIFI_SSID "Dung HN 2.4G"
// #define CONFIG_WIFI_PASSWORD "88888888"


// #define CONFIG_WIFI_SSID "The Coffee House"
// #define CONFIG_WIFI_PASSWORD "thecoffeehouse"

// #define CONFIG_WIFI_SSID "Nikola Tesla PhD 2"
// #define CONFIG_WIFI_PASSWORD "anhcungkhongbiet"


#define METHOD_CONTROL "control"


#define QUEUE_SERVER 800 // 800 for rssiPKt[20+24*30+30 = 770 < 800], 200 for adePkt
//#define F 200


//static DeviceVal_t g_deviceInfo;

static QueueHandle_t queueServerRecv = NULL;
static QueueHandle_t queueServerSend = NULL;
static QueueHandle_t queueOnOff = NULL;
//static QueueHandle_t queueMesh = NULL;

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

uint32_t AIValue;
uint32_t AVValue;
uint32_t PValue ;
static esp_mqtt_client_handle_t client;
//static bool isServerConnected = false;

#define METHOD_MEASURED "measured"

typedef struct {
	//PacketSendType_t type;
	char pPkt[800]; // UPDATE AFTER SIZE
} QueueServer_t;

typedef struct {
	uint8_t ctrol; // 0 - OFF/ 1- ON
} QueueOnOff_t;

typedef enum {
	PKT_ADE = 0,
	PKT_STATUS
} PacketSendType_t;

typedef struct {
	int32_t AIReg;
	float AIValue;
	int32_t AVReg;
	float AVValue;
} RMSRegs_t;

typedef struct {
	int32_t activeReg;
	float activeValue;
	int32_t reactiveReg;
	float reactiveValue;
	int32_t apparentReg;
	float apparentValue;
} PowRegs_t;

typedef struct {
	int32_t activeReg;
	float activeValue;
	int32_t reactiveReg;
	float reactiveValue;
	int32_t apparentReg;
	float apparentValue;
} EnergyRegs_t;

static const char *TAG = "SMART PLUG";



typedef enum {
	BTN_STATE_ON,
	BTN_STATE_OFF,

} BtnState_t;

BtnState_t g_btnState;

#define PIN_CTROL_RELAY 26
#define PIN_BTN 27
#define PIN_TEST 19
void gpio_init() {

     // PIN_CTROL_RELAY
	gpio_config_t cfgOutput = { .mode = GPIO_MODE_OUTPUT, .intr_type =
			GPIO_INTR_DISABLE, .pin_bit_mask = (1 << PIN_CTROL_RELAY)
			 }; 
	gpio_config(&cfgOutput);
	gpio_set_level(PIN_CTROL_RELAY, 1);

    // PIN_BTN
	gpio_config_t cfgInput = { .mode = GPIO_MODE_INPUT, .pull_down_en =
			GPIO_PULLDOWN_ENABLE, .intr_type = GPIO_INTR_DISABLE,
			.pin_bit_mask = (1 << PIN_BTN) };
	gpio_config(&cfgInput);


	g_btnState = BTN_STATE_OFF;
	
}


void ctrol_on() {
	gpio_set_level(PIN_CTROL_RELAY, 0);
}

void ctrol_off() {
	gpio_set_level(PIN_CTROL_RELAY, 1);
}

void btn_read_state(void *pvParam) {
	bool pushState = false; //true = push, false = no push
	while (1) {
		if (gpio_get_level(PIN_BTN) == 1) {
			pushState = true;
		} else {
			pushState = false;
		}

		if (pushState) {
			if (g_btnState == BTN_STATE_ON) {
				g_btnState = BTN_STATE_OFF;
			} else if (g_btnState == BTN_STATE_OFF) {
				g_btnState = BTN_STATE_ON;
			}
		}
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}


void mqtt_pkt_ade_send(float IRMS, float VRMS, char *pa_pkt)
{
    char *pktStr = NULL;

    cJSON *retPkt = cJSON_CreateObject();
    cJSON_AddNumberToObject(retPkt, "AI",
			((int) (IRMS * 100 )) / 100.0);
        
    cJSON_AddNumberToObject(retPkt, "AV",
			 ((int) (VRMS * 100 )) / 100.0);
  
    pktStr = cJSON_PrintUnformatted(retPkt);
    strcpy(pa_pkt, pktStr);    
    cJSON_Delete(retPkt);
	free(pktStr);   
}

/* T4: CONTROL_ON_OFF */
void control_on_off(void *pParam) {
	QueueOnOff_t buffOnOff;

	while (1) {
		printf("\nT4: control_on_off\n");

		if (xQueueReceive(queueOnOff, &buffOnOff, 0) == pdPASS) {
			printf("CONTROL FROM QUEUE ON OFF\n");
			if (buffOnOff.ctrol == 0) {
				printf("BTN_STATE_OFF\n");
				g_btnState = BTN_STATE_OFF;
				ctrol_off();
			} else if (buffOnOff.ctrol == 1) {
				printf("BTN_STATE_ON\n");
				g_btnState = BTN_STATE_ON;
				ctrol_on();
			}
		} else {
			if (g_btnState == BTN_STATE_ON) {
				printf("CONTROL FROM BTN ON\n");
				//g_deviceInfo.ctrol = 1;
				ctrol_on();
			} else if (g_btnState == BTN_STATE_OFF) {
				printf("CONTROL FROM BTN OFF\n");
				//g_deviceInfo.ctrol = 0;
				ctrol_off();
			}
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

//Task1
void ade_get_measured(void *pParam)
{
    QueueServer_t adePkt;
    float irms_value, vrms_value;
    memset(adePkt.pPkt, '\0', sizeof(adePkt.pPkt));
    while(1){

        printf("\nT1: ade_get_measured\n");
 
        //irms_value = (float)spi_read24(IRMS_REG)*0.5/(0.001*16*1868467*sqrt(2.0))*1000;  //mA
        
        if((float)spi_read24(IRMS_REG)>1300)
        {
            irms_value = (float)spi_read24(IRMS_REG)*3.16/10000;
        }
        else {
            irms_value = 0;
        }
        vrms_value = (float)(spi_read24(VRMS_REG)*0.5*1021/(1561400.0*sqrt(2.0))-9);  //V
       
        printf("|READ IRMS: %f|\n", irms_value);
        printf("|READ VRMS: %f|\n", vrms_value);

        printf("|READ IRMS_REG: %X|\n", spi_read24(IRMS_REG));
        printf("|READ VRMS_REG: %X|\n", spi_read24(VRMS_REG));

       // irms_value = (rand() % 50) + 5.25;
       // vrms_value = (rand() % 5) + 220000.35;   S
       // AIValue = (rand() % 50) + 5.25;
      //  AVValue = (rand() % 5) + 220000.35;  
        mqtt_pkt_ade_send(irms_value, vrms_value, adePkt.pPkt);
        printf("|adePkt: %s|\n", adePkt.pPkt);
        if(xQueueSendToBack(queueServerSend, &adePkt, 0) == pdTRUE)
        {
            printf("\n T1- send queue\n");
        }
        else{
            printf("\n T1- fail send queue\n");
        }
        memset(adePkt.pPkt, '\0', sizeof(adePkt.pPkt));
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    }
}


//Task2
void server_send(void *pParam)
{   
    QueueServer_t serverSend;
    int32_t msg_id = 0;
    while(1){
    //if(isServerConnected)
        
        if (xQueueReceive(queueServerSend, &serverSend,
						portMAX_DELAY) == pdPASS) {
                printf("\nT2: server_send\n");
                
				if (xQueueReceive(queueServerSend, &serverSend,
						portMAX_DELAY) == pdPASS) {
                printf("\nT2: server_send\n");
                printf("ADEPKT|%s|\n", serverSend.pPkt);
                msg_id = esp_mqtt_client_publish(client, "v1/devices/me/telemetry", serverSend.pPkt, 0, 1, 0); 
                ESP_LOGI(TAG, "T2- publish message, msg_id=%d", msg_id);
            	}
        else{
            printf("\nfail recv queue\n");
        }
                  
        vTaskDelay(100 / portTICK_PERIOD_MS);
    
    }
}
}


void server_recv(void *pParam) {
	char buffServerRecv[200] = { 0, };
	QueueOnOff_t onOff;
	cJSON *recvPkt = NULL;
	cJSON *method = NULL;
	cJSON *param = NULL;

	while (1) {		
			if (xQueueReceive(queueServerRecv, buffServerRecv,
					portMAX_DELAY) == pdPASS) {
				printf("\nT11: server_recv\n");
				printf("PKT|%s|\n", buffServerRecv);
				
					// Process pkt to queue
					recvPkt = cJSON_Parse(buffServerRecv);
					if (recvPkt == NULL) { 
						goto T11end;
					}
					method = cJSON_GetObjectItemCaseSensitive(recvPkt,
							"method");
					if (!cJSON_IsString(method)
							|| (method->valuestring == NULL)) {
						goto T11end;
					}
					param = cJSON_GetObjectItemCaseSensitive(recvPkt, "params");
					if (!cJSON_IsObject(param)) {
						goto T11end;
					}
					if (memcmp(method->valuestring, METHOD_CONTROL,
							strlen(METHOD_CONTROL)) == 0) {
						cJSON *ctrol = cJSON_GetObjectItemCaseSensitive(param,
								"ctrol");
						if (!cJSON_IsNumber(ctrol)) {
							goto T11end;
						}
						onOff.ctrol = ctrol->valueint;
						printf("ctrol %d\n", onOff.ctrol);
						xQueueSendToBack(queueOnOff, &onOff, 0);
					}
					memset(buffServerRecv, '\0',
							sizeof(buffServerRecv));
					T11end: cJSON_Delete(recvPkt);				
			        }	
}
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            ESP_LOGI(TAG,"connect wifi OK");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG,"connect to the AP fail");
            esp_wifi_connect();
            ESP_LOGI(TAG, "retry to connect to the AP");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void wifi_init()
{   
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            //isServerConnected = true;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
         //   msg_id = esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
         //   ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

           	msg_id = esp_mqtt_client_subscribe(client,
				"v1/devices/me/rpc/request/+", 1);
			ESP_LOGI(TAG, "sent subscribe, msg_id = %d", msg_id);	
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
           // isServerConnected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "v1/gateway/telemetry", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            event->data[event->data_len] = '\0';
		/* RECV PACKET FROM SERVER */
		    xQueueSendToBack(queueServerRecv, event->data, 0);

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .host = CONFIG_BROKER_HOST, 
        .port = CONFIG_BROKER_PORT, 
        .username = CONFIG_BROKER_USERNAME,
     
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}



void app_main(void)
{
    queueServerSend = xQueueCreate(5, sizeof(QueueServer_t));
    queueServerRecv = xQueueCreate(5, 200 * sizeof(char));
	queueOnOff = xQueueCreate(2, sizeof(QueueOnOff_t));

	gpio_init();
    
   /* gpio_set_level(PIN_CTROL_RELAY, 0); */

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    mqtt_app_start();   
    gpio_config_t cfgCS = { .mode = GPIO_MODE_OUTPUT, .intr_type =
			GPIO_INTR_DISABLE, .pin_bit_mask = (1 << PIN_SPI_CS)
			 }; 
	gpio_config(&cfgCS);



    init_spiADE7753();


    ADE7753_gainSetup(0x80);
   
    ADE7753_resetStatus();
    ADE7753_setMode(0x000C);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    //ADE7753_enable();
    
    printf("MODE|%x|\n", ADE7753_getMode());
    printf("IRQ|%x|\n",  ADE7753_getInterrupts());
    printf("CFNUM|%x|\n",  spi_read16(0x14));
	printf("LINECYC|%x|\n",  spi_read16(0x1C));
    printf("PHCAL|%x|\n",  spi_read8(0x10));
    printf("SAGCYC|%x|\n",  spi_read8(0x1E));
    printf("IPEAK|%x|\n",  spi_read8(0x22));

	xTaskCreate(&btn_read_state, "btn_read_state", 1000, NULL, 1, NULL); 
    xTaskCreate(&control_on_off, "control_on_off", 3000, NULL, 16,
				NULL);
    xTaskCreate(&ade_get_measured, "ade_get_measured", 4000, NULL, 16, NULL);
    xTaskCreate(&server_send, "server_send", 4000, NULL, 18, NULL);
    xTaskCreate(&server_recv, "server_recv", 4000, NULL, 20,
				NULL);
}