﻿/*
* Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
*
* Alibaba Group retains all right, title and interest (including all
* intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have
* obtained a separate written license from Alibaba Group., you are not
* authorized to utilize all or a part of this computer program for any
* purpose (including reproduction, distribution, modification, and
* compilation into object code), and you must immediately destroy or
* return to Alibaba Group all copies of this computer program.  If you
* are licensed by Alibaba Group, your rights to utilize this computer
* program are limited by the terms of that license.  To obtain a license,
* please contact Alibaba Group.
*
* This computer program contains trade secrets owned by Alibaba Group.
* and, unless unauthorized by Alibaba Group in writing, you agree to
* maintain the confidentiality of this computer program and related
* information and to not disclose this computer program and related
* information to any other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
* INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
* PURPOSE, TITLE, AND NONINFRINGEMENT.
*/
#include "c_types.h"
#include "alink_export.h"
#include "alink_json.h"
#include <stdio.h>
#include <string.h>
#include "alink_export_rawdata.h"
#include "esp_common.h"
#include "user_config.h"

#include "ledctl.h" 
#include "hnt_interface.h"

#if USER_VIRTUAL_DEV_TEST
#include "user_smartled.h" 
#else
#include "user_uart.h"
#endif

#define wsf_deb  os_printf
#define wsf_err os_printf

deviceParameterTable_t DeviceCustomParamTable = {NULL, 0};
customInfo_t *DeviceCustomInfo = NULL;

extern void alink_sleep(int);
/*do your job here*/
/*这里是一个虚拟的设备,将5个设备属性对应的值保存到全局变量,真实的设备需要去按照实际业务处理这些属性值 */

VIRTUAL_DEV virtual_device;// = {0x01, 0x30, 0x50, 0, 0x01};

//const char *main_dev_params =
  //  "{\"attrSet\": [ \"OnOff_Power\", \"Color_Temperature\", \"Light_Brightness\", \"TimeDelay_PowerOff\", \"WorkMode_MasterLight\"], \"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";
char *device_attr[5] = { "OnOff_Power", "Color_Temperature", "Light_Brightness",
	"TimeDelay_PowerOff", "WorkMode_MasterLight"
};   // this is a demo json package data, real device need to update this package

const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%s\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";

static char device_status_change = 1;

void debug_printf_data(char *data, int len)
{
	int i = 0;
	os_printf("\n*******print len[%d]******\n",len);
	for(i=0; i<len; i++)
	{
		os_printf("%02x ",*(data+i));
	}
	os_printf("\n__________end_________\n");
	return;
}


#define buffer_size 256
static int ICACHE_FLASH_ATTR alink_device_post_data(alink_down_cmd_ptr down_cmd)
{
	alink_up_cmd up_cmd;
	int ret = ALINK_ERR;
	char *buffer = NULL;
    deviceParameter_t *deviceParam;  
    int i;
    int len = 0;    
    char value_char[16];
    
	if (device_status_change) {
		wsf_deb("##[%s][%s|%d]Malloc %u. Available memory:%d.\n", __FILE__, __FUNCTION__, __LINE__,
			buffer_size, system_get_free_heap_size());

		buffer = (char *)malloc(buffer_size);
		if (buffer == NULL)
			return -1;

		memset(buffer, 0, buffer_size);
#if USER_VIRTUAL_DEV_TEST
        eSmartLedGetPower(NULL,value_char);

		sprintf(buffer, main_dev_params, value_char,
			virtual_device.temp_value, virtual_device.light_value,
			virtual_device.time_delay, virtual_device.work_mode);
#else
        len = sprintf(buffer, "{");

        for (i = 0, deviceParam = DeviceCustomParamTable.table; 
             i < DeviceCustomParamTable.tableSize;
             i++, deviceParam++) {        
                if(deviceParam->getParameterFunc){
                    deviceParam->getParameterFunc(deviceParam->paramName, value_char);
                    len += sprintf(buffer+len, "\"%s\": { \"value\": \"%s\" },",
                        deviceParam->paramName,value_char);                    
                    }
        }
        len = len-1;//strip last ","
        len += sprintf(buffer+len, "}");
#endif

		up_cmd.param = buffer;
		if (down_cmd != NULL) {
			up_cmd.target = down_cmd->account;
			up_cmd.resp_id = down_cmd->id;
		} else {
			up_cmd.target = NULL;
			up_cmd.resp_id = -1;
		}
		ret = alink_post_device_data(&up_cmd);
        
		if (buffer)
			free(buffer);

		if (ret == ALINK_ERR) {
			wsf_err("post failed!\n");
			alink_sleep(2000);
		} else {
			wsf_deb("dev post data success!\n");
			device_status_change = 0;
		}

		wsf_deb("##[%s][%s][%d]  Free |Aviable Memory|%5d| \n", __FILE__, __FUNCTION__, __LINE__,
			system_get_free_heap_size());

		stack_free_size();
	}
	return ret;

}

