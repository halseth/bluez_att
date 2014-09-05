#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "curl_proxy.h"

#define ATT_CID 4

#define BDADDR_LE_PUBLIC       0x01
#define BDADDR_LE_RANDOM       0x02

struct sockaddr_l2
{
  sa_family_t    l2_family;
  unsigned short l2_psm;
  bdaddr_t       l2_bdaddr;
  unsigned short l2_cid;
  uint8_t        l2_bdaddr_type;
};

#define L2CAP_CONNINFO  0x02
struct l2cap_conninfo
{
  uint16_t       hci_handle;
  uint8_t        dev_class[3];
};

#define CHAR_VAL_BUFFER_SIZE 512

uint8_t m_uri[CHAR_VAL_BUFFER_SIZE];
uint8_t m_header[CHAR_VAL_BUFFER_SIZE];
uint8_t m_body[CHAR_VAL_BUFFER_SIZE];
uint8_t m_header_response[CHAR_VAL_BUFFER_SIZE];
uint8_t m_body_response[CHAR_VAL_BUFFER_SIZE];

struct l2cap_conninfo l2capConnInfo;
/** GATT Server Record View. */
/*-------------------------------------------------------------------------------------------------+
 | Handle | Attribute Description | UUID   | Value                                                 |
 ---------+-----------------------+--------+-------------------------------------------------------+
 | 0x0010 | HPS Service UUID      | 0x2800 | 0xAAB0                                                |
 | 0x0011 | URI Characteristic    | 0x2803 | 0x8A 0x0012 0xAABA                                    |
 | 0x0012 | URI Value             | 0xAABA | Variable.                                             |
 | 0x0013 | Extended properties   | 0x2902 | 0x0001 (Reliable Write)                               |
 | 0x0014 | Headers Characterist  | 0x2803 | 0x8A 0x0015 0xAABB                                    |
 | 0x0015 | Header Value          | 0xAABB | Variable.                                             |
 | 0x0016 | Extended properties   | 0x2902 | 0x0001 (Reliable Write)                               | 
 | 0x0017 | Entity Body Characte  | 0x2803 | 0x8A 0x0018 0xAABC                                    |
 | 0x0018 | Entity Body Value     | 0xAABC | Variable.                                             |
 | 0x0019 | Extended properties   | 0x2902 | 0x0001 (Reliable Write)                               |
 | 0x001A | Status Characteristi  | 0x2803 | 0x10 0x001B 0xAABD                                    |
 | 0x001B | Status Value          | 0xAABD | uint16 HTTP Status + uint8 Data status                |
 | 0x001C | Status CCCD           | 0x2902 | 0x0001 or 0x0001                                      |
 | 0x001D | Control Point Charac  | 0x2803 | 0x08 0x001E 0xAABE                                    |
 | 0x001E | Control Point Value   | 0xAABE | uint8 values from 0x01 to 0x0F                        |
 | 0x001F | HTTPS Security Chara  | 0x2803 | 0x08 0x001E 0xAABE                                    |
 | 0x0020 | HTTPS Security Value  | 0xAABF | boolean                                               |
--------------------------------------------------------------------------------------------------*/


#define URI_CHAR_VAL_HANDLE               0x0012
#define HEADER_CHAR_VAL_HANDLE            0x0015
#define ENTITY_BODY_CHAR_VAL_HANDLE       0x0018
#define STATUS_CHAR_VAL_HANDLE            0x001B
#define CNTRL_PNT_CHAR_VAL_HANDLE         0x001E
#define SECURITY_CHAR_VAL_HANDLE          0x0020


#define ATT_ERROR_RESPONSE                0x01
#define ATT_EXHCNAGE_MTU_REQUEST          0x02
#define ATT_EXHCNAGE_MTU_RESPONSE         0x03
#define ATT_FIND_INFO_REQUEST             0x04
#define ATT_FIND_INFO_RESPONSE            0x05
#define ATT_FIND_BY_TYPE_VALUE_REQUEST    0x06
#define ATT_FIND_BY_TYPE_VALUE_RESPONSE   0x07
#define ATT_READ_BY_TYPE_REQUEST          0x08
#define ATT_READ_BY_TYPE_RESPONSE         0x09
#define ATT_READ_REQUEST                  0x0A
#define ATT_READ_RESPONSE                 0x0B
#define ATT_READ_BLOB_REQUEST             0x0C
#define ATT_READ_BLOB_RESPONSE            0x0D
#define ATT_READ_MULTIPLE_REQUEST         0x0E
#define ATT_READ_MULTIPLE_RESPONSE        0x0F
#define ATT_READ_BY_GROUP_REQUEST         0x10
#define ATT_READ_BY_GROUP_RESPONSE        0x11
#define ATT_WRITE_REQUEST                 0x12
#define ATT_WRITE_RESPONSE                0x13
#define ATT_WRITE_COMMAND                 0x52
#define ATT_PREPARE_WRITE_REQUEST         0x16
#define ATT_PREPARE_WRITE_RESPONSE        0x17
#define ATT_EXECUTE_WRITE_REQUEST         0x18
#define ATT_EXECUTE_WRITE_RESPONSE        0x19
#define ATT_HANDLE_VALUE_NOTIFICATION     0x1B
#define ATT_HANDLE_VALUE_INDICATION       0x1D
#define ATT_HANDLE_VALUE_CONFIRMATION     0x1E
#define ATT_SIGNED_WRITE_COMMAND          0xD2

