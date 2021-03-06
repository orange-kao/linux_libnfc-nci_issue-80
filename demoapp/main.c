/******************************************************************************
 *
 *  Copyright (C) 2015 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License")
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "linux_nfc_api.h"
#include "tools.h"

static void* global_thread_handle = NULL;

static void* global_lock = NULL;
static int global_present = 0;
static int global_exit    = 0;
static nfcSnepServerCallback_t g_SnepServerCB;
static nfcSnepClientCallback_t g_SnepClientCB;

void help(int mode);
int init_env();
int look_for_tag(char** args, int args_len, char* tag, char** data, int format);

/********************************** CallBack **********************************/

void on_server_device_arrival (void)
{
    printf("\tSNEP server - device arrival - Line %d %s()\n", __LINE__, __func__);
    return;
}

void on_server_device_departure (void)
{
    printf("\tSNEP server - device departure - Line %d %s()\n", __LINE__, __func__);
    return;
}

void on_server_message_received(unsigned char *message, unsigned int length)
{
    printf("\tSNEP server - message received - Line %d %s()\n", __LINE__, __func__);
    unsigned int i = 0x00;
    printf("\t\tNDEF Message Received : \n");
    print_ndef_content(NULL, NULL, message, length);
}

void on_client_device_arrival()
{
    printf("\tSNEP client - device arrival - Line %d %s()\n", __LINE__, __func__);
    framework_LockMutex(global_lock);
    global_present = 1;
    framework_UnlockMutex(global_lock);
    return;
}

void on_client_device_departure()
{
    printf("\tSNEP client - device departure - Line %d %s()\n", __LINE__, __func__);
    framework_LockMutex(global_lock);
    global_present = 0;
    framework_UnlockMutex(global_lock);
    return;
}

int init_mode(int tag, int p2p, int hce, int snep_server, int snep_client)
{
    int res = 0x00;

    if(snep_server == 1){
        g_SnepServerCB.onDeviceArrival = on_server_device_arrival;
        g_SnepServerCB.onDeviceDeparture = on_server_device_departure;
        g_SnepServerCB.onMessageReceived = on_server_message_received;
    }

    if(snep_client == 1){
        g_SnepClientCB.onDeviceArrival = on_client_device_arrival;
        g_SnepClientCB.onDeviceDeparture = on_client_device_departure;
    }

    if(0x00 == res)
    {
        res = nfcManager_doInitialize();
        if(0x00 != res)
        {
            printf("NfcService Init Failed\n");
        }
    }

    if(0x00 == res)
    {
        if(0x01 == p2p && snep_client == 1)
        {
            res = nfcSnep_registerClientCallback(&g_SnepClientCB);
            if(0x00 != res)
            {
                printf("SNEP Client Register Callback Failed\n");
            }
        }
    }

    if(0x00 == res)
    {
        nfcManager_enableDiscovery(DEFAULT_NFA_TECH_MASK, 0x00, hce, 1);
        if(0x01 == p2p && snep_server == 1)
        {
            res = nfcSnep_startServer(&g_SnepServerCB);
            if(0x00 != res)
            {
                printf("Start SNEP Server Failed\n");
            }
        }
    }

    return res;
}

int deinit_poll_mode()
{
    int res = 0x00;

    nfcSnep_stopServer();

    nfcManager_disableDiscovery();

    nfcSnep_deregisterClientCallback();

    nfcManager_deregisterTagCallback();

    nfcHce_deregisterHceCallback();

    res = nfcManager_doDeinitialize();

    if(0x00 != res)
    {
        printf("NFC Service Deinit Failed\n");
    }
    return res;
}

int snep_push(unsigned char* msgToPush, unsigned int len)
{
    printf("\tsnep_push - Line %d %s()\n", __LINE__, __func__);
    int res = 0x00;

    printf("\tPutting SNEP message - Line %d %s()\n", __LINE__, __func__);
    res = nfcSnep_putMessage(msgToPush, len);

    if(0x00 != res)
    {
        printf("\t\tPush Failed\n");
    }
    else
    {
        printf("\t\tPush successful\n");
    }

    return res;
}