/* do your job end */

int sample_running = ALINK_TRUE;

/*get json cmd from server 服务器下发命令,需要设备端根据具体业务设定去解析处理*/
int ICACHE_FLASH_ATTR main_dev_set_device_status_callback(alink_down_cmd_ptr down_cmd)
{

	json_value *jptr;
	json_value *jstr;
	json_value *jstr_value;
	int value = 0, i = 0;
	char *value_str = NULL;
    deviceParameter_t *deviceParam;  
    int ret = 0;

	wsf_deb("%s %d \n",__FUNCTION__,__LINE__);
	wsf_deb("%s %d\n%s\n",down_cmd->uuid,down_cmd->method, down_cmd->param);

	jptr = json_parse(down_cmd->param, strlen(down_cmd->param));


#if USER_VIRTUAL_DEV_TEST
    char power_char[8];
    int power_value;

	for (i = 0; i < 5; i++) 
	{
		jstr = json_object_object_get_e(jptr, device_attr[i]);
		jstr_value = json_object_object_get_e(jstr, "value");
		value_str = json_object_to_json_string_e(jstr_value);
		if (value_str) {
			value = atoi(value_str);
			switch (i) {
			case 0:
                eSmartLedGetPower(NULL,power_char);
                power_value = atoi(power_char);
				if (power_value != value) {
					eSmartLedSetPower(NULL,value_str);
				}
				break;
			case 1:
				if (virtual_device.temp_value != value) {
					virtual_device.temp_value = value;
				}
				break;
			case 2:
				if (virtual_device.light_value != value) {
					virtual_device.light_value = value;
				}
				break;
			case 3:
				if (virtual_device.time_delay != value) {
					virtual_device.time_delay = value;
				}
				break;
			case 4:
				if (virtual_device.work_mode != value) {
					virtual_device.work_mode = value;
				}
				break;
			default:
				break;
			}
		}
	}
#else
        for (i = 0,deviceParam = DeviceCustomParamTable.table; 
            i < DeviceCustomParamTable.tableSize; 
            i++, deviceParam++) 
        {
            jstr = json_object_object_get_e(jptr, deviceParam->paramName);
            jstr_value = json_object_object_get_e(jstr, "value");
            value_str = json_object_to_json_string_e(jstr_value);
                        
            if (value_str) {
                wsf_deb("%s %s\n",deviceParam->paramName,value_str);
                
                if(deviceParam->setParameterFunc)
                    ret = deviceParam->setParameterFunc(deviceParam->paramName, value_str);
            }
        }
#endif

	json_value_free(jptr);
	device_status_change = 1;   // event send current real device status to the alink server

	return 0;		// alink_device_post_data(down_cmd);
	/* do your job end! */
}

/*服务器查询设备状态,需要设备上报状态*/
int ICACHE_FLASH_ATTR main_dev_get_device_status_callback(alink_down_cmd_ptr down_cmd)
{
	wsf_deb("%s %d \n", __FUNCTION__, __LINE__);
	wsf_deb("%s %d\n%s\n", down_cmd->uuid, down_cmd->method, down_cmd->param);

	if(!device_status_change)
		device_status_change = 1;   // event send current real device status to the alink server

	return 0;		//alink_device_post_data(down_cmd);
}

