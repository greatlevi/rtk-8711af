/**
******************************************************************************
* @file     zc_hf_adpter.c
* @authors  cxy
* @version  V1.0.0
* @date     10-Sep-2014
* @brief    Event
******************************************************************************
*/
#include <zc_protocol_controller.h>
#include <zc_timer.h>
#include <zc_module_interface.h>
#include <zc_hf_adpter.h>
#include <stdlib.h>
#include <stdio.h> 
#include <stdarg.h>
#include <ac_cfg.h>
#include <ac_api.h>
#include "sockets.h"
#include "wifi_conf.h"
#include "timer_api.h"
#include "sys.h"
#include "task.h"
#include "netdb.h" 
#include "sockets.h"
#include "flash_api.h"
#include "wifi_constants.h"
#include "rtl8195a.h"
#include "zc_common.h"

gtimer_t g_struTimer1;

extern PTC_ProtocolCon  g_struProtocolController;
PTC_ModuleAdapter g_struHfAdapter;

MSG_Buffer g_struRecvBuffer;
MSG_Buffer g_struRetxBuffer;
MSG_Buffer g_struClientBuffer;


MSG_Queue  g_struRecvQueue;
MSG_Buffer g_struSendBuffer[MSG_BUFFER_SEND_MAX_NUM];
MSG_Queue  g_struSendQueue;

u8 g_u8MsgBuildBuffer[MSG_BULID_BUFFER_MAXLEN];
u8 g_u8ClientSendLen = 0;

u16 g_u16TcpMss;
u16 g_u16LocalPort;

u8 g_u8recvbuffer[HF_MAX_SOCKET_LEN];
ZC_UartBuffer g_struUartBuffer;
HF_TimerInfo g_struHfTimer[ZC_TIMER_MAX_NUM];
sys_mutex_t g_struTimermutex;
u8  g_u8BcSendBuffer[100];
u32 g_u32BcSleepCount = 800;
struct sockaddr_in struRemoteAddr;

u16 g_u16TiTimerCount[ZC_TIMER_MAX_NUM];
flash_t cloud_flash;

u32 newImg2Addr = 0xFFFFFFFF;
u32 oldImg2Addr = 0xFFFFFFFF;

