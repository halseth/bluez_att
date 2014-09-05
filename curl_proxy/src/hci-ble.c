#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <pthread.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int lastSignal = 0;

typedef enum
{
    NONE,
    CONNECTABLE,
    CONNECTING,
    CONNECTED,
}state_t;

typedef struct
{
    bdaddr_t addr;
    uint8_t  type;
}hci_ble_bd_addr_t;

#define PEER_DEVICE_LIST_COUNT   1
#define HCI_RESET_CMD_LEN        4

#define USE_WHITE_LIST 0


const static hci_ble_bd_addr_t m_device_list[PEER_DEVICE_LIST_COUNT] =
{
#if 0
    {
      // {0xEC, 0x03, 0x0E, 0x28, 0x61, 0xFD},
       {0xFD, 0x61, 0x28, 0x0E, 0x03, 0xEC},
       LE_RANDOM_ADDRESS
    },
#endif
    {
       {0x30, 0x63, 0x12, 0xF0, 0xC5, 0xD9},
       //{0xD9, 0xC5, 0xF0, 0x12, 0x63, 0x30},
       LE_RANDOM_ADDRESS
    }
};


const uint8_t m_hci_reset_cmd[HCI_RESET_CMD_LEN] = {HCI_COMMAND_PKT, 0x03, 0x0C, 0x00};

uint8_t counter;
state_t gapState;
pthread_t m_l2cap_thread[PEER_DEVICE_LIST_COUNT];
int hciSocket;
char* adapterState = NULL;
void * l2cap_thread_start (void * arg);

void l2cap_thread_cleanup(void);

static void signalHandler(int signal)
{
     lastSignal = signal;
}

#if (USE_WHITE_LIST == 1)
static void set_whitelist(void)
{
    int i;
    for (i = 0; i < PEER_DEVICE_LIST_COUNT; i++)
    {
        int err = hci_le_add_white_list(hciSocket, &m_device_list[i].addr, m_device_list[i].type, 1000);
        printf ("Whitelist add result %d\n", err);
    }
}

#define search_device_list(X) 1

#else

#define set_whitelist()


static int search_device_list(hci_ble_bd_addr_t * addr) 
{
    int i;
    printf ("Search for device: \n");
    printf ("0x%02X: 0x%02X: 0x%02X: 0x%02X: 0x%02X: 0x%02X\n",
            addr->addr.b[0], addr->addr.b[1], addr->addr.b[2],
            addr->addr.b[3], addr->addr.b[4], addr->addr.b[5]);

    for (i = 0; i < PEER_DEVICE_LIST_COUNT; i++)
    {
        printf ("0x%02X: 0x%02X: 0x%02X: 0x%02X: 0x%02X: 0x%02X\n",
                 m_device_list[i].addr.b[0], m_device_list[i].addr.b[1], m_device_list[i].addr.b[2],
                 m_device_list[i].addr.b[3], m_device_list[i].addr.b[4], m_device_list[i].addr.b[5]);
        if(0 == memcmp(&m_device_list[i], addr, sizeof(hci_ble_bd_addr_t)))
        {
             printf ("Found device %d\n", i);
             return 1;
        }
    }
    return 0;
}

#endif


#ifdef SCANNER
static void enter_connectable_mode(void)
{
    int err = hci_le_set_scan_enable(hciSocket, 0x00, 0, 1000); //Disable before starting again.
    err = hci_le_set_scan_parameters(hciSocket, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 10000);
    if (err == 0)
    {
         err = hci_le_set_scan_enable(hciSocket, 0x01, 0, 10000);
         if (err == 0)
         {
              printf ("Scan started!\n");
              adapterState = "Connectable!";
             gapState = CONNECTABLE;
         }
    }
}

static void exit_connectable_mode(void)
{
    int err = hci_le_set_scan_enable(hciSocket, 0x00, 0, 10000);
    if (err == 0)
    {
         printf ("Scan stopped!\n");
         adapterState = "NonConnectable!";
         gapState = NONE;
    }

}

#else// SCANNER


int hci_le_set_advertising_data(int dd, uint8_t* data, uint8_t length, int to)
{
  struct hci_request rq;
  le_set_advertising_data_cp data_cp;
  uint8_t status;

  memset(&data_cp, 0, sizeof(data_cp));
  data_cp.length = length;
  memcpy(&data_cp.data, data, sizeof(data_cp.data));

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &data_cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(dd, &rq, to) < 0)
    return -1;

  if (status) {
    errno = EIO;
    return -1;
  }

  return 0;
}