/* 根据不同系统打印剩余内存,用于平台调试 */
int ICACHE_FLASH_ATTR print_mem_callback(void *a, void *b)
{
	int ret = 0;
	ret = system_get_free_heap_size();
	os_printf("heap_size %d\n", ret);
	return ret;
}

#ifdef PASS_THROUGH
/* device response server command,用户需要自己实现这个函数,处理服务器下发的指令*/
/* this sample save cmd value to virtual_device*/
static int ICACHE_FLASH_ATTR execute_cmd(const char *rawdata, int len)
{
	int ret = 0, i = 0;
	if (len < 1)
		ret = -1;
	for (i = 0; i < len; i++) {
		wsf_deb("%2x ", rawdata[i]);
		switch (i) {
		case 2:
			if (virtual_device.power != rawdata[i]) {
				virtual_device.power = rawdata[i];
			}
			break;
		case 4:
			if (virtual_device.temp_value != rawdata[i]) {
				virtual_device.temp_value = rawdata[i];
			}
			break;
		case 5:
			if (virtual_device.light_value != rawdata[i]) {
				virtual_device.light_value = rawdata[i];
			}
			break;
		case 6:
			if (virtual_device.time_delay != rawdata[i]) {
				virtual_device.time_delay = rawdata[i];
			}
			break;
		case 3:
			if (virtual_device.work_mode != rawdata[i]) {
				virtual_device.work_mode = rawdata[i];
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

/*获取设备信息,需要用户实现 */
static int ICACHE_FLASH_ATTR get_device_status(char *rawdata, int len)
{
	/* do your job here */
	int ret = 0;
	if (len > 7) {
		rawdata[0] = 0xaa;
		rawdata[1] = 0x07;
		rawdata[2] = virtual_device.power;
		rawdata[3] = virtual_device.work_mode;
		rawdata[4] = virtual_device.temp_value;
		rawdata[5] = virtual_device.light_value;
		rawdata[6] = virtual_device.time_delay;
		rawdata[7] = 0x55;
	} else {
		ret = -1;
	}
	/* do your job end */
	return ret;
}

static unsigned int delta_time = 0;

/*主动上报设备状态,需要用户自己实现*/
int ICACHE_FLASH_ATTR alink_device_post_raw_data(void)
{
	/* do your job here */
	int len = 8, ret = 0;
	char rawdata[8] = { 0 };
	if (device_status_change) {

		ESP_DBG(("********POST DATA*************\n"));
		wsf_deb("[%s][%d|  Available memory:%d.\n", __FILE__, __LINE__,system_get_free_heap_size());

		delta_time = system_get_time() - delta_time;
		wsf_deb("%s %d \n delta_time = %d ", __FUNCTION__, __LINE__, delta_time / 1000);
		get_device_status(rawdata, len);

		ret = alink_post_device_rawdata(rawdata, len);
		device_status_change = 0;

		if (ret) {
			wsf_err("post failed!\n");
		} else {
			wsf_deb("dev post raw data success!\n");
		}
	}
	/* do your job end */
	return ret;
}

/*透传方式服务器查询指令回调函数*/

int ICACHE_FLASH_ATTR rawdata_get_callback(const char *in_rawdata, int in_len, char *out_rawdata, int *out_len)
{
	//TODO: 下发指令到MCU
	int ret = 0;
	wsf_deb("%s %d \n", __FUNCTION__, __LINE__);
	//ret=alink_device_post_raw_data(); //  此例是假设能同步获取到虚拟设备数据, 实际应用中,处理服务器指令是异步方式,需要厂家处理完毕后主动上报一次设备状态
// do your job end!

	if(!device_status_change)
		device_status_change = 1;   // event send current real device status to the alink server

	return ret;
}

/*透传方式服务器下发指令回调函数*/
/*实际应用中,处理服务器指令是异步方式,需要厂家处理完毕后主动上报一次设备状态*/
int ICACHE_FLASH_ATTR rawdata_set_callback(char *rawdata, int len)
{
	// TODO: 
	//get cmd from server, do your job here!
	int ret = 0;
	wsf_deb("%s %d \n", __FUNCTION__, __LINE__);
	ret = execute_cmd(rawdata, len);
	//ret=alink_device_post_raw_data();
	// do your job end!
	delta_time = system_get_time();

	if(!device_status_change)
		device_status_change = 1;   // event send current real device status to the alink server
	return ret;
}

#endif //PASS_THROUGH

/*alink-sdk ״̬��ѯ�ص�����*/
int ICACHE_FLASH_ATTR alink_handler_systemstates_callback(void *dev_mac, void *sys_state)
{
	char uuid[33] = { 0 };
	char *mac = (char *)dev_mac;
	enum ALINK_STATUS *state = (enum ALINK_STATUS *)sys_state;
	switch (*state) {
	case ALINK_STATUS_LINK_DOWN:
		sprintf(uuid, "%s", alink_get_uuid(NULL));
		wsf_deb("ALINK_STATUS_LINK_DOWN, mac %s uuid %s\n", mac, uuid);
		break;          
	case ALINK_STATUS_INITED:
		sprintf(uuid, "%s", alink_get_uuid(NULL));
		wsf_deb("ALINK_STATUS_INITED, mac %s uuid %s \n", mac, uuid);
        if(STATION_GOT_IP == wifi_station_get_connect_status())
            wifi_led_set_status(WIFI_LED_CONNECTED_AP);
		break;
	case ALINK_STATUS_REGISTERED:
		sprintf(uuid, "%s", alink_get_uuid(NULL));
		wsf_deb("ALINK_STATUS_REGISTERED, mac %s uuid %s \n", mac, uuid);
		break;
	case ALINK_STATUS_LOGGED:
		sprintf(uuid, "%s", alink_get_uuid(NULL));
		wsf_deb("ALINK_STATUS_LOGGED, mac %s uuid %s\n", mac, uuid);
        wifi_led_set_status(WIFI_LED_CONNECTED_SERVER);
		break;
	case ALINK_STATUS_LOGOUT:
		sprintf(uuid, "%s", alink_get_uuid(NULL));
		wsf_deb("ALINK_STATUS_LOGOUT, mac %s uuid %s\n", mac, uuid);
		break;        
	default:
		break;
	}
	return 0;
}


/* fill device info 这里设备信息需要修改对应宏定义,其中DEV_MAC和DEV_CHIPID 需要用户自己实现接口函数*/
void ICACHE_FLASH_ATTR alink_fill_deviceinfo(struct device_info *deviceinfo)
{
	uint8 macaddr[6];
	strcpy(deviceinfo->name, DeviceCustomInfo->name);
	strcpy(deviceinfo->sn, DeviceCustomInfo->sn);
	strcpy(deviceinfo->brand, DeviceCustomInfo->brand);
	strcpy(deviceinfo->key, DeviceCustomInfo->key);
	strcpy(deviceinfo->model, DeviceCustomInfo->model);
	strcpy(deviceinfo->secret, DeviceCustomInfo->secret);
	strcpy(deviceinfo->type, DeviceCustomInfo->type);
	strcpy(deviceinfo->version, DeviceCustomInfo->version);
	strcpy(deviceinfo->category, DeviceCustomInfo->category);
	strcpy(deviceinfo->manufacturer, DeviceCustomInfo->manufacturer);
	strcpy(deviceinfo->key_sandbox, DeviceCustomInfo->key_sandbox);
	strcpy(deviceinfo->secret_sandbox, DeviceCustomInfo->secret_sandbox);

	if (wifi_get_macaddr(0, macaddr)) {
		wsf_deb("macaddr=%02x:%02x:%02x:%02x:%02x:%02x\n", MAC2STR(macaddr));
		snprintf(deviceinfo->mac, sizeof(deviceinfo->mac), "%02x:%02x:%02x:%02x:%02x:%02x", MAC2STR(macaddr));
	} else
		strcpy(deviceinfo->mac, "19:FE:34:A2:C7:1A");

	snprintf(deviceinfo->cid, sizeof(deviceinfo->cid), "%024d", system_get_chip_id());
	wsf_deb("DEV_MODEL:%s \n", DeviceCustomInfo->model);
}


//#define GET_ALIGN_STRING_LEN(str)    ((strlen(str) + 3) & ~3)
/*ALINK_CONFIG_LEN 2048, len need < ALINK_CONFIG_LEN */
static int ICACHE_FLASH_ATTR write_config(unsigned char *buffer, unsigned int len)
{
	int res = 0, pos = 0;
	ESP_DBG(("...write cfg....."));

	if (buffer == NULL) {
		return ALINK_ERR;
	}
	if (len > ALINK_CONFIG_LEN)
		len = ALINK_CONFIG_LEN;

	res = spi_flash_erase_sector(LFILE_START_ADDR / 4096);	//one sector is 4KB 
	if (res != SPI_FLASH_RESULT_OK) {
		wsf_err("erase flash data fail\n");
		return ALINK_ERR;
	} else {
		wsf_err("erase flash data %d Byte\n", res);
	}
	os_printf("write data:\n");

	debug_printf_data(buffer, len);
	ESP_DBG((">>>>>>>>>>>>>>>"));

	res = spi_flash_write((LFILE_START_ADDR), (uint32 *) buffer, len);
	if (res != SPI_FLASH_RESULT_OK) {
		wsf_err("write data error\n");
		return ALINK_ERR;
	}
	wsf_deb("write key(%s) success.", buffer);
	return ALINK_OK;
}

 /*ALINK_CONFIG_LEN 2048, len need < ALINK_CONFIG_LEN */
static int ICACHE_FLASH_ATTR read_config(unsigned char *buffer, unsigned int len)
{

	int res = 0;
	int pos = 0;
	ESP_DBG(("...test read cfg len [%d].....", len));
	res = spi_flash_read(LFILE_START_ADDR, (uint32 *) buffer, len);
	debug_printf_data(buffer, len);
	ESP_DBG((">>>>>>>>>>>>>>>"));
	if (res != SPI_FLASH_RESULT_OK) {
		wsf_err("read flash data error\n");
		return ALINK_ERR;
	}
	os_printf("read data:\n");
	return ALINK_OK;
}

int ICACHE_FLASH_ATTR alink_get_debuginfo(info_type type, char *status)
{
	int used;  
	switch (type) {    
		case MEMUSED:    
			used = 100 - ((system_get_free_heap_size()*100)/(96*1024));   
			sprintf(status, "%d%%", used);    
			break;    
		case WIFISTRENGTH:    
			sprintf(status , "%ddB",wifi_station_get_rssi());    
			break;    
		default:    
			status[0] = '\0';    
			break;  
	}  
	return 0;
}
int esp_ota_firmware_update( char * buffer, int len)
{
    os_printf("esp_ota_firmware_update \n");
   return upgrade_download(buffer , len);
}

int esp_ota_upgrade(void)
{
    os_printf("esp_ota_upgrade \n");
    system_upgrade_reboot();
    return 0;
}
extern int need_notify_app;
extern int  need_factory_reset ;

void set_thread_stack_size(struct thread_stacksize * p_thread_stacksize)
{
    p_thread_stacksize->alink_main_thread_size = 0xc00;
    p_thread_stacksize->send_work_thread_size = 0x800;
    p_thread_stacksize->wsf_thread_size = 0x1000;
    p_thread_stacksize->func_thread_size = 0x800;
}
   
int ICACHE_FLASH_ATTR alink_demo()
{
	struct device_info main_dev;
#if USER_UART_CTRL_DEV_EN
	dbg_get_recv_times = 0;
#endif
	memset(&main_dev, 0, sizeof(main_dev));
	alink_fill_deviceinfo(&main_dev);	// 必须根据PRD表格更新设备信息
	alink_set_loglevel(ALINK_LL_DEBUG | ALINK_LL_INFO | ALINK_LL_WARN | ALINK_LL_ERROR);
	//alink_set_loglevel(ALINK_LL_ERROR);
	//alink_enable_sandbox_mode();                                      // 线上环境需要注解此函数
    main_dev.sys_callback[ALINK_FUNC_SERVER_STATUS] = alink_handler_systemstates_callback;
	alink_set_callback(ALINK_FUNC_AVAILABLE_MEMORY, print_mem_callback);

	/* ALINK_CONFIG_LEN 2048 */
	alink_register_cb(ALINK_FUNC_READ_CONFIG, (void *)&read_config);
	alink_register_cb(ALINK_FUNC_WRITE_CONFIG, (void *)&write_config);
	alink_register_cb(ALINK_FUNC_GET_STATUS, alink_get_debuginfo);
	//alink_enable_sandbox(&main_dev);                                      // 线上环境需要注解此函数
    alink_register_cb(ALINK_FUNC_OTA_FIRMWARE_SAVE, esp_ota_firmware_update);
    alink_register_cb(ALINK_FUNC_OTA_UPGRADE, esp_ota_upgrade);
	/*start alink-sdk */
    set_thread_stack_size(&g_thread_stacksize);
#ifdef PASS_THROUGH		
	alink_start_rawdata(&main_dev, rawdata_get_callback, rawdata_set_callback);
#else // 非透传方式(设备与服务器采用json格式数据通讯)
	main_dev.dev_callback[ACB_GET_DEVICE_STATUS] = main_dev_get_device_status_callback;
	main_dev.dev_callback[ACB_SET_DEVICE_STATUS] = main_dev_set_device_status_callback;
	
	alink_start(&main_dev);	//register main device here
#endif //PASS_THROUGH

	os_printf("%s %d wait time=%d \n", __FUNCTION__, __LINE__, ALINK_WAIT_FOREVER);

	alink_wait_connect(NULL, ALINK_WAIT_FOREVER);	//wait main device login, -1 means wait forever

	if(need_notify_app) {
		need_notify_app = 0;
		uint8 macaddr[6] ={0};
		char mac[17+1] = {0};
		if (wifi_get_macaddr(0, macaddr)) {
			os_printf("macaddr=%02x:%02x:%02x:%02x:%02x:%02x\n", MAC2STR(macaddr));
			snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", MAC2STR(macaddr));
			os_printf("aws_notify_app %s %s\n", vendor_get_model(),mac);
//            aws_notify_app(vendor_get_model(), mac, ""); // if not factory reset , 
            aws_notify_app();

        }
	}


	/* 设备主动上报数据 */
	
	if(!device_status_change)
		device_status_change = 0x01;
    
	while(sample_running) {

		//os_printf("%s %d \n",__FUNCTION__,__LINE__);
#ifdef PASS_THROUGH
		alink_device_post_raw_data();
#else
		alink_device_post_data(NULL);
#endif //PASS_THROUGH

		if(need_factory_reset) {
		wsf_deb("key to factory_reset\n");
			need_factory_reset = 0;
		alink_factory_reset();
		}
		alink_sleep(1000);
	}

	/*  设备退出alink-sdk */
	alink_end();

	return 0;
}

void ICACHE_FLASH_ATTR
hnt_device_status_change(void)
{
    device_status_change = 1;   
}

char *platform_get_os_version(char os_ver[PLATFORM_OS_VERSION_LEN])
{
    return strncpy(os_ver, "1.3.0(68c9e7b", PLATFORM_OS_VERSION_LEN);
}
char *platform_get_module_name(char name_str[PLATFORM_MODULE_NAME_LEN])
{
    return strncpy(name_str,"WME66",PLATFORM_MODULE_NAME_LEN);
}

void ICACHE_FLASH_ATTR
hnt_param_array_regist(deviceParameter_t *custom_param_array, uint32 array_size)
{
    DeviceCustomParamTable.tableSize = array_size;
    DeviceCustomParamTable.table = custom_param_array;
}

void ICACHE_FLASH_ATTR
hnt_custom_info_regist(customInfo_t *customInfo)
{
    DeviceCustomInfo = customInfo;   
}

char* vendor_get_model(void)
{
    return DeviceCustomInfo->model;
}
char* vendor_get_alink_secret(void)
{
    return DeviceCustomInfo->secret;
}

