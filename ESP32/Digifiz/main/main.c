/* Digifiz Replica Next
*/
#include <stdio.h>
#include <esp_ota_ops.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Include semaphore/mutex header

#include "digifiz_ws_server.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include <time.h>
#include "led_strip.h"
#include "sdkconfig.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "setup.h"
#include "params.h"
#include "display_next.h"
#include "tacho.h"
#include "adc.h"
#include "speedometer.h"
#include "device_sleep.h"
#include "reg_inout.h"
#include "buzzer.h"
#include "emergency.h"
#include "digifiz_watchdog.h"
#include "vehicle_data.h"

#include "millis.h"

int i = 0;
int saveParametersCounter = 0;
uint16_t displaySpeedCnt = 0;
//Speed related data
uint32_t spd_m = 0;
float spd_m_speedometer = 0;
int spd_m_speedometerCnt = 0; //spd_m_speedometerCnt
float current_averageSpeed = 0;

extern digifiz_pars digifiz_parameters;

//RPM-related data
float averageRPM = 0;
int averageRPMCnt = 0;
uint32_t rpm = 0;
long clockDot;
int test = 1;
int sinceStart_hours = 99;
int sinceStart_minutes = 99;

int16_t seconds_block1;
int16_t seconds_block2;

bool clockRunning;
struct tm saved_time1;
struct tm saved_time2;
uint8_t fuel = 0;

#define MAIN_TASK_DELAY_MS 1000 // 1 second delay
#define DIGIFIZ_TASK_DELAY_MS 1000 // 0.1 second delay
#define ADC_TASK_DELAY_MS 10 // 0.01 second delay
#define DISPLAY_UPDATE_DELAY_MS (1000 / 32) // 32 Hz rate calculation

SemaphoreHandle_t displayMutex; // Mutex to protect access to display

// Get current time from RTC and print hours and minutes
void set_time() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    printf("Current time: %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
    // Convert current time to struct tm
    struct tm *current_time = localtime(&now);

    // Convert both current time and saved time to time_
    time_t saved_t1 = mktime(&saved_time1);
    time_t saved_t2 = mktime(&saved_time2);
    time_t current_time_seconds = mktime(current_time);

    // Calculate the difference in seconds
    double diff_seconds1 = difftime(current_time_seconds, saved_t1)+seconds_block1;
    double diff_seconds2 = difftime(current_time_seconds, saved_t2)+seconds_block2;

    // Convert the difference to hours and minutes

    int diff_minutes = 0;
    
    if (digifiz_parameters.mfaBlock)
    {
        diff_minutes = diff_seconds2 / 60;
    }
    else
    {
        diff_minutes = diff_seconds1 / 60;
    }
    
    int diff_hours = diff_minutes / 60;
    diff_minutes %= 60;

    // Handle negative differences if the saved time is in the future
    if (diff_seconds1 < 0) {
        diff_hours = -diff_hours;
        diff_minutes = -diff_minutes;
    }

    //printf("time:%u:%u\n",timeinfo.tm_hour,timeinfo.tm_min);
    setClockData(timeinfo.tm_hour,timeinfo.tm_min);
    
    sinceStart_hours = diff_hours;
    sinceStart_minutes = diff_minutes;
    digifiz_parameters.duration[0]=(uint16_t)fabs(diff_seconds1);
    digifiz_parameters.duration[1]=(uint16_t)fabs(diff_seconds2);
}

// Shutdown handler function
void shutdown_handler(void)
{
    ESP_LOGI("SHUTDOWN_HANDLER", "Running shutdown handler...");

    // Stop and deinitialize Timer
    xSemaphoreTake(displayMutex, portMAX_DELAY); // Take the mutex
    deinit_leds();
    deinit_tacho_gpio();
    deinit_gptimer();
    // Disconnect and deinitialize Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}


//100 Hz loop
void adcLoop(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(300));
    xSemaphoreTake(displayMutex, portMAX_DELAY); // Take the mutex
    read_initial_adc_values();
    log_sensor_data();
    xSemaphoreGive(displayMutex); // Give back the mutex
    vTaskDelay(pdMS_TO_TICKS(100));
    while(1) {
        // Print current time in hours and minutes
        // Read ADC values from multiple pins
        xSemaphoreTake(displayMutex, portMAX_DELAY); // Take the mutex
        processADC();
        set_coolant_temp_c(getCoolantTemperature());
        set_oil_temp_c(getOilTemperature());
        set_ambient_temp_c(getAmbientTemperature());
        set_speed_kmh(spd_m_speedometer);
        set_rpm(rpm);
        set_fuel_level_l(getLitresInTank());
        set_uptime_h(digifiz_parameters.uptime);
        set_mileage_km(digifiz_parameters.mileage);
        set_fuel_consumption_l100(getFuelConsumption());
        set_intake_voltage_v(getIntakeVoltage());
        set_fuel_pressure_bar(getFuelPressure());
        xSemaphoreGive(displayMutex); // Give back the mutex
        
        vTaskDelay(pdMS_TO_TICKS(ADC_TASK_DELAY_MS));
    }
}

