#ifndef APP_TASKS_H
#define APP_TASKS_H

void task_alert_entry(void *pvParameters);
void task_sensor_entry(void *pvParameters);
void task_process_entry(void *pvParameters);
void task_display_entry(void *pvParameters);
void task_storage_entry(void *pvParameters);
void task_comms_entry(void *pvParameters);
void task_power_entry(void *pvParameters);

#endif // APP_TASKS_H