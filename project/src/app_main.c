/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/**
 * \mainpage User Application template doxygen documentation
 *
 * \par Empty user application template
 *
 * Bare minimum empty user application template
 *
 * \par Content
 *
 * -# Include the ASF header files (through asf.h)
 * -# Minimal main function that starts with a call to board_init()
 * -# "Insert application code here" comment
 *-pipe -fno-strict-aliasing -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror-implicit-function-declaration -Wpointer-arith -std=gnu99 -ffunction-sections -fdata-sections -Wchar-subscripts -Wcomment -Wformat=2 -Wimplicit-int -Wmain -Wparentheses -Wsequence-point -Wreturn-type -Wswitch -Wtrigraphs -Wunused -Wuninitialized -Wunknown-pragmas -Wfloat-equal -Wundef -Wshadow -Wbad-function-cast -Wwrite-strings -Wsign-compare -Waggregate-return  -Wmissing-declarations -Wformat -Wmissing-format-attribute -Wno-deprecated-declarations -Wpacked -Wredundant-decls -Wnested-externs -Wlong-long -Wunreachable-code -Wcast-align --param max-inline-insns-single=500
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
//#include <hsf.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zc_hf_adpter.h>
#include <zc_common.h>
#include <ac_hal.h>
void HF_BcInit(void);


static int lver(pat_session_t s,int argc,char *argv[],char *rsp,int len)
{
u8 u8WifiVerSion[4] ={0};
    u8WifiVerSion[0] = (u8)(ZC_MODULE_VERSION >> 16);
    u8WifiVerSion[1] = (u8)(ZC_MODULE_VERSION >> 8);
    u8WifiVerSion[2] = (u8)(ZC_MODULE_VERSION);
    sprintf(rsp,"{\"Type\":\"%s\",\"Verion\":\"%d.%d.%d\",\"BuildTime\":\"%s %s\"}\r\n",
                  hfsys_get_sdk_version(),
                  u8WifiVerSion[0],
                  u8WifiVerSion[1],
                  u8WifiVerSion[2],
                  __TIME__,__DATE__);
	return 0;

}

const hfat_cmd_t user_define_at_cmds_table[]=
{
    {"LVER", lver, "   AT+LVER\r\n", NULL},
    {NULL,NULL,NULL,NULL} //the last item must be null
};
#if 0
static int USER_FUNC socketa_recv_callback(uint32_t event,char *data,uint32_t len,uint32_t buf_len)
{
	if(event==HFNET_SOCKETA_DATA_READY)
	{
		HF_Debug(DEBUG_LEVEL_LOW,"socketa recv %d bytes %d\n",len,buf_len);
	}
	else if(event==HFNET_SOCKETA_CONNECTED)
		u_printf("socket a connected!\n");
	else if(event==HFNET_SOCKETA_DISCONNECTED)
		u_printf("socket a disconnected!\n");
	
	return len;
}

static int USER_FUNC socketb_recv_callback(uint32_t event,char *data,uint32_t len,uint32_t buf_len)
{
    if(event==HFNET_SOCKETB_DATA_READY)
        HF_Debug(DEBUG_LEVEL_LOW,"socketb recv %d bytes %d\n",len,buf_len);
    else if(event==HFNET_SOCKETB_CONNECTED)
        u_printf("socket b connected!\n");
    else if(event==HFNET_SOCKETB_DISCONNECTED)
        u_printf("socket b disconnected!\n");
            
    return len;
}

#endif
extern u32 g_u32GloablIp;
void app_init(void)
{
    u_printf("ex app_init \n");
    HF_Debug(DEBUG_LEVEL_USER,"app init\n");
}

static void show_reset_reason(void)
{
    uint32_t reset_reason=0;
    
    reset_reason = hfsys_get_reset_reason();
    
    
#if 0
    u_printf("reset_reasion:%08x\n",reset_reason);
#else   
    if(reset_reason&HFSYS_RESET_REASON_ERESET)
    {
        u_printf("ERESET\n");
    }
    if(reset_reason&HFSYS_RESET_REASON_IRESET0)
    {
        u_printf("IRESET0\n");
    }
    if(reset_reason&HFSYS_RESET_REASON_IRESET1)
    {
        u_printf("IRESET1\n");
    }
    if(reset_reason==HFSYS_RESET_REASON_NORMAL)
    {
        u_printf("RESET NORMAL\n");
    }
    if(reset_reason&HFSYS_RESET_REASON_WPS)
    {
        u_printf("RESET FOR WPS\n");
    }
    if(reset_reason&HFSYS_RESET_REASON_SMARTLINK_START)
    {
        u_printf("RESET FOR SMARTLINK START\n"); 

    }
    if(reset_reason&HFSYS_RESET_REASON_SMARTLINK_OK)
    {
        u_printf("RESET FOR SMARTLINK OK\n");
        HF_ReadDataFromFlash((u8 *)&g_struZcConfigDb,sizeof(g_struZcConfigDb));
        g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
        g_struZcConfigDb.struDeviceInfo.u32UnBcFlag = 0xFFFFFFFF;
        g_struZcConfigDb.u32Crc = crc16_ccitt(((u8 *)&g_struZcConfigDb) + 4, sizeof(g_struZcConfigDb) - 4);        
        HF_WriteDataToFlash((u8 *)&g_struZcConfigDb, sizeof(ZC_ConfigDB));
    }
    if(reset_reason&HFSYS_RESET_REASON_WPS_OK)
    {
        u_printf("RESET FOR WPS OK\n");
    }
#endif
    
    return;
}
static int hfsys_event_callback( uint32_t event_id,void * param) 
{ 
    switch(event_id) 
    { 
        case HFE_WIFI_STA_CONNECTED: 
            u_printf("wifi sta connected!!\n"); 
            break; 
        case HFE_WIFI_STA_DISCONNECTED: 
            u_printf("wifi sta disconnected!!\n"); 
            HF_Sleep();
            HF_BcInit();
            break; 
        case HFE_DHCP_OK: 
        { 
            g_u32GloablIp = *((uint32_t*)param); 
            g_u32GloablIp = ZC_HTONL(g_u32GloablIp);
            u_printf("dhcp ok %08X!\n",g_u32GloablIp); 
            
            HF_WakeUp();
        } 
            break; 
        case HFE_SMTLK_OK: 
            u_printf("smtlk ok!\n"); 
            break; 
        case HFE_CONFIG_RELOAD: 
            u_printf("reload!\n"); 
            break; 
        default: 
            break; 
    } 
    return 0; 
} 
  
static int USER_FUNC uart_recv_callback(uint32_t event,char *data,uint32_t len,uint32_t buf_len) 
{
#ifdef ZC_EASY_UART 
    AC_UartRecv((u8 *)data, len);  
#else
    ZC_Moudlefunc((u8 *)data, len);
#endif
    return len; 
} 

void uart_config(void)
{
	char buffer[64];
    int ret, resetflag = 0;
	
	ret = hfat_send_cmd("AT+UART\r\n", sizeof("AT+URAT\r\n"), buffer, 64);
    if(strcmp(buffer + 4, "115200,8,1,None,NFC") != 0)
    {
        ret = hfat_send_cmd("AT+UART=115200,8,1,NONE,NFC,0\r\n", sizeof("AT+UART=115200,8,1,NONE,NFC,0\r\n"), buffer, 64);
        if(ret != HF_SUCCESS)
        {
        	u_printf("Failed to set UART0");
        }
        resetflag = 1;
    }

	if(resetflag == 1)
    {
        hfsys_softreset();
    }
}

int USER_FUNC app_main (void)
{
	hfdbg_set_level(10);
    HF_Debug(DEBUG_LEVEL,"sdk version(%s),the app_main start time is %s %s\n",hfsys_get_sdk_version(),__DATE__,__TIME__);
    HF_Debug(DEBUG_LEVEL_USER,"hello hf\n");
    if(hfgpio_fmap_check(module_type)!=0)
    {
        while(1)
        {
            HF_Debug(DEBUG_ERROR,"gpio map file error\n");
            msleep(1000);
        }
    }

    show_reset_reason();
    uart_config();
    while(!hfnet_wifi_is_active())
    {
        msleep(50);
    }    
    HF_Init();
    
    hfsys_register_system_event( (hfsys_event_callback_t)hfsys_event_callback);
    
    #if 0
    int up_result=0;
    up_result = hfupdate_auto_upgrade();
    if(up_result<0)
    {
        u_printf("no entry the auto upgrade mode\n");
    }
    else if(up_result==0)
    {
        u_printf("upgrade success\n");
    }
    else
    {
        u_printf("upgrade fail %d\n",up_result);
    }
    #endif
    
    if(hfnet_start_assis(ASSIS_PORT)!=HF_SUCCESS)
    {
        HF_Debug(DEBUG_WARN,"start httpd fail\n");
    }
    
    if(hfnet_start_uart(HFTHREAD_PRIORITIES_LOW,(hfnet_callback_t)uart_recv_callback)!=HF_SUCCESS)
    {
        HF_Debug(DEBUG_WARN,"start uart fail!\n");
    }
    #if 0
    if(hfnet_start_socketa(HFTHREAD_PRIORITIES_LOW,(hfnet_callback_t)socketa_recv_callback)!=HF_SUCCESS)
    {
        HF_Debug(DEBUG_WARN,"start socketa fail\n");
    }
    if(hfnet_start_socketb(HFTHREAD_PRIORITIES_LOW,(hfnet_callback_t)socketb_recv_callback)!=HF_SUCCESS)
    {
        HF_Debug(DEBUG_WARN,"start socketb fail\n");
    }
    #endif
    if(hfnet_start_httpd(HFTHREAD_PRIORITIES_MID)!=HF_SUCCESS)
    {
        HF_Debug(DEBUG_WARN,"start httpd fail\n");
    }
    
    return 1;
}