#define ATT_MTU_SIZE                      23

const uint8_t serviceDiscoveryResponse[] =
{
   0x07, // Find By Type Response Op Code.
   0x10, 0x00, 0x1F, 0x00, // Handle 0x0010-0x001F Has HPS.
};

const uint8_t charDiscoveryResponse1[] =
{
  0x09, // Read By Type Response Op Code.
  0x07, // Size of each Handle Value Pair.
  0x11, 0x00, 0x8A, 0x12, 0x00, 0xBA, 0xAA, // URI Characteristic
  0x14, 0x00, 0x8A, 0x15, 0x00, 0xBB, 0xAA, // Header Characteristic
  0x17, 0x00, 0x8A, 0x18, 0x00, 0xBC, 0xAA, // Entity Body Characteristic
  
};

const uint8_t charDiscoveryResponse2[] =
{
  0x09, // Read By Type Response Op Code.
  0x07, // Size of each Handle Value Pair.
  0x1A, 0x00, 0x10, 0x1B, 0x00, 0xBD, 0xAA, // Status Characteristic
  0x1D, 0x00, 0x08, 0x1E, 0x00, 0xBE, 0xAA, // Control Point Characteristic
  0x1F, 0x00, 0x02, 0x20, 0x00, 0xBF, 0xAA, // Security Characteristic
};


const uint8_t charDiscoveryResponse3[]=
{
  0x01, // Error Response
  0x08,
  0x21, 0x00,
  0x0A 
};


const uint8_t descDiscoveryResponse[]=
{
  0x05, // Find Information Response
  0x01, // Format indicating 16-bit UUID in Handle-UUID Pair
  0x1C, 0x00, //Handle
  0x02, 0x29  //UUID
};

const uint8_t writeResponse[]=
{
   0x13
};

const uint8_t executeWriteResponse[]=
{
   0x19
};

uint8_t statusNotification[]=
{
   0x1B, // Notification Opode
   0x1B, 0x00, // Value Handle for status characteristic.
   0xC8, 00, 00, 00
  
};

int l2capSock;
request_t m_request;
uint8_t ReadResponse[ATT_MTU_SIZE];

void l2cap_thread_cleanup(void);

void unpack_16_bit_int(uint8_t * data, uint16_t * value)
{
    (*value) = ((uint16_t)(data[1]));
    (*value) = (((*value) >> 8) | (data[0] & 0xFFFF));
}


void curl_http_response_cb(const response_t * p_response, request_t * p_request)
{
    int len;

    memset(m_uri, 0x00, 512);
    memset(m_header, 0x00, 512);
    memset(m_body, 0x00, 512);
    memset(m_header_response, 0x00, 512);
    memset(m_body_response, 0x00, 512);
    strcpy(m_header_response, p_response->http_header);
    strcpy(m_body_response, p_response->http_body);
    len = write(l2capSock,statusNotification, sizeof(statusNotification));
    if(len != sizeof(statusNotification))
    {
        printf ("Failed to send Status Notification\n");
    }
}