//1 Hz loop
void digifizLoop(void *pvParameters) {
    TickType_t oneHz_lastWakeTime;
    const TickType_t oneHz_rateCalculationPeriod = pdMS_TO_TICKS(DIGIFIZ_TASK_DELAY_MS);
    while(1) {
        oneHz_lastWakeTime = xTaskGetTickCount();
        // Print current time in hours and minutes
        xSemaphoreTake(displayMutex, portMAX_DELAY); // Take the mutex
        set_time();
        setRPMData(averageRPM);
        digifiz_parameters.mileage+=spd_m;
        digifiz_parameters.daily_mileage[digifiz_parameters.mfaBlock]+=spd_m;
#ifdef AVERAGE_CONSUMPTION_L100KM
        digifiz_parameters.averageConsumption[digifiz_parameters.mfaBlock] += 0.01f*(getCurrentIntakeFuelConsumption()-digifiz_parameters.averageConsumption[digifiz_parameters.mfaBlock]);//getFuelConsumption()*digifiz_parameters.tankCapacity;
#endif
#ifdef CURRENT_CONSUMPTION_L100KM
        digifiz_parameters.averageConsumption[digifiz_parameters.mfaBlock] = getCurrentIntakeFuelConsumption();//getFuelConsumption()*digifiz_parameters.tankCapacity;
#endif
        setMileage(uptimeDisplayEnabled ? (digifiz_parameters.uptime/3600) : (digifiz_parameters.mileage/3600)); //to km
#if defined(AUDI_DISPLAY) || defined(AUDI_RED_DISPLAY)
        setDailyMileage((uint16_t)(digifiz_parameters.daily_mileage[digifiz_parameters.mfaBlock]/3600));
#endif

        saveParametersCounter++;
        setBacklight(digifiz_parameters.backlight_on ? true : false);
        if (saveParametersCounter==EEPROM_SAVE_INTERVAL)
        {
            digifiz_parameters.averageSpeed[digifiz_parameters.mfaBlock] = current_averageSpeed;
            saveParameters();
            saveParametersCounter=0;
            //pressMFAMode();
            //setAudiOptions(0x6);
        }
        checkEmergency(rpm);
        setMFABlock(digifiz_parameters.mfaBlock ? 0 : 1); //in display h
        displayMFAType(uptimeDisplayEnabled ? 6 : digifiz_parameters.mfaState);
        setDot(false);
        //printf("Reg in: %u %u %u\n", digifiz_reg_in.bytes[0], digifiz_reg_in.bytes[1], digifiz_reg_in.mfaReset);
        xSemaphoreGive(displayMutex); // Give back the mutex
        
        device_sleep_dump();

        //pressMFAMode();
        // Read ADC values from multiple pins
        //vTaskDelay(pdMS_TO_TICKS(DIGIFIZ_TASK_DELAY_MS));
        
        if (getBuzzerEnabled())
        {
            buzzerToggle();
        }
        vTaskDelayUntil(&oneHz_lastWakeTime, oneHz_rateCalculationPeriod);
    }
}