extern int write_ota_addr_to_system_data(flash_t *flash, u32 ota_addr);
/*************************************************
* Function: HF_ReadDataFormFlash
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_ReadDataFromFlash(u8 *pu8Data, u16 u16Len) 
{
    if (1 != flash_stream_read(&cloud_flash, FLASH_ADDRESS, u16Len, pu8Data))
    {
        ZC_Printf("HF_ReadDataFromFlash error\n\r");
    } 
}
/*************************************************
* Function: HF_WriteDataToFlash
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_WriteDataToFlash(u8 *pu8Data, u16 u16Len)
{
    flash_erase_sector(&cloud_flash, FLASH_ADDRESS);
    if (1 != flash_stream_write(&cloud_flash, FLASH_ADDRESS, u16Len, pu8Data))
    {
        ZC_Printf("flash_stream_write error\n\r");
    }
    else
    {
        ZC_Printf("flash_stream_write successed\n\r");
    }
}
/*************************************************
* Function: HF_timer_callback
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_timer_callback(void *pContext) 
{
    u8 u8TimeId;
    u8TimeId = *(u8 *)pContext;
    sys_mutex_lock(g_struTimermutex);

    TIMER_TimeoutAction(u8TimeId);
    TIMER_StopTimer(u8TimeId);
    
    sys_mutex_unlock(g_struTimermutex);

}
/*************************************************
* Function: HF_StopTimer
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_StopTimer(u8 u8TimerIndex)
{
    g_struHfTimer[u8TimerIndex].u8ValidFlag = 0;
}

/*************************************************
* Function: HF_SetTimer
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_SetTimer(u8 u8Type, u32 u32Interval, u8 *pu8TimeIndex)
{
    u8 u8TimerIndex;
    u32 u32Retval;
    u32Retval = TIMER_FindIdleTimer(&u8TimerIndex);

    if (ZC_RET_OK == u32Retval)
    {
        TIMER_AllocateTimer(u8Type, u8TimerIndex, (u8*)&g_struHfTimer[u8TimerIndex]);
        g_struHfTimer[u8TimerIndex].u32Interval = u32Interval / 1000;
        g_struHfTimer[u8TimerIndex].u8ValidFlag = 1;
        g_u16TiTimerCount[u8TimerIndex] = 0;
        *pu8TimeIndex = u8TimerIndex;
    }
    return u32Retval;

}
/*************************************************
* Function: HF_StopTimer
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void Timer_callback(void) 
{
    u32  u32i = 0;
    for (u32i = 0; u32i < ZC_TIMER_MAX_NUM; u32i++)
    {
        if (g_struHfTimer[u32i].u8ValidFlag)
        {
            if (g_struHfTimer[u32i].u32Interval == g_u16TiTimerCount[u32i]++)
            {
                g_u16TiTimerCount[u32i] = 0;
                TIMER_TimeoutAction(u32i);
                TIMER_StopTimer(u32i);
            }
        }
    }
}
/*************************************************
* Function: HF_ReadNewImg2Addr
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_ReadNewImg2Addr(void)
{
	flash_t	flash;
	u32 NewImg2Addr = 0;	
	u32 Img2Len = 0;	
	u32 IMAGE_x = 0, ImgxLen = 0, ImgxAddr = 0;
    
	u32 SigImage0,SigImage1;
	u32 Part1Addr=0xFFFFFFFF, Part2Addr=0xFFFFFFFF, ATSCAddr=0xFFFFFFFF;
	u32 OldImg2Addr;	
    
	u32 ota_addr = 436*1024;  //0x80000 0x6d000
    ZC_Printf("HF_ReadNewImg2Addr\n\r");
	// The upgraded image2 pointer must 4K aligned and should not overlap with Default Image2
	flash_read_word(&flash, IMAGE_2_BASE, &Img2Len);
	IMAGE_x = IMAGE_2_BASE + Img2Len + 0x10;
	flash_read_word(&flash, IMAGE_x, &ImgxLen);
	flash_read_word(&flash, IMAGE_x+4, &ImgxAddr);
	if(ImgxAddr == 0x30000000)
    {
		printf("\n\r[%s] IMAGE_3 0x%x Img3Len 0x%x", __FUNCTION__, IMAGE_x, ImgxLen);
	}
    else
    {
		printf("\n\r[%s] no IMAGE_3", __FUNCTION__);
		// no image3
		IMAGE_x = IMAGE_2_BASE;
		ImgxLen = Img2Len;
	}
	if((ota_addr > IMAGE_x) && ((ota_addr < (IMAGE_x + ImgxLen)))
        || (ota_addr < IMAGE_x)
        || ((ota_addr & 0xfff) != 0)
        || (ota_addr == ~0x0))
    {
		printf("\n\r[%s] illegal ota addr 0x%x", __FUNCTION__, ota_addr);
		newImg2Addr = 0xFFFFFFFF;
		return;
	}
    else
    {
        printf("write_ota_addr_to_system_data\n\r");
		write_ota_addr_to_system_data( &flash, ota_addr);
    }
	//Get upgraded image 2 addr from offset
	flash_read_word(&flash, FLASH_SYSTEM_DATA_ADDR, &NewImg2Addr);
	if((NewImg2Addr > IMAGE_x) && ((NewImg2Addr < (IMAGE_x+ImgxLen)))
        || (NewImg2Addr < IMAGE_x)
        || ((NewImg2Addr & 0xfff) != 0)
        || (NewImg2Addr == ~0x0))
    {
		printf("\n\r[%s] Invalid OTA Address 0x%x", __FUNCTION__, NewImg2Addr);
		newImg2Addr = 0xFFFFFFFF;
		return;
	}

	flash_read_word(&flash, 0x18, &Part1Addr); // Part1Addr-default image2 address
	Part1Addr = (Part1Addr & 0xFFFF) * 1024;	// first partition
	Part2Addr = NewImg2Addr;
	//printf("1.Part1Addr is 0x%08x,Part2Addr is 0x%08x\n\r", Part1Addr, Part2Addr);
    //printf("1.OldImg2Addr is 0x%08x,NewImg2Addr is 0x%08x\n\r", OldImg2Addr, NewImg2Addr);
	// read Part1/Part2 signature
	flash_read_word(&flash, Part1Addr+8, &SigImage0);
	flash_read_word(&flash, Part1Addr+12, &SigImage1);
	printf("\n\r[%s] Part1 Sig %x", __FUNCTION__, SigImage0);
	if(SigImage0==0x30303030 && SigImage1==0x30303030)
		ATSCAddr = Part1Addr;		// ATSC signature
	else if(SigImage0==0x35393138 && SigImage1==0x31313738)	
		OldImg2Addr = Part1Addr;	// newer version, change to older version
	else
		NewImg2Addr = Part1Addr;	// update to older version	
	//printf("2.Part1Addr is 0x%08x,Part2Addr is 0x%08x\n\r", Part1Addr, Part2Addr);
    //printf("2.OldImg2Addr is 0x%08x,NewImg2Addr is 0x%08x\n\r", OldImg2Addr, NewImg2Addr);
	flash_read_word(&flash, Part2Addr+8, &SigImage0);
	flash_read_word(&flash, Part2Addr+12, &SigImage1);
	printf("\n\r[%s] Part2 Sig %x", __FUNCTION__, SigImage0);
	if(SigImage0==0x30303030 && SigImage1==0x30303030)
		ATSCAddr = Part2Addr;		// ATSC signature
	else if(SigImage0==0x35393138 && SigImage1==0x31313738)
		OldImg2Addr = Part2Addr;
	else
		NewImg2Addr = Part2Addr;
	//printf("3.Part1Addr is 0x%08x,Part2Addr is 0x%08x\n\r", Part1Addr, Part2Addr);
    //printf("3.OldImg2Addr is 0x%08x,NewImg2Addr is 0x%08x\n\r", OldImg2Addr, NewImg2Addr);	
	// update ATSC clear partitin first
	if(ATSCAddr != ~0x0){
		OldImg2Addr = NewImg2Addr;
		NewImg2Addr = ATSCAddr;
	}
	oldImg2Addr = OldImg2Addr;
	printf("\n\r[%s] New %x, Old %x", __FUNCTION__, NewImg2Addr, OldImg2Addr);	

	newImg2Addr = NewImg2Addr;
    printf("oldImg2Addr is %d, newImg2Addr is %d\n\r", oldImg2Addr, newImg2Addr);
}
/*************************************************
* Function: HF_CheckImg2Addr
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_CheckImg2Addr(u32 u32TotalLen)
{
	flash_t flash;
    s32 fileBlkSize;
    u32 u32i;

	if (newImg2Addr == 0xFFFFFFFF)
    {
		printf("\n\r[%s] illegal ota addr 0x%x", __FUNCTION__, newImg2Addr);
		return ZC_RET_ERROR;			
	}		
	if (oldImg2Addr != 0xFFFFFFFF && newImg2Addr < oldImg2Addr)
    {
		if (u32TotalLen > (oldImg2Addr - newImg2Addr))   // firmware size too large
        {   
			printf("\n\r[%s] Part1 size < OTA size", __FUNCTION__);
			return ZC_RET_ERROR;
		}
	}
    else if(u32TotalLen > (FLASH_ADDRESS - newImg2Addr))
    {
		printf("\n\r[%s] Part2 size < OTA size", __FUNCTION__);
		return ZC_RET_ERROR;
	}
	//erase sector
	fileBlkSize = ((u32TotalLen - 1) / 4096) + 1;
	for(u32i = 0; u32i < fileBlkSize; u32i++)
    {
		flash_erase_sector(&flash, newImg2Addr + u32i * 4096);	
	}
    return ZC_RET_OK;
}

/*************************************************
* Function: HF_FirmwareUpdateFinish
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_FirmwareUpdateFinish(u32 u32TotalLen)
{
    int i = 0;
    int ret = -1 ;
    flash_t flash;
    u32 NewImg2Addr = newImg2Addr;
    u32 OldImg2Addr = oldImg2Addr;

    char custom_sig[32] = "Customer Signature-modelxxx";
    u32 read_custom_sig[8];

    for(i = 0; i < 8; i ++)
    {
        flash_read_word(&flash, NewImg2Addr + 0x28 + i *4, read_custom_sig + i);
    }
    printf("\n\r[%s] read_custom_sig %s", __FUNCTION__ , (char*)read_custom_sig);

    // compare checksum with received checksum
    //if(!memcmp(&checksum,file_info,sizeof(checksum))
    if(!strcmp((char*)read_custom_sig,custom_sig))
    {
        //Set signature in New Image 2 addr + 8 and + 12
        u32 sig_readback0,sig_readback1;
        flash_write_word(&flash,NewImg2Addr + 8, 0x35393138);
        flash_write_word(&flash,NewImg2Addr + 12, 0x31313738);
        flash_read_word(&flash, NewImg2Addr + 8, &sig_readback0);
        flash_read_word(&flash, NewImg2Addr + 12, &sig_readback1);
        printf("\n\r[%s] signature %x,%x", __FUNCTION__ , sig_readback0, sig_readback1);

        if(OldImg2Addr != ~0x0)
        {
            flash_write_word(&flash,OldImg2Addr + 8, 0x35393130);
            flash_write_word(&flash,OldImg2Addr + 12, 0x31313738);
            flash_read_word(&flash, OldImg2Addr + 8, &sig_readback0);
            flash_read_word(&flash, OldImg2Addr + 12, &sig_readback1);
            printf("\n\r[%s] old signature %x,%x", __FUNCTION__ , sig_readback0, sig_readback1);
        }
	
        printf("\n\r[%s] Update OTA success!", __FUNCTION__);
        ret = 0;
    }
    if(!ret)
    {
        printf("\n\r[%s] Ready to reboot", __FUNCTION__);   
        ota_platform_reset(); 
    }
    return 0;
}
/*************************************************
* Function: HF_FirmwareUpdate
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_FirmwareUpdate(u8 *pu8FileData, u32 u32Offset, u32 u32DataLen)
{
    int ret;
    flash_t flash;
    //ZC_Printf("HF_FirmwareUpdate: newImg2Addr is %d, oldImg2Addr is %d\n\r", newImg2Addr, oldImg2Addr);
    ret = flash_stream_write(&flash, newImg2Addr + u32Offset, u32DataLen, pu8FileData);

    if(1 != ret)
    {
        ZC_Printf("HF_FirmwareUpdate :write flash fail\r\n");
        return ZC_RET_ERROR;
    }
    return ZC_RET_OK;
}
/*************************************************
* Function: HF_SendDataToMoudle
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_SendDataToMoudle(u8 *pu8Data, u16 u16DataLen)
{
    AC_RecvMessage((ZC_MessageHead *)pu8Data);
    return ZC_RET_OK;
}
/*************************************************
* Function: HF_Rest
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Rest(void)
{   
	int argc=0;
	char *argv[2] = {0};
    g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
    g_struZcConfigDb.struDeviceInfo.u32UnBcFlag = 0xFFFFFFFF;
    g_struZcConfigDb.struSwitchInfo.u32SecSwitch = 1;
    memcpy(g_struZcConfigDb.struCloudInfo.u8CloudAddr, "test.ablecloud.cn", ZC_CLOUD_ADDR_MAX_LEN);
    ZC_ConfigUnBind(ZC_MAGIC_FLAG);
    cmd_simple_config(argc, argv);
}
/*************************************************
* Function: HF_SendTcpData
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_SendTcpData(u32 u32Fd, u8 *pu8Data, u16 u16DataLen, ZC_SendParam *pstruParam)
{
    send(u32Fd, pu8Data, u16DataLen, 0);
}
/*************************************************
* Function: HF_SendUdpData
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_SendUdpData(u32 u32Fd, u8 *pu8Data, u16 u16DataLen, ZC_SendParam *pstruParam)
{
    sendto(u32Fd,(char*)pu8Data,u16DataLen,0,
        (struct sockaddr *)pstruParam->pu8AddrPara,
        sizeof(struct sockaddr_in)); 
}

/*************************************************
* Function: HF_CloudRecvfunc
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
static void HF_CloudRecvfunc(void* arg) 
{
    s32 s32RecvLen=0; 
    fd_set fdread;
    u32 u32Index;
    u32 u32Len=0; 
    u32 u32ActiveFlag = 0;
    struct sockaddr_in cliaddr;
    int connfd;
    extern u8 g_u8ClientStart;
    u32 u32MaxFd = 0;
    struct timeval timeout; 
    struct sockaddr_in addr;
    int tmp=1;    
    s32 s32ret = 0;

    while(1) 
    {
        ZC_StartClientListen();

        u32ActiveFlag = 0;
        
        timeout.tv_sec= 0; 
        timeout.tv_usec= 1000; 
        
        FD_ZERO(&fdread);

        FD_SET(g_Bcfd, &fdread);
        u32MaxFd = u32MaxFd > g_Bcfd ? u32MaxFd : g_Bcfd;

        if (PCT_INVAILD_SOCKET != g_struProtocolController.struClientConnection.u32Socket)
        {
            FD_SET(g_struProtocolController.struClientConnection.u32Socket, &fdread);
            u32MaxFd = u32MaxFd > g_struProtocolController.struClientConnection.u32Socket ? u32MaxFd : g_struProtocolController.struClientConnection.u32Socket;
            u32ActiveFlag = 1;
        }
        
        if ((g_struProtocolController.u8MainState >= PCT_STATE_WAIT_ACCESSRSP) 
        && (g_struProtocolController.u8MainState < PCT_STATE_DISCONNECT_CLOUD))
        {
            FD_SET(g_struProtocolController.struCloudConnection.u32Socket, &fdread);
            u32MaxFd = u32MaxFd > g_struProtocolController.struCloudConnection.u32Socket ? u32MaxFd : g_struProtocolController.struCloudConnection.u32Socket;
            u32ActiveFlag = 1;
        }

        for (u32Index = 0; u32Index < ZC_MAX_CLIENT_NUM; u32Index++)
        {
            if (0 == g_struClientInfo.u32ClientVaildFlag[u32Index])
            {
                FD_SET(g_struClientInfo.u32ClientFd[u32Index], &fdread);
                u32MaxFd = u32MaxFd > g_struClientInfo.u32ClientFd[u32Index] ? u32MaxFd : g_struClientInfo.u32ClientFd[u32Index];
                u32ActiveFlag = 1;            
            }
        }


        if (0 == u32ActiveFlag)
        {
            continue;
        }
        
        s32ret = select(u32MaxFd + 1, &fdread, NULL, NULL, &timeout);
        if(s32ret<=0)
        {
           continue; 
        }
        if ((g_struProtocolController.u8MainState >= PCT_STATE_WAIT_ACCESSRSP) 
        && (g_struProtocolController.u8MainState < PCT_STATE_DISCONNECT_CLOUD))
        {
            if (FD_ISSET(g_struProtocolController.struCloudConnection.u32Socket, &fdread))
            {
                s32RecvLen = recv(g_struProtocolController.struCloudConnection.u32Socket, g_u8recvbuffer, HF_MAX_SOCKET_LEN, 0); 
                
                if(s32RecvLen > 0) 
                {
                    ZC_Printf("recv data len = %d\n", s32RecvLen);
                    MSG_RecvDataFromCloud(g_u8recvbuffer, s32RecvLen);
                }
                else
                {
                    ZC_Printf("recv error, len = %d\n",s32RecvLen);
                    PCT_DisConnectCloud(&g_struProtocolController);
                    
                    g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
                    g_struUartBuffer.u32RecvLen = 0;
                }
            }
            
        }

        
        for (u32Index = 0; u32Index < ZC_MAX_CLIENT_NUM; u32Index++)
        {
            if (0 == g_struClientInfo.u32ClientVaildFlag[u32Index])
            {
                if (FD_ISSET(g_struClientInfo.u32ClientFd[u32Index], &fdread))
                {
                    s32RecvLen = recv(g_struClientInfo.u32ClientFd[u32Index], g_u8recvbuffer, HF_MAX_SOCKET_LEN, 0); 
                    if (s32RecvLen > 0)
                    {
                        ZC_RecvDataFromClient(g_struClientInfo.u32ClientFd[u32Index], g_u8recvbuffer, s32RecvLen);
                    }
                    else
                    {   
                        ZC_ClientDisconnect(g_struClientInfo.u32ClientFd[u32Index]);
                        close(g_struClientInfo.u32ClientFd[u32Index]);
                    }
                    
                }
            }
            
        }

        if (PCT_INVAILD_SOCKET != g_struProtocolController.struClientConnection.u32Socket)
        {
            if (FD_ISSET(g_struProtocolController.struClientConnection.u32Socket, &fdread))
            {
                connfd = accept(g_struProtocolController.struClientConnection.u32Socket,(struct sockaddr *)&cliaddr,&u32Len);

                if (ZC_RET_ERROR == ZC_ClientConnect((u32)connfd))
                {
                    close(connfd);
                }
                else
                {
                    ZC_Printf("accept client = %d\n", connfd);
                }
            }
        }

        if (FD_ISSET(g_Bcfd, &fdread))
        {
            tmp = sizeof(addr); 
            s32RecvLen = recvfrom(g_Bcfd, g_u8BcSendBuffer, 100, 0, (struct sockaddr *)&addr, (socklen_t*)&tmp); 
            if(s32RecvLen > 0) 
            {
                ZC_SendClientQueryReq(g_u8BcSendBuffer, (u16)s32RecvLen);
            } 
        }
        
    } 
}
/*************************************************
* Function: HF_GetMac
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_GetMac(u8 *pu8Mac)
{
    u8 mac[32] = {0};
    u8 macTmp[12] = {0};
	wifi_get_mac_address((char*)mac);
    sscanf(mac,
            "%2x:%2x:%2x:%2x:%2x:%2x",
            macTmp,macTmp+1,macTmp+2,macTmp+3,macTmp+4,macTmp+5);
    
    ZC_HexToString(mac, macTmp, ZC_SERVER_MAC_LEN / 2);
    memcpy(pu8Mac, mac, ZC_SERVER_MAC_LEN);
}

/*************************************************
* Function: HF_Reboot
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Reboot(void)
{
	printf("Restart Ac !!!\r\n");
	HAL_WRITE32(SYSTEM_CTRL_BASE,REG_SOC_FUNC_EN, 
		(HAL_READ32(SYSTEM_CTRL_BASE,REG_SOC_FUNC_EN)&(~BIT27)));
	sys_reset();
}
/*************************************************
* Function: HF_ConnectToCloud
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_ConnectToCloud(PTC_Connection *pstruConnection)
{
    int fd; 
    struct sockaddr_in addr;
    struct ip_addr struIp;
    int retval;
    u16 port;
    int keepalive_enable = 1;
    memset((char*)&addr, 0, sizeof(addr));
    if (1 == g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig)
    {
        port = g_struZcConfigDb.struSwitchInfo.u16ServerPort;
        struIp.addr = htonl(g_struZcConfigDb.struSwitchInfo.u32ServerIp);
        retval = ZC_RET_OK;
    }
    else
    {
        port = ZC_CLOUD_PORT;
        retval = netconn_gethostbyname((const char *)g_struZcConfigDb.struCloudInfo.u8CloudAddr, &struIp);
    }

    if (ZC_RET_OK != retval)
    {
        return ZC_RET_ERROR;
    }

    ZC_Printf("connect ip = 0x%x!\n",struIp.addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = struIp.addr;
    fd = socket(AF_INET, SOCK_STREAM, 0);

    if(fd < 0)
    {
        return ZC_RET_ERROR;
    }
    if (ZC_CLOUD_PORT != port)
    {
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
               (const char *) &keepalive_enable, sizeof( keepalive_enable ));   
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))< 0)
    {
        close(fd);
        if(g_struProtocolController.struCloudConnection.u32ConnectionTimes++>20)
        {
           g_struZcConfigDb.struSwitchInfo.u32ServerAddrConfig = 0;
        }

        return ZC_RET_ERROR;
    }
    g_struProtocolController.struCloudConnection.u32ConnectionTimes = 0;

    ZC_Printf("connect ok!\n");
    g_struProtocolController.struCloudConnection.u32Socket = fd;

    ZC_Rand(g_struProtocolController.RandMsg);
    return ZC_RET_OK;
}
/*************************************************
* Function: HF_ListenClient
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
u32 HF_ListenClient(PTC_Connection *pstruConnection)
{
    int fd; 
    struct sockaddr_in servaddr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd<0)
        return ZC_RET_ERROR;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    servaddr.sin_port = htons(pstruConnection->u16Port);
    if(bind(fd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0)
    {
        close(fd);
        return ZC_RET_ERROR;
    }
    
    if (listen(fd, TCP_DEFAULT_LISTEN_BACKLOG)< 0)
    {
        close(fd);
        return ZC_RET_ERROR;
    }

    ZC_Printf("Tcp Listen Port = %d\n", pstruConnection->u16Port);
    g_struProtocolController.struClientConnection.u32Socket = fd;

    return ZC_RET_OK;
}

/*************************************************
* Function: HF_Printf
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Printf(const char *pu8format, ...)
{
    char buffer[100 + 1]={0};
    va_list arg;
    va_start (arg, pu8format);
    vsnprintf(buffer, 100, pu8format, arg);
    va_end (arg);
    printf(buffer);
}
/*************************************************
* Function: HF_BcInit
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_BcInit()
{
    int tmp=1;
    struct sockaddr_in addr; 

    addr.sin_family = AF_INET; 
    addr.sin_port = htons(ZC_MOUDLE_PORT); 
    addr.sin_addr.s_addr=htonl(INADDR_ANY);

    g_Bcfd = socket(AF_INET, SOCK_DGRAM, 0); 

    tmp=1; 
    setsockopt(g_Bcfd, SOL_SOCKET,SO_BROADCAST,&tmp,sizeof(tmp)); 

    //hfnet_set_udp_broadcast_port_valid(ZC_MOUDLE_PORT, ZC_MOUDLE_PORT + 1);

    bind(g_Bcfd, (struct sockaddr*)&addr, sizeof(addr)); 
    g_struProtocolController.u16SendBcNum = 0;

    memset((char*)&struRemoteAddr,0,sizeof(struRemoteAddr));
    struRemoteAddr.sin_family = AF_INET; 
    struRemoteAddr.sin_port = htons(ZC_MOUDLE_BROADCAST_PORT); 
    struRemoteAddr.sin_addr.s_addr=inet_addr("255.255.255.255"); 
    g_pu8RemoteAddr = (u8*)&struRemoteAddr;
    g_u32BcSleepCount = 2.5 * 250000;

    return;
}

/*************************************************
* Function: HF_Cloudfunc
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
static void HF_Cloudfunc(void* arg) 
{
    int fd;
    u32 u32Timer = 0;

    HF_BcInit();

    while(1) 
    {
        fd = g_struProtocolController.struCloudConnection.u32Socket;
        PCT_Run();
        
        if (PCT_STATE_DISCONNECT_CLOUD == g_struProtocolController.u8MainState)
        {
            close(fd);
            if (0 == g_struProtocolController.struCloudConnection.u32ConnectionTimes)
            {
                u32Timer = 1000;
            }
            else
            {
                u32Timer = rand();
                u32Timer = (PCT_TIMER_INTERVAL_RECONNECT) * (u32Timer % 10 + 1);
            }
            PCT_ReconnectCloud(&g_struProtocolController, u32Timer);
            g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
            g_struUartBuffer.u32RecvLen = 0;
        }
        else
        {
            MSG_SendDataToCloud((u8*)&g_struProtocolController.struCloudConnection);
        }
        ZC_SendBc();
    } 
}

/*************************************************
* Function: HF_Init
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Init(void)
{
    if (sys_mutex_new(&g_struTimermutex) != 0)
    {
        printf("Create g_struTimermutex failed\n");
    }
    //网络通信接口
    g_struHfAdapter.pfunConnectToCloud = HF_ConnectToCloud;

    g_struHfAdapter.pfunListenClient = HF_ListenClient;
    g_struHfAdapter.pfunSendTcpData = HF_SendTcpData; 
    g_struHfAdapter.pfunSendUdpData = HF_SendUdpData; 
    g_struHfAdapter.pfunUpdate = HF_FirmwareUpdate;  
    //设备内部通信接口
    g_struHfAdapter.pfunSendToMoudle = HF_SendDataToMoudle; 
    //定时器类接口
    g_struHfAdapter.pfunStopTimer = HF_StopTimer;  
    g_struHfAdapter.pfunSetTimer = HF_SetTimer;  

    //存储类接口
    g_struHfAdapter.pfunUpdateFinish = HF_FirmwareUpdateFinish;
    g_struHfAdapter.pfunWriteFlash = HF_WriteDataToFlash;
    g_struHfAdapter.pfunReadFlash = HF_ReadDataFromFlash;
    //系统类接口    
    g_struHfAdapter.pfunRest = HF_Rest;    
    g_struHfAdapter.pfunGetMac = HF_GetMac;
    g_struHfAdapter.pfunReboot = HF_Reboot;
    g_struHfAdapter.pfunMalloc = malloc;
    g_struHfAdapter.pfunFree = free;
    g_struHfAdapter.pfunPrintf = HF_Printf;
    g_u16TcpMss = 1000;

    PCT_Init(&g_struHfAdapter);
    // Initial a periodical timer
    gtimer_init(&g_struTimer1, TIMER0);
    gtimer_start_periodical(&g_struTimer1, 1000000, (void*)Timer_callback, 0);
    
    printf("RTK Init\n");

    g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
    g_struUartBuffer.u32RecvLen = 0;

	if(xTaskCreate(HF_Cloudfunc, ((const char*)"HF_Cloudfunc"), 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    {   
		printf("\n\r%s xTaskCreate(init_thread) failed", __FUNCTION__);
    }
	if(xTaskCreate(HF_CloudRecvfunc, ((const char*)"HF_CloudRecvfunc"), 512, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
    {   
		printf("\n\r%s xTaskCreate(init_thread) failed", __FUNCTION__);
    }     

}

/*************************************************
* Function: HF_WakeUp
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_WakeUp()
{
    ZC_Printf("HF_WakeUp\n\r");
    PCT_WakeUp();
}
/*************************************************
* Function: HF_Sleep
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void HF_Sleep()
{
    u32 u32Index;
    ZC_Printf("HF_Sleep\r\n");
    close(g_Bcfd);

    if (PCT_INVAILD_SOCKET != g_struProtocolController.struClientConnection.u32Socket)
    {
        close(g_struProtocolController.struClientConnection.u32Socket);
        g_struProtocolController.struClientConnection.u32Socket = PCT_INVAILD_SOCKET;
    }

    if (PCT_INVAILD_SOCKET != g_struProtocolController.struCloudConnection.u32Socket)
    {
        close(g_struProtocolController.struCloudConnection.u32Socket);
        g_struProtocolController.struCloudConnection.u32Socket = PCT_INVAILD_SOCKET;
    }
    
    for (u32Index = 0; u32Index < ZC_MAX_CLIENT_NUM; u32Index++)
    {
        if (0 == g_struClientInfo.u32ClientVaildFlag[u32Index])
        {
            close(g_struClientInfo.u32ClientFd[u32Index]);
            g_struClientInfo.u32ClientFd[u32Index] = PCT_INVAILD_SOCKET;
        }
    }

    PCT_Sleep();
    
    g_struUartBuffer.u32Status = MSG_BUFFER_IDLE;
    g_struUartBuffer.u32RecvLen = 0;
}

/*************************************************
* Function: AC_UartSend
* Description: 
* Author: cxy 
* Returns: 
* Parameter: 
* History:
*************************************************/
void AC_UartSend(u8* inBuf, u32 datalen)
{
     //hfuart_send(HFUART0,(char*)inBuf,datalen,1000); 
}
/******************************* FILE END ***********************************/