void * l2cap_thread_start(void * arg)
{
    char *hciDeviceIdOverride = NULL;
    int hciDeviceId = 0;
    le_advertising_info *leAdvertisingInfo =(le_advertising_info *) (arg);
    struct sockaddr_l2 sockAddr;
    socklen_t l2capConnInfoLen;
    int hciHandle;
    int result;
    char att_opcode;
    int readResponseLen;
    char *srcBuffer;
  

    fd_set rfds;
    struct timeval tv;

    char stdinBuf[256 * 2 + 1];
    char l2capSockBuf[256];
    int len,dataLen;
    int i;
    unsigned int data;
    uint16_t offset;
    uint16_t handle;

    // remove buffering 
    //setbuf(stdin, NULL);
    //setbuf(stdout, NULL);
    ///setbuf(stderr, NULL);

    hciDeviceIdOverride = getenv("NOBLE_HCI_DEVICE_ID");
    if (hciDeviceIdOverride != NULL)
    {
        hciDeviceId = atoi(hciDeviceIdOverride);
    }
    else
    {
        // if no env variable given, use the first available device
        hciDeviceId = hci_get_route(NULL);
    }

    if (hciDeviceId < 0)
    {
        hciDeviceId = 0; // use device 0, if device id is invalid
    }

    // create socket
    l2capSock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

    // bind
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.l2_family = AF_BLUETOOTH;
    bacpy(&sockAddr.l2_bdaddr, BDADDR_ANY);
    sockAddr.l2_cid = htobs(ATT_CID);

    result = bind(l2capSock, (struct sockaddr*)&sockAddr, sizeof(sockAddr));

    printf("bind %s\n", (result == -1) ? strerror(errno) : "success");

    // connect
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.l2_family = AF_BLUETOOTH;
    memcpy (&sockAddr.l2_bdaddr, &leAdvertisingInfo->bdaddr, 6 );
    sockAddr.l2_bdaddr_type = leAdvertisingInfo->bdaddr_type;
    sockAddr.l2_cid = htobs(ATT_CID);

    result = connect(l2capSock, (struct sockaddr *)&sockAddr, sizeof(sockAddr));

    l2capConnInfoLen = sizeof(l2capConnInfo);
    getsockopt(l2capSock, SOL_L2CAP, L2CAP_CONNINFO, &l2capConnInfo, &l2capConnInfoLen);
    hciHandle = l2capConnInfo.hci_handle;

    printf("connect handle %s 0x%04X\n", (result == -1) ? strerror(errno) : "success", hciHandle);

    if (result == -1)
    {
        l2cap_thread_cleanup();
    }

    while(1)
    {
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        FD_SET(l2capSock, &rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        result = select(l2capSock + 1, &rfds, NULL, NULL, &tv);

        if (-1 == result)
        {
            break;
        }
    
        if (FD_ISSET(0, &rfds))
        {
            len = read(0, stdinBuf, sizeof(stdinBuf));

            if (len <= 0)
            {
                break;
            }

            i = 0;
            while(stdinBuf[i] != '\n')
            {
                sscanf(&stdinBuf[i], "%02x", &data);
                l2capSockBuf[i / 2] = data;
                i += 2;
            }

            len = write(l2capSock, l2capSockBuf, (len - 1) / 2);  
        }
        if (FD_ISSET(l2capSock, &rfds))
        {
            len = read(l2capSock, l2capSockBuf, sizeof(l2capSockBuf));

            if (len <= 0)
            {
                break;
            }
            dataLen = len;
            printf ("l2capConnInfo.hci_handle 0x%04X\n", l2capConnInfo.hci_handle);

#if 0
            printf("data ");
            for(i = 0; i < len; i++)
            {
                printf("%02x", ((int)l2capSockBuf[i]) & 0xff);
            }
            printf("\n");
#endif //0
            att_opcode = l2capSockBuf[0];
            switch (att_opcode)
            {
                case ATT_FIND_INFO_REQUEST:
                {
                    len = write(l2capSock,descDiscoveryResponse, sizeof(descDiscoveryResponse));
                    if(len != sizeof(descDiscoveryResponse))
                    {
                        printf ("Failed to send Descriptor Discovery Response\n");
                    }
                    break;
                }
                case ATT_FIND_BY_TYPE_VALUE_REQUEST:
                {
                    len = write(l2capSock,serviceDiscoveryResponse, sizeof(serviceDiscoveryResponse));
                    if(len != sizeof(serviceDiscoveryResponse))
                    {
                        printf ("Failed to send Service Discovery Response\n");
                    }
                    break;
                }
                case ATT_READ_BY_TYPE_REQUEST:
                {
                    unpack_16_bit_int(&l2capSockBuf[1],&handle);
                    if(handle == 0x0010)
                    {
                        len = write(l2capSock,charDiscoveryResponse1, sizeof(charDiscoveryResponse1));
                        if(len != sizeof(charDiscoveryResponse1))
                        {
                         printf ("Failed to send Char Discovery Response 1\n");
                        }
                    }
                    else if(handle == 0x0020)
                    {
                        len = write(l2capSock,charDiscoveryResponse3, sizeof(charDiscoveryResponse3));
                        if(len != sizeof(charDiscoveryResponse3))
                        {
                         printf ("Failed to send Char Discovery Response 3\n");
                        }
                    }
                    else
                    {
                        len = write(l2capSock,charDiscoveryResponse2, sizeof(charDiscoveryResponse2));
                        if(len != sizeof(charDiscoveryResponse2))
                        {
                         printf ("Failed to send Char Discovery Response 2\n");
                        }
                    }
                    break;
                }
                case ATT_READ_REQUEST:
                {
                     ReadResponse[0] = ATT_READ_RESPONSE;
                     unpack_16_bit_int(&l2capSockBuf[1],&handle);

                     if (handle == URI_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_uri;
                     }
                     else if(handle == HEADER_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_header_response;
                     }
                     else if(handle == ENTITY_BODY_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_body_response;
                     }

                     readResponseLen = strlen(srcBuffer);
                     readResponseLen = (readResponseLen >= ATT_MTU_SIZE ? (ATT_MTU_SIZE -1) : readResponseLen);
                     printf ("srcBuffer %s\n", srcBuffer);
                     memcpy(&ReadResponse[1], srcBuffer, readResponseLen);
                     len = write(l2capSock,ReadResponse, readResponseLen);
                     if(len != readResponseLen)
                     {
                          printf ("Failed to send Read Response\n");
                     }
                     break;
                }
                case ATT_READ_BLOB_REQUEST:
                {
                     ReadResponse[0] = ATT_READ_BLOB_RESPONSE;
                     unpack_16_bit_int(&l2capSockBuf[1],&handle);
                     unpack_16_bit_int(&l2capSockBuf[3],&offset);

                     if (handle == URI_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_uri;
                     }
                     else if(handle == HEADER_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_header_response;
                     }
                     else if(handle == ENTITY_BODY_CHAR_VAL_HANDLE)
                     {
                          srcBuffer = m_body_response;
                     }

                     readResponseLen = strlen(srcBuffer) - offset;
                     readResponseLen = (readResponseLen >= ATT_MTU_SIZE ? (ATT_MTU_SIZE -1) : readResponseLen);
                     printf ("Offset %d\n", offset);
                     memcpy(&ReadResponse[1], srcBuffer+offset, readResponseLen);
                     len = write(l2capSock,ReadResponse, readResponseLen);
                     if(len != readResponseLen)
                     {
                          printf ("Failed to send Read Response\n");
                     }
                     break;
                }
                case ATT_WRITE_REQUEST:
                {
                    unpack_16_bit_int(&l2capSockBuf[1],&handle);
                    if(handle == CNTRL_PNT_CHAR_VAL_HANDLE)
                    {
                        printf ("HTTP Request Received\n");
                        printf ("URI: %s\n", m_uri);
                        printf ("Header: %s\n", m_header);
                        printf ("Body: %s\n", m_body);
                        init_request(&m_request);
                        m_request.uri = m_uri;
                        m_request.http_header = m_header;
                        if(strlen(m_body))
                        {
                            m_request.http_body = m_body;
                        }
                        if (l2capSockBuf[3] == 1)
                        {
                            m_request.controlpoint = "GET";
                        }
                        if (l2capSockBuf[3] == 4)
                        {
                            m_request.controlpoint = "PUT";
                        }
                        add_server_request(&m_request, curl_http_response_cb);
                    }
                    len = write(l2capSock,writeResponse, sizeof(writeResponse));
                    if(len != sizeof(writeResponse))
                    {
                        printf ("Failed to send Write Response\n");
                    }
                    break;
                }
                case ATT_PREPARE_WRITE_REQUEST:
                {
                    unpack_16_bit_int(&l2capSockBuf[1],&handle);
                    unpack_16_bit_int(&l2capSockBuf[3],&offset);

                    printf ("Handle 0x%04X, offset 0x%04X\n", handle, offset);

                    if (handle == URI_CHAR_VAL_HANDLE)
                    {
                        memcpy(&m_uri[offset], &l2capSockBuf[5], (dataLen-5)); 
                    }
                    else if(handle == HEADER_CHAR_VAL_HANDLE)
                    {
                        memcpy(&m_header[offset], &l2capSockBuf[5], (dataLen-5)); 
                    }
                    else if(handle == ENTITY_BODY_CHAR_VAL_HANDLE)
                    {
                        memcpy(&m_body[offset], &l2capSockBuf[5], (dataLen-5)); 
                    }

                    l2capSockBuf[0] = 0x17;
                    len = write(l2capSock,l2capSockBuf, len);
                    if(len != dataLen)
                    {
                        printf ("Failed to send Prepare Write Response\n");
                    }
                    break;
                }
                case ATT_EXECUTE_WRITE_REQUEST:
                {
                    len = write(l2capSock,executeWriteResponse, sizeof(executeWriteResponse));
                    if(len != sizeof(executeWriteResponse))
                    {
                      printf ("Failed to send Write Response\n");
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    return 0;
}


void l2cap_thread_cleanup(void)
{
    close(l2capSock);
    printf("disconnect\n");
}
