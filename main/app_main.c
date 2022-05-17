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
//#include "tcpip_adapter.h"


#define CONFIG_BROKER_HOST "45.77.243.33"
#define CONFIG_BROKER_PORT 1882
#define CONFIG_BROKER_USERNAME "VnTNf7NU0Tp0iZTWtE3W"
#define CONFIG_WIFI_SSID "Fullhouse2"
#define CONFIG_WIFI_PASSWORD "1133557799"

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

uint32_t AIValue;
uint32_t AVValue;

static esp_mqtt_client_handle_t client;
static bool isServerConnected = false;

typedef struct {
	//PacketSendType_t type;
	char pPkt[800]; // UPDATE AFTER SIZE
} QueueServer_t;

static QueueHandle_t queueServerSend = NULL;

static const char *TAG = "SMART PLUG";

void mqtt_pkt_ade_send(uint32_t IRMS, uint32_t VRMS, char *pa_pkt)
{
    char *pktStr = NULL;

    cJSON *retPkt = cJSON_CreateObject();
    cJSON_AddNumberToObject(retPkt, "AI",
			((int) (IRMS * 100)) / 100.0);
    cJSON_AddNumberToObject(retPkt, "AV",
			((int) (VRMS * 100)) / 100.0); 
    pktStr = cJSON_PrintUnformatted(retPkt);
    strcpy(pa_pkt, pktStr);    
    cJSON_Delete(retPkt);
	free(pktStr);   
}

//Task1
void ade_get_measured(void *pParam)
{
    QueueServer_t adePkt;
    memset(adePkt.pPkt, '\0', sizeof(adePkt.pPkt));
    while(1){
   // if(isServerConnected)
        printf("\nT1: ade_get_measured\n");
        AIValue = (rand() % 50) + 5.25;
        AVValue = (rand() % 5) + 220000.35;
        mqtt_pkt_ade_send(AIValue, AVValue, adePkt.pPkt);
        printf("|adePkt: %s|\n", adePkt.pPkt);
        if(xQueueSendToBack(queueServerSend, &adePkt, 0) == pdTRUE)
        {
            printf("\n T1- send queue\n");
        }
        else{
            printf("\n T1- fail send queue\n");
        }
        memset(adePkt.pPkt, '\0', sizeof(adePkt.pPkt));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    
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
          /*  ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id); */
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
        //.uri = "mqtt://45.77.243.33:1882",
        .host = CONFIG_BROKER_HOST, 
        .port = CONFIG_BROKER_PORT, 
        .username = CONFIG_BROKER_USERNAME,
     /* .username = "diripar8@gmail.com",
        .password = "03T*Dl@g" */
        
        /* .event_handle = mqtt_event_handler, */
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    queueServerSend = xQueueCreate(5, sizeof(QueueServer_t));

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    mqtt_app_start();
    
    xTaskCreate(&ade_get_measured, "ade_get_measured", 4000, NULL, 18, NULL);
    xTaskCreate(&server_send, "server_send", 4000, NULL, 18, NULL);


}