//32 Hz loop
void displayUpdate(void *pvParameters) {
    TickType_t lastWakeTime;
    const TickType_t rateCalculationPeriod = pdMS_TO_TICKS(DISPLAY_UPDATE_DELAY_MS);
    while(1) {
        lastWakeTime = xTaskGetTickCount();
        xSemaphoreTake(displayMutex, portMAX_DELAY); // Take the mutex
        spd_m = readLastSpeed();
        if (spd_m>0)
        {
            spd_m *= digifiz_parameters.speedCoefficient; //to kmh (or to miles? - why not)
            if (digifiz_parameters.digifiz_options.option_miles)
                spd_m *= 0.6214;
            spd_m /= 100;
        }

#ifndef TESTMODE
        spd_m_speedometer += (spd_m-spd_m_speedometer)*0.5;
#endif
        //For test fuel intake
        //spd_m_speedometer = 60.0f;

        rpm = readLastRPM(); 
        if (rpm>0)
        {
            if((getRPMDispertion()<digifiz_parameters.medianDispFilterThreshold)) //30 or LESS!!!
            {
                //50 Hz - 1500 rev/min
                //rpmCoefficient = 3000 for Golf 2/Mk2 gasoline
                //rpmCoefficient = 1500 for Audi 80 b2
                rpm *= digifiz_parameters.rpmCoefficient/100; //4 cylinder motor, 60 sec in min, 2 strokes per revolution
                
                averageRPM += ((rpm-averageRPM)*digifiz_parameters.rpmFilterK)/1000;
            }
        }
        else
        {
            averageRPM += (0-averageRPM)*0.5;
        }

        

        //For test fuel intake
#ifdef TESTMODE
        averageRPM = 3000.0f;
#endif

        displaySpeedCnt++;
        if (displaySpeedCnt==16) // 2 Hz loop(as on original Digifiz)  
        {
            setSpeedometerData((uint16_t)spd_m_speedometer);
            current_averageSpeed += (spd_m_speedometer-current_averageSpeed)*0.01;
#ifdef TESTMODE
            spd_m_speedometer+=1;
            if (spd_m_speedometer==25)
                spd_m_speedometer=0;
#endif
            displaySpeedCnt = 0;
            printf("Status T RPM:%d %f %f %f %f\n",getDisplayedCoolantTemp(),getCoolantTemperature(),(float)((getCoolantTemperature()-digifiz_parameters.coolantMin)/
                 (digifiz_parameters.coolantMax - digifiz_parameters.coolantMin)*14.0f), averageRPM, spd_m_speedometer);
        }

        if (digifiz_parameters.digifiz_options.option_gallons)
        {
            fuel = getGallonsInTank();
            if (fuel<2)
            setRefuelSign(true);
            else
            setRefuelSign(false);
        }
        else
        {
            fuel = getLitresInTank();
            if (fuel<10)
            setRefuelSign(true);
            else
            setRefuelSign(false);  
        }
 
        setFuel(fuel);
        setCoolantData(getDisplayedCoolantTemp());
        processIndicators();
        if (millis()>2000)
        {
            setBrightness(digifiz_parameters.autoBrightness ? getBrightnessLevel() : digifiz_parameters.brightnessLevel);
        }
        else
        {
            setBrightness(30);
        }

        setRPMData(averageRPM);
        fireDigifiz();
        setMFAType(uptimeDisplayEnabled ? 6 : digifiz_parameters.mfaState);
        processMFA();
        regout_all(digifiz_reg_out.byte);
        regin_read(digifiz_reg_in.bytes);
        //protocolParse();
        xSemaphoreGive(displayMutex); // Give back the mutex
        
        // Wait until it's time to perform the next iteration
        vTaskDelayUntil(&lastWakeTime, rateCalculationPeriod);
    }
}


void initDigifiz(void)
{
    ESP_LOGI(LOG_TAG, "initDigifiz started");
    spd_m = 0;
    rpm = 0;
    spd_m_speedometer = 0;
    spd_m_speedometerCnt = 0;
    averageRPM = 0;
    averageRPMCnt = 0;
    time_t current_time_t;
    time(&current_time_t);
    // Convert current time to struct tm
    localtime_r(&current_time_t, &saved_time1);
    localtime_r(&current_time_t, &saved_time2);
    seconds_block1 = digifiz_parameters.duration[0];
    seconds_block2 = digifiz_parameters.duration[1];
    printf("Sec block1,2: %u %u", seconds_block1, seconds_block2);
    current_averageSpeed = digifiz_parameters.averageSpeed[digifiz_parameters.mfaBlock];
    digifiz_reg_out.byte = 0;
    ESP_LOGI(LOG_TAG, "initDigifiz ended");
}

void digifizWebserver(void *pvParameters)
{
    digifiz_wifi_connect();
    while (1)
    {
        vTaskDelay(MAIN_TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void on_cpu_0(void *pvParameters)
{
    xTaskCreatePinnedToCore(digifizWebserver, "digifizWebserver", 4096, NULL, 3, NULL, 0);
    while (1) 
    {
        vTaskDelay(MAIN_TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void on_cpu_1(void *pvParameters)
{
    displayMutex = xSemaphoreCreateMutex(); // Create the mutex
    initEEPROM(); //Start memory container
    initADC();
    initDisplay();
    initDigifiz();
    initSpeedometer();
    initTacho();
    initDeviceSleep();
    initRegInOut();
    initVehicleJSON();

    xTaskCreatePinnedToCore(digifizLoop, "digifizLoop", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(displayUpdate, "displayUpdate", 4096, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(adcLoop, "adcLoop", 4096, NULL, 3, NULL, 1);

    while (1) 
    {
        vTaskDelay(MAIN_TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // Create the mutex for watchdog synchronization
    watchdog_mutex_create();

    // Initialize the watchdog timer
    if (watchdog_init() != ESP_OK) {
        ESP_LOGE("Watchdog", "Failed to initialize watchdog timer");
        return;
    }

    const esp_partition_t *partition = esp_ota_get_running_partition();
	printf("Currently running partition: %s\r\n", partition->label);
    esp_ota_img_states_t ota_state;
	if (esp_ota_get_state_partition(partition, &ota_state) == ESP_OK) {
		if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
			esp_ota_mark_app_valid_cancel_rollback();
		}
	}
    xTaskCreatePinnedToCore(on_cpu_0, "on_cpu_0", 4096, NULL, 3, NULL,0);
    xTaskCreatePinnedToCore(on_cpu_1, "on_cpu_1", 4096, NULL, 3, NULL,1);
    ESP_ERROR_CHECK(esp_register_shutdown_handler(shutdown_handler));
    while (1) 
    {
        watchdog_update();
        vTaskDelay(MAIN_TASK_DELAY_MS / portTICK_PERIOD_MS);
        //printf("app_main\n");
    }
}