static void enter_connectable_mode(void)
{
    int err =  hci_le_set_advertise_enable (hciSocket, 0x00, 10000);
    err =  hci_le_set_advertise_enable (hciSocket, 0x01, 10000);
    if (err == 0)
    {
         printf ("Advertising started!\n");
         adapterState = "Connectable!";
         gapState = CONNECTABLE;
    }
}

static void exit_connectable_mode(void)
{
     int  err = hci_le_set_advertise_enable(hciSocket, 0x00, 1000);
     if (err == 0)
     {
          printf ("Advertising stopped!\n");
          adapterState = "NonConnectable!";
          gapState = NONE;
     }
}

#endif //SCANNER


int main(int argc, const char* argv[])
{
     char *hciDeviceIdOverride = NULL;
     int hciDeviceId = 0;
     int err;
     struct hci_dev_info hciDevInfo;

     struct hci_filter oldHciFilter;
     struct hci_filter newHciFilter;
     socklen_t oldHciFilterLen;

     int previousAdapterState = -1;
     int currentAdapterState;
  
     fd_set rfds;
     struct timeval tv;
     int selectRetval;

     unsigned char hciEventBuf[HCI_MAX_EVENT_SIZE];
     int hciEventLen;
     evt_le_meta_event *leMetaEvent;
     le_advertising_info *leAdvertisingInfo;
     char btAddress[18];
     int i;
     int8_t rssi;
     uint16_t handle;
     hci_ble_bd_addr_t advAddress;

     memset(&hciDevInfo, 0x00, sizeof(hciDevInfo));

     // setup signal handlers
     signal(SIGINT, signalHandler);
     signal(SIGSTOP, signalHandler);
     signal(SIGKILL, signalHandler);
     signal(SIGUSR1, signalHandler);
     signal(SIGUSR2, signalHandler);
     signal(SIGHUP, signalHandler);

     prctl(PR_SET_PDEATHSIG, SIGINT);

     // remove buffering 
     setbuf(stdin, NULL);
     setbuf(stdout, NULL);
     setbuf(stderr, NULL);

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

     // setup HCI socket
     hciSocket = hci_open_dev(hciDeviceId);

     if (hciSocket == -1)
     {
          printf("adapterState unsupported\n");
          return -1;
     }
     hciDevInfo.dev_id = hciDeviceId;

#if 0
     // Send HCI Reset to avoid restart issues.
     if (HCI_RESET_CMD_LEN != write(hciSocket, m_hci_reset_cmd, HCI_RESET_CMD_LEN))
     {
         printf ("Failed to send HCI reset\n");
     }

     sleep(2);
#endif //0

     // get old HCI filter
     oldHciFilterLen = sizeof(oldHciFilter);
     getsockopt(hciSocket, SOL_HCI, HCI_FILTER, &oldHciFilter, &oldHciFilterLen);

     // setup new HCI filter
     hci_filter_clear(&newHciFilter);
     hci_filter_set_ptype(HCI_EVENT_PKT, &newHciFilter);
     hci_filter_set_event(EVT_LE_META_EVENT, &newHciFilter);

     setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &newHciFilter, sizeof(newHciFilter));

     while(1)
     {
          if((lastSignal == SIGKILL) || (lastSignal == SIGSTOP) || (lastSignal == SIGINT))
          {
             break;
          }
          FD_ZERO(&rfds);
          FD_SET(hciSocket, &rfds);

          tv.tv_sec = 2;
          tv.tv_usec = 0;

          // get HCI dev info for adapter state
          ioctl(hciSocket, HCIGETDEVINFO, (void *)&hciDevInfo);
          currentAdapterState = hci_test_bit(HCI_UP, &hciDevInfo.flags);

          if (previousAdapterState != currentAdapterState)
          {
                previousAdapterState = currentAdapterState;
                adapterState = "unknown";

                if (!currentAdapterState)
                {
                     adapterState = "poweredOff";
                }
                else
                {
                    initialize(PEER_DEVICE_LIST_COUNT, 10000);

                    set_whitelist();
                    enter_connectable_mode();
                }

                 printf("adapterState %s\n", adapterState);
                 if (gapState == NONE)
                 {
                     break;
                 }
            }

            selectRetval = select(hciSocket + 1, &rfds, NULL, NULL, &tv);

            if (-1 == selectRetval)
            {
                 perror("select()");
                 break;
            }
            else if (selectRetval)
            {
                 //printf ("leMetaEvent->subevent %02X, state 0x%08X\n",leMetaEvent->subevent, gapState);
                 // read event
                 hciEventLen = read(hciSocket, hciEventBuf, sizeof(hciEventBuf));
                 leMetaEvent = (evt_le_meta_event *)(hciEventBuf + (1 + HCI_EVENT_HDR_SIZE));
                 hciEventLen -= (1 + HCI_EVENT_HDR_SIZE);

                 if (gapState == NONE)
                 {
                     // ignore, not connectable
                     continue;
                 }


                switch (leMetaEvent->subevent)
                {
                    case 0x01: // Connection Complete
                    {
                         printf ("Connected, handle 0x%04X\n", handle);
                         // create L2CAP thread to handle ATT traffic.
                         err = pthread_create(&m_l2cap_thread[counter], NULL, l2cap_thread_start, leAdvertisingInfo);
                         if(err == 0)
                         {
                              printf ("L2CAP thread setup to handle ATT traffic, hurrah!\n");
                         }
                         enter_connectable_mode();
                         break;
                     }
                     case 0x02: //Advertising report
                     {
                          if (gapState != CONNECTABLE)
                          {
                              continue;
                          }
                          leAdvertisingInfo = (le_advertising_info *)(leMetaEvent->data + 1);
                	  ba2str(&leAdvertisingInfo->bdaddr, btAddress);
                          memcpy(advAddress.addr.b, leAdvertisingInfo->bdaddr.b, 6);
                          advAddress.type = leAdvertisingInfo->bdaddr_type;

                          //printf("Peer Addr %s,%s,\n", btAddress, (leAdvertisingInfo->bdaddr_type == LE_PUBLIC_ADDRESS) ? "public" : "random");
                          exit_connectable_mode();
                          //err = hci_le_set_scan_parameters(hciSocket, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 1, 1000);
                          if (search_device_list(&advAddress))
                          {
                               printf ("Requesting connection\n");
                               err = hci_le_create_conn(hciSocket,
                                              htobs(0x0010),
                                              htobs(0x0010),
                                              USE_WHITE_LIST,
                                              leAdvertisingInfo->bdaddr_type,
                                              leAdvertisingInfo->bdaddr,
                                              LE_PUBLIC_ADDRESS,
                                              htobs(0x0020),
                                              htobs(0x0060),
                                              0,
                                              htobs(0x0100),
                                              htobs(0x0010),
                                              htobs(0x0020),
                                              &handle,
                                              1000);
                               if(err == 0)
                               {
                                    printf ("Connected, handle 0x%04X\n", handle);
                                    // create L2CAP thread to handle ATT traffic.
                                    err = pthread_create(&m_l2cap_thread[counter], NULL, l2cap_thread_start, leAdvertisingInfo);
                                    if(err == 0)
                                    {
                                         printf ("L2CAP thread setup to handle ATT traffic, hurrah!\n");
                                    }
                                    counter++;
                               }
                               else
                               {
                                    printf("Connection establishment failed. Err = %d\n", err);
                               }
                          }
                          sleep(2);
                          if (counter < PEER_DEVICE_LIST_COUNT)
                          {
                              enter_connectable_mode();
                          }
                          break;
                     }
                     default:
                          continue;
                }
          }
          else
          {
               printf ("*");
          }
     }

    exit_connectable_mode();

    // restore original filter
    setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &oldHciFilter, sizeof(oldHciFilter));

    // Send HCI Reset to avoid restart issues.
    if (HCI_RESET_CMD_LEN != write(hciSocket, m_hci_reset_cmd, HCI_RESET_CMD_LEN))
    {
         printf ("Failed to send HCI reset\n");
    }
#if 0
    l2cap_thread_cleanup();
    printf ("Bye!\n");
    for (i = 0; i < counter; i++)
    {
        pthread_join(m_l2cap_thread[counter], NULL);
    }
#endif //0 

    close(hciSocket);

    return 0;
}