void print_ndef_content(nfc_tag_info_t* TagInfo, ndef_info_t* NDEFinfo, unsigned char* ndefRaw, unsigned int ndefRawLen)
{
    unsigned char* NDEFContent = NULL;
    nfc_friendly_type_t lNDEFType = NDEF_FRIENDLY_TYPE_OTHER;
    unsigned int res = 0x00;
    unsigned int i = 0x00;
    char* TextContent = NULL;
    char* URLContent = NULL;
    nfc_handover_select_t HandoverSelectContent;
    nfc_handover_request_t HandoverRequestContent;
    if(NULL != NDEFinfo)
    {
        ndefRawLen = NDEFinfo->current_ndef_length;
        NDEFContent = malloc(ndefRawLen * sizeof(unsigned char));
        res = nfcTag_readNdef(TagInfo->handle, NDEFContent, ndefRawLen, &lNDEFType);
    }
    else if (NULL != ndefRaw && 0x00 != ndefRawLen)
    {
        NDEFContent = malloc(ndefRawLen * sizeof(unsigned char));
        memcpy(NDEFContent, ndefRaw, ndefRawLen);
        res = ndefRawLen;
        if((NDEFContent[0] & 0x7) == NDEF_TNF_WELLKNOWN && 0x55 == NDEFContent[3])
        {
            lNDEFType = NDEF_FRIENDLY_TYPE_URL;
        }
        if((NDEFContent[0] & 0x7) == NDEF_TNF_WELLKNOWN && 0x54 == NDEFContent[3])
        {
            lNDEFType = NDEF_FRIENDLY_TYPE_TEXT;
        }
    }
    else
    {
        printf("\t\t\t\tError : Invalid Parameters\n");
    }

    if(res != ndefRawLen)
    {
        printf("\t\t\t\tRead NDEF Content Failed\n");
    }
    else
    {
        switch(lNDEFType)
        {
            case NDEF_FRIENDLY_TYPE_TEXT:
                {
                    TextContent = malloc(res * sizeof(char));
                    res = ndef_readText(NDEFContent, res, TextContent, res);
                    if(0x00 <= res)
                    {
                        printf("\t\t\t\tType :                 'Text'\n");
                        printf("\t\t\t\tText :                 '%s'\n", TextContent);
                    }
                    else
                    {
                        printf("\t\t\t\tRead NDEF Text Error\n");
                    }
                    if(NULL != TextContent)
                    {
                        free(TextContent);
                        TextContent = NULL;
                    }
                } break;
            case NDEF_FRIENDLY_TYPE_URL:
                {
                    /*NOTE : + 27 = Max prefix lenght*/
                    URLContent = malloc(res * sizeof(unsigned char) + 27 );
                    memset(URLContent, 0x00, res * sizeof(unsigned char) + 27);
                    res = ndef_readUrl(NDEFContent, res, URLContent, res + 27);
                    if(0x00 <= res)
                    {
                        printf("                Type :                 'URI'\n");
                        printf("                URI :                 '%s'\n", URLContent);
                    }
                    else
                    {
                        printf("                Read NDEF URL Error\n");
                    }
                    if(NULL != URLContent)
                    {
                        free(URLContent);
                        URLContent = NULL;
                    }
                } break;
            case NDEF_FRIENDLY_TYPE_HS:
                {
                    res = ndef_readHandoverSelectInfo(NDEFContent, res, &HandoverSelectContent);
                    if(0x00 <= res)
                    {
                        printf("\n\t\tHandover Select : \n");

                        printf("\t\tBluetooth : \n\t\t\t\tPower state : ");
                        switch(HandoverSelectContent.bluetooth.power_state)
                        {
                            case HANDOVER_CPS_INACTIVE:
                                {
                                    printf(" 'Inactive'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVE:
                                {
                                    printf(" 'Active'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVATING:
                                {
                                    printf(" 'Activating'\n");
                                } break;
                            case HANDOVER_CPS_UNKNOWN:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                            default:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                        }
                        if(HANDOVER_TYPE_BT == HandoverSelectContent.bluetooth.type)
                        {
                            printf("\t\t\t\tType :         'BT'\n");
                        }
                        else if(HANDOVER_TYPE_BLE == HandoverSelectContent.bluetooth.type)
                        {
                            printf("\t\t\t\tType :         'BLE'\n");
                        }
                        else
                        {
                            printf("\t\t\t\tType :            'Unknown'\n");
                        }
                        printf("\t\t\t\tAddress :      '");
                        for(i = 0x00; i < 6; i++)
                        {
                            printf("%02X ", HandoverSelectContent.bluetooth.address[i]);
                        }
                        printf("'\n\t\t\t\tDevice Name :  '");
                        for(i = 0x00; i < HandoverSelectContent.bluetooth.device_name_length; i++)
                        {
                            printf("%c ", HandoverSelectContent.bluetooth.device_name[i]);
                        }
                        printf("'\n\t\t\t\tNDEF Record :     \n\t\t\t\t");
                        for(i = 0x01; i < HandoverSelectContent.bluetooth.ndef_length+1; i++)
                        {
                            printf("%02X ", HandoverSelectContent.bluetooth.ndef[i]);
                            if(i%8 == 0)
                            {
                                printf("\n\t\t\t\t");
                            }
                        }
                        printf("\n\t\tWIFI : \n\t\t\t\tPower state : ");
                        switch(HandoverSelectContent.wifi.power_state)
                        {
                            case HANDOVER_CPS_INACTIVE:
                                {
                                    printf(" 'Inactive'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVE:
                                {
                                    printf(" 'Active'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVATING:
                                {
                                    printf(" 'Activating'\n");
                                } break;
                            case HANDOVER_CPS_UNKNOWN:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                            default:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                        }

                        printf("\t\t\t\tSSID :         '");
                        for(i = 0x01; i < HandoverSelectContent.wifi.ssid_length+1; i++)
                        {
                            printf("%02X ", HandoverSelectContent.wifi.ssid[i]);
                            if(i%30 == 0)
                            {
                                printf("\n");
                            }
                        }
                        printf("'\n\t\t\t\tKey :          '");
                        for(i = 0x01; i < HandoverSelectContent.wifi.key_length+1; i++)
                        {
                            printf("%02X ", HandoverSelectContent.wifi.key[i]);
                            if(i%30 == 0)
                            {
                                printf("\n");
                            }
                        }
                        printf("'\n\t\t\t\tNDEF Record : \n");
                        for(i = 0x01; i < HandoverSelectContent.wifi.ndef_length+1; i++)
                        {
                            printf("%02X ", HandoverSelectContent.wifi.ndef[i]);
                            if(i%30 == 0)
                            {
                                printf("\n");
                            }
                        }
                        printf("\n");
                    }
                    else
                    {
                        printf("\n\t\tRead NDEF Handover Select Failed\n");
                    }

                } break;
            case NDEF_FRIENDLY_TYPE_HR:
                {
                    res = ndef_readHandoverRequestInfo(NDEFContent, res, &HandoverRequestContent);
                    if(0x00 <= res)
                    {
                        printf("\n\t\tHandover Request : \n");
                        printf("\t\tBluetooth : \n\t\t\t\tPower state : ");
                        switch(HandoverRequestContent.bluetooth.power_state)
                        {
                            case HANDOVER_CPS_INACTIVE:
                                {
                                    printf(" 'Inactive'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVE:
                                {
                                    printf(" 'Active'\n");
                                } break;
                            case HANDOVER_CPS_ACTIVATING:
                                {
                                    printf(" 'Activating'\n");
                                } break;
                            case HANDOVER_CPS_UNKNOWN:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                            default:
                                {
                                    printf(" 'Unknown'\n");
                                } break;
                        }
                        if(HANDOVER_TYPE_BT == HandoverRequestContent.bluetooth.type)
                        {
                            printf("\t\t\t\tType :         'BT'\n");
                        }
                        else if(HANDOVER_TYPE_BLE == HandoverRequestContent.bluetooth.type)
                        {
                            printf("\t\t\t\tType :         'BLE'\n");
                        }
                        else
                        {
                            printf("\t\t\t\tType :            'Unknown'\n");
                        }
                        printf("\t\t\t\tAddress :      '");
                        for(i = 0x00; i < 6; i++)
                        {
                            printf("%02X ", HandoverRequestContent.bluetooth.address[i]);
                        }
                        printf("'\n\t\t\t\tDevice Name :  '");
                        for(i = 0x00; i < HandoverRequestContent.bluetooth.device_name_length; i++)
                        {
                            printf("%c ", HandoverRequestContent.bluetooth.device_name[i]);
                        }
                        printf("'\n\t\t\t\tNDEF Record :     \n\t\t\t\t");
                        for(i = 0x01; i < HandoverRequestContent.bluetooth.ndef_length+1; i++)
                        {
                            printf("%02X ", HandoverRequestContent.bluetooth.ndef[i]);
                            if(i%8 == 0)
                            {
                                printf("\n\t\t\t\t");
                            }
                        }
                        printf("\n\t\t\t\tWIFI :         'Has WIFI Request : %X '", HandoverRequestContent.wifi.has_wifi);
                        printf("\n\t\t\t\tNDEF Record :     \n\t\t\t\t");
                        for(i = 0x01; i < HandoverRequestContent.wifi.ndef_length+1; i++)
                        {
                            printf("%02X ", HandoverRequestContent.wifi.ndef[i]);
                            if(i%8 == 0)
                            {
                                printf("\n\t\t\t\t");
                            }
                        }
                        printf("\n");
                    }
                    else
                    {
                        printf("\n\t\tRead NDEF Handover Request Failed\n");
                    }
                } break;
            case NDEF_FRIENDLY_TYPE_OTHER:
                {
                    switch(NDEFContent[0] & 0x7)
                    {
                        case NDEF_TNF_EMPTY:
                            {
                                printf("\n\t\tTNF Empty\n");
                            } break;
                        case NDEF_TNF_WELLKNOWN:
                            {
                                printf("\n\t\tTNF Well Known\n");
                            } break;
                        case NDEF_TNF_MEDIA:
                            {
                                printf("\n\t\tTNF Media\n");
                                printf("\t\t\tType : ");
                                for(i = 0x00; i < NDEFContent[1]; i++)
                                {
                                    printf("%c", NDEFContent[3 + i]);
                                }
                                printf("\n\t\t\tData : ");
                                for(i = 0x00; i < NDEFContent[2]; i++)
                                {
                                    printf("%c", NDEFContent[3 + NDEFContent[1] + i]);
                                    if('\n' == NDEFContent[3 + NDEFContent[1] + i])
                                    {
                                        printf("\t\t\t");
                                    }
                                }
                                printf("\n");

                            } break;
                        case NDEF_TNF_URI:
                            {
                                printf("\n\t\tTNF URI\n");
                            } break;
                        case NDEF_TNF_EXT:
                            {
                                printf("\n\t\tTNF External\n");
                                printf("\t\t\tType : ");
                                for(i = 0x00; i < NDEFContent[1]; i++)
                                {
                                    printf("%c", NDEFContent[3 + i]);
                                }
                                printf("\n\t\t\tData : ");
                                for(i = 0x00; i < NDEFContent[2]; i++)
                                {
                                    printf("%c", NDEFContent[3 + NDEFContent[1] + i]);
                                    if('\n' == NDEFContent[3 + NDEFContent[1] + i])
                                    {
                                        printf("\t\t\t");
                                    }
                                }
                                printf("\n");
                            } break;
                        case NDEF_TNF_UNKNOWN:
                            {
                                printf("\n\t\tTNF Unknown\n");
                            } break;
                        case NDEF_TNF_UNCHANGED:
                            {
                                printf("\n\t\tTNF Unchanged\n");
                            } break;
                        default:
                            {
                                printf("\n\t\tTNF Other\n");
                            } break;
                    }
                } break;
            default:
                {
                } break;
        }
        printf("\t\t%d bytes of NDEF data received :\n\t\t", ndefRawLen);
        for(i = 0x00; i < ndefRawLen; i++)
        {
            printf("%02X ", NDEFContent[i]);
            if(i%30 == 0 && 0x00 != i)
            {
                printf("\n\t\t");
            }
        }
        printf("\n");
    }

    if(NULL != NDEFContent)
    {
        free(NDEFContent);
        NDEFContent = NULL;
    }
}

/* mode=1 => poll, mode=2 => push, mode=3 => write, mode=4 => HCE */
int wait_device_arrival(int mode, unsigned char* msgToSend, unsigned int len)
{

    do
    {
        printf("do loop first line\n");

        while(1){
            int present_dup = 0;
            int exit_dup = 0;

            framework_LockMutex(global_lock);
            present_dup = global_present;
            exit_dup = global_exit;
            framework_UnlockMutex(global_lock);

            if (exit_dup == 1){
                return 0;
            }
            if (present_dup == 1){
                break;
            }
            usleep(10000);
        }

        printf("\tPushing...\n");
        for(int push_i = 0; push_i < 5; push_i++){

            printf("\tSleeping before push (%d)\n", push_i);
            usleep(50000);

            if(snep_push(msgToSend, len) != 0){
                break;
            }
        }
    }while(1);

    printf("\tdo loop break\n");

    return 0;
}

void str_to_lower(char * string)
{
    unsigned int i = 0x00;

    for(i = 0; i < strlen(string); i++)
    {
        string[i] = tolower(string[i]);
    }
}

char* str_removce_char(const char* str, char car)
{
    unsigned int i = 0x00;
    unsigned int index = 0x00;
    char * dest = (char*)malloc((strlen(str) + 1) * sizeof(char));

    for(i = 0x00; i < strlen(str); i++)
    {
        if(str[i] != car)
        {
            dest[index++] = str[i];
        }
    }
    dest[index] = '\0';
    return dest;
}

int convert_param_to_buffer(char* param, unsigned char** outBuffer, unsigned int* outBufferLen)
{
    int res = 0x00;
    unsigned int i = 0x00;
    int index = 0x00;
    char atoiBuf[3];
    atoiBuf[2] = '\0';

    if(NULL == param || NULL == outBuffer || NULL == outBufferLen)
    {
        printf("Parameter Error\n");
        res = 0xFF;
    }

    if(0x00 == res)
    {
        param = str_removce_char(param, ' ');
    }

    if(0x00 == res)
    {
        if(0x00 == strlen(param) % 2)
        {
            *outBufferLen = strlen(param) / 2;

            *outBuffer = (unsigned char*) malloc((*outBufferLen) * sizeof(unsigned char));
            if(NULL != *outBuffer)
            {
                for(i = 0x00; i < ((*outBufferLen) * 2); i = i + 2)
                {
                    atoiBuf[0] = param[i];
                    atoiBuf[1] = param[i + 1];
                    (*outBuffer)[index++] = strtol(atoiBuf, NULL, 16);
                }
            }
            else
            {
                printf("Memory Allocation Failed\n");
                res = 0xFF;
            }
        }
        else
        {
            printf("Invalid NDEF Message Param\n");
        }
        free(param);
    }

    return res;
}

int build_ndef_message(int arg_len, char** arg, unsigned char** outNDEFBuffer, unsigned int* outNDEFBufferLen)
{
    int res = 0x00;
    nfc_handover_cps_t cps = HANDOVER_CPS_UNKNOWN;
    unsigned char* ndef_msg = NULL;
    unsigned int ndef_msg_len = 0x00;
    char *type = NULL;
    char *uri = NULL;
    char *text = NULL;
    char* lang = NULL;
    char* mime_type = NULL;
    char* mime_data = NULL;
    char* carrier_state = NULL;
    char* carrier_name = NULL;
    char* carrier_data = NULL;

    if(0xFF == look_for_tag(arg, arg_len, "-t", &type, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--type", &type, 0x01))
    {
        res = 0xFF;
        printf("Record type missing (-t)\n");
        help(0x02);
    }
    if(0x00 == res)
    {
        str_to_lower(type);
        if(0x00 == strcmp(type, "uri"))
        {
            if(0x00 == look_for_tag(arg, arg_len, "-u", &uri, 0x00) || 0x00 == look_for_tag(arg, arg_len, "--uri", &uri, 0x01))
            {
                *outNDEFBufferLen = strlen(uri) + 30; /*TODO : replace 30 by URI NDEF message header*/
                *outNDEFBuffer = (unsigned char*) malloc(*outNDEFBufferLen * sizeof(unsigned char));
                res = ndef_createUri(uri, *outNDEFBuffer, *outNDEFBufferLen);
                if(0x00 >= res)
                {
                    printf("Failed to build URI NDEF Message\n");
                }
                else
                {
                    *outNDEFBufferLen = res;
                    res = 0x00;
                }
            }
            else
            {
                printf("URI Missing (-u)\n");
                help(0x02);
                res = 0xFF;
            }
        }
        else if(0x00 == strcmp(type, "text"))
        {
            if(0xFF == look_for_tag(arg, arg_len, "-r", &text, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--rep", &text, 0x01))
            {
                printf("Representation missing (-r)\n");
                res = 0xFF;
            }

            if(0xFF == look_for_tag(arg, arg_len, "-l", &lang, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--lang", &lang, 0x01))
            {
                printf("Language missing (-l)\n");
                res = 0xFF;
            }
            if(0x00 == res)
            {
                *outNDEFBufferLen = strlen(text) + strlen(lang) + 30; /*TODO : replace 30 by TEXT NDEF message header*/
                *outNDEFBuffer = (unsigned char*) malloc(*outNDEFBufferLen * sizeof(unsigned char));
                res = ndef_createText(lang, text, *outNDEFBuffer, *outNDEFBufferLen);
                if(0x00 >= res)
                {
                    printf("Failed to build TEXT NDEF Message\n");
                }
                else
                {
                    *outNDEFBufferLen = res;
                    res = 0x00;
                }
            }
            else
            {
                help(0x02);
            }
        }
        else if(0x00 == strcmp(type, "mime"))
        {
            if(0xFF == look_for_tag(arg, arg_len, "-m", &mime_type, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--mime", &mime_type, 0x01))
            {
                printf("Mime-type missing (-m)\n");
                res = 0xFF;
            }
            if(0xFF == look_for_tag(arg, arg_len, "-d", &mime_data, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--data", &mime_data, 0x01))
            {
                printf("NDEF Data missing (-d)\n");
                res = 0xFF;
            }
            if(0x00 == res)
            {
                *outNDEFBufferLen = strlen(mime_data) +  strlen(mime_type) + 30; /*TODO : replace 30 by MIME NDEF message header*/
                *outNDEFBuffer = (unsigned char*) malloc(*outNDEFBufferLen * sizeof(unsigned char));

                res = convert_param_to_buffer(mime_data, &ndef_msg, &ndef_msg_len);

                if(0x00 == res)
                {
                    res = ndef_createMime(mime_type, ndef_msg, ndef_msg_len, *outNDEFBuffer, *outNDEFBufferLen);
                    if(0x00 >= res)
                    {
                        printf("Failed to build MIME NDEF Message\n");
                    }
                    else
                    {
                        *outNDEFBufferLen = res;
                        res = 0x00;
                    }
                }
            }
            else
            {
                help(0x02);
            }
        }
        else if(0x00 == strcmp(type, "hs"))
        {

            if(0xFF == look_for_tag(arg, arg_len, "-cs", &carrier_state, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--carrierState", &carrier_state, 0x01))
            {
                printf("Carrier Power State missing (-cs)\n");
                res = 0xFF;
            }
            if(0xFF == look_for_tag(arg, arg_len, "-cn", &carrier_name, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--carrierName", &carrier_name, 0x01))
            {
                printf("Carrier Reference Name missing (-cn)\n");
                res = 0xFF;
            }

            if(0xFF == look_for_tag(arg, arg_len, "-d", &carrier_data, 0x00) && 0xFF == look_for_tag(arg, arg_len, "--data", &carrier_data, 0x01))
            {
                printf("NDEF Data missing (-d)\n");
                res = 0xFF;
            }

            if(0x00 == res)
            {
                *outNDEFBufferLen = strlen(carrier_name) + strlen(carrier_data) + 30;  /*TODO : replace 30 by HS NDEF message header*/
                *outNDEFBuffer = (unsigned char*) malloc(*outNDEFBufferLen * sizeof(unsigned char));

                str_to_lower(carrier_state);

                if(0x00 == strcmp(carrier_state, "inactive"))
                {
                    cps = HANDOVER_CPS_INACTIVE;
                }
                else if(0x00 == strcmp(carrier_state, "active"))
                {
                    cps = HANDOVER_CPS_ACTIVE;
                }
                else if(0x00 == strcmp(carrier_state, "activating"))
                {
                    cps = HANDOVER_CPS_ACTIVATING;
                }
                else
                {
                    printf("Error : unknown carrier power state %s\n", carrier_state);
                    res = 0xFF;
                }

                if(0x00 == res)
                {
                    res = convert_param_to_buffer(carrier_data, &ndef_msg, &ndef_msg_len);
                }

                if(0x00 == res)
                {
                    res = ndef_createHandoverSelect(cps, carrier_name, ndef_msg, ndef_msg_len, *outNDEFBuffer, *outNDEFBufferLen);
                    if(0x00 >= res)
                    {
                        printf("Failed to build handover select message\n");
                    }
                    else
                    {
                        *outNDEFBufferLen = res;
                        res = 0x00;
                    }
                }
            }
            else
            {
                help(0x02);
            }
        }
        else
        {
            printf("NDEF Type %s not supported\n", type);
            res = 0xFF;
        }
    }

    if(NULL != ndef_msg)
    {
        free(ndef_msg);
        ndef_msg = NULL;
        ndef_msg_len = 0x00;
    }

    return res;
}

/* if data = NULL this tag is not followed by dataStr : for example -h --help
   if format = 0 tag format -t "text" if format=1 tag format : --type=text */
int look_for_tag(char** args, int args_len, char* tag, char** data, int format)
{
    int res = 0xFF;
    int i = 0x00;
    int found = 0xFF;

    for(i = 0x00; i < args_len; i++)
    {
        found = 0xFF;
        str_to_lower(args[i]);
        if(0x00 == format)
        {
            found = strcmp(args[i], tag);
        }
        else
        {
            found = strncmp(args[i], tag, strlen(tag));
        }

        if(0x00 == found)
        {
            if(NULL != data)
            {
                if(0x00 == format)
                {
                    if(i < (args_len - 1))
                    {
                        *data = args[i + 1];
                        res = 0x00;
                        break;
                    }
                    else
                    {
                        printf("Argument missing after %s\n", tag);
                    }
                }
                else
                {
                    *data = &args[i][strlen(tag) + 1]; /* +1 to remove '='*/
                    res = 0x00;
                    break;
                }
            }
            else
            {
                res = 0x00;
                break;
            }
        }
    }

    return res;
}

void cmd_poll(int arg_len, char** arg)
{
    int res = 0x00;

    printf("#################################################\n");
    printf("##                   NFC demo                  ##\n");
    printf("#################################################\n");
    printf("##             Poll mode activated             ##\n");
    printf("#################################################\n");

    init_env();
    if(0x00 == look_for_tag(arg, arg_len, "-h", NULL, 0x00) || 0x00 == look_for_tag(arg, arg_len, "--help", NULL, 0x01))
    {
        help(0x01);
    }
    else
    {
        res = init_mode(0x00, 0x01, 0x00, 1, 0);

        if(0x00 == res)
        {
            wait_device_arrival(0x01, NULL , 0x00);
        }

        res = deinit_poll_mode();
    }

    printf("Leaving ...\n");
}

void cmd_push(int arg_len, char** arg)
{
    int res = 0x00;
    unsigned char * NDEFMsg = NULL;
    unsigned int NDEFMsgLen = 0x00;

    printf("#################################################\n");
    printf("##                   NFC demo                  ##\n");
    printf("#################################################\n");
    printf("##             Push mode activated             ##\n");
    printf("#################################################\n");

    init_env();

    if(0x00 == look_for_tag(arg, arg_len, "-h", NULL, 0x00) || 0x00 == look_for_tag(arg, arg_len, "--help", NULL, 0x01))
    {
        help(0x02);
    }
    else
    {
        res = init_mode(0x00, 0x01, 0x00, 0, 1);

        if(0x00 == res)
        {
            res = build_ndef_message(arg_len, arg, &NDEFMsg, &NDEFMsgLen);
        }

        if(0x00 == res)
        {
            wait_device_arrival(0x02, NDEFMsg, NDEFMsgLen);
        }

        if(NULL != NDEFMsg)
        {
            free(NDEFMsg);
            NDEFMsg = NULL;
            NDEFMsgLen = 0x00;
        }

        res = deinit_poll_mode();
    }

    printf("Leaving ...\n");
}

void* exit_thread(void* pContext)
{
    printf("                              ... press enter to quit ...\n");

    getchar();
    framework_LockMutex(global_lock);
    global_exit = 1;
    framework_UnlockMutex(global_lock);

    return NULL;
}

int init_env()
{
    eResult tool_res = FRAMEWORK_SUCCESS;
    int res = 0x00;

    tool_res = framework_CreateMutex(&global_lock);
    if(FRAMEWORK_SUCCESS != tool_res)
    {
        res = 0xFF;
    }

    if(0x00 == res)
    {
        tool_res = framework_CreateThread(&global_thread_handle, exit_thread, NULL);
        if(FRAMEWORK_SUCCESS != tool_res)
        {
            res = 0xFF;
        }
    }
    return res;
}

int clean_env()
{
    if(NULL != global_thread_handle)
    {
        framework_JoinThread(global_thread_handle);
        framework_DeleteThread(global_thread_handle);
        global_thread_handle = NULL;
    }

    if(NULL != global_lock)
    {
        framework_DeleteMutex(global_lock);
        global_lock = NULL;
    }

    return 0x00;
}

int main(int argc, char ** argv)
{
    if (argc<2)
    {
        printf("Missing argument\n");
        help(0x00);
    }
    else if (strcmp(argv[1],"poll") == 0)
    {
        cmd_poll(argc - 2, &argv[2]);
    }
    else if(strcmp(argv[1],"push") == 0)
    {
        cmd_push(argc - 2, &argv[2]);
    }
    else
    {
        help(0x00);
    }
    printf("\n");

    clean_env();

    return 0;
}

void help(int mode)
{
    printf("\nCOMMAND: \n");
    printf("\tpoll\tPolling mode \t e.g. <nfcDemoApp poll >\n");
    printf("\tpush\tPush to device \t e.g. <nfcDemoApp push -t URI -u http://www.nxp.com>\n");
    printf("\t    \t               \t e.g. <nfcDemoApp push --type=mime -m \"application/vnd.bluetooth.ep.oob\" -d \"2200AC597405AF1C0E0947616C617879204E6F74652033040D0C024005031E110B11\">\n");
    printf("\n");
    printf("Help Options:\n");
    printf("-h, --help                       Show help options\n");
    printf("\n");

    if(0x01 == mode)
    {
        printf("No Application Options for poll mode\n");
    }
    if(0x02 == mode)
    {
        printf("Supported NDEF Types : \n");
        printf("\t URI\n");
        printf("\t TEXT\n");
        printf("\t MIME\n");
        printf("\t HS (handover select)\n");
        printf("\n");

        printf("Application Options : \n");
        printf("\t-l,  --lang=en                   Language\n");
        printf("\t-m,  --mime=audio/mp3            Mime-type\n");
        printf("\t-r,  --rep=sample text           Representation\n");
        printf("\t-t,  --type=Text                 Record type (Text, URI...\n");
        printf("\t-u,  --uri=http://www.nxp.com    URI\n");
        printf("\t-d,  --data=00223344565667788    NDEF Data (for type MIME & Handover select)\n");
        printf("\t-cs, --carrierState=active       Carrier State (for type Handover select)\n");
        printf("\t-cn, --carrierName=ble           Carrier Reference Name (for type Handover select)\n");
    }
    printf("\n");
}

