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

typedef struct
{
    bdaddr_t addr;
    uint8_t  type;
}hci_ble_bd_addr_t;

#define PEER_DEVICE_LIST_COUNT 1
#define HCI_RESET_CMD_LEN      4

static hci_ble_bd_addr_t m_device_list[] =
{
    {
       {0xEC, 0x03, 0x0E, 0x28, 0x61, 0xFD},
       //{0xFD, 0x61, 0x28, 0x0E, 0x03, 0xEC},
       LE_RANDOM_ADDRESS
    }
};

static char * peer_addr = "EC:03:0E:28:61:FD";

static uint8_t m_hci_reset_cmd[HCI_RESET_CMD_LEN] = {HCI_COMMAND_PKT, 0x03, 0x0C, 0x00};
static pthread_t m_l2cap_thread;

void * l2cap_thread_start (void * arg);

void l2cap_thread_cleanup(void);

static void signalHandler(int signal)
{
     lastSignal = signal;
}

int main(int argc, const char* argv[])
{
     char *hciDeviceIdOverride = NULL;
     int hciDeviceId = 0;
     int hciSocket;
     int err;
     struct hci_dev_info hciDevInfo;

     struct hci_filter oldHciFilter;
     struct hci_filter newHciFilter;
     socklen_t oldHciFilterLen;

     int previousAdapterState = -1;
     int currentAdapterState;
     const char* adapterState = NULL;
  
     fd_set rfds;
     struct timeval tv;
     int selectRetval;

     unsigned char hciEventBuf[HCI_MAX_EVENT_SIZE];
     int hciEventLen;
     evt_le_meta_event *leMetaEvent;
     le_advertising_info *leAdvertisingInfo;
     char btAddress[18];
     int i;
     int scanning = 0;
     int8_t rssi;
     uint16_t handle;

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
#if 0
                    err = hci_le_clear_white_list(hciSocket, 1000);

                    err = hci_le_add_white_list(hciSocket, &m_device_list[0].addr, m_device_list[0].type, 1000);
                    printf ("Whitelist add result %d\n", err);

                    err = hci_le_set_scan_parameters(hciSocket, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 1, 1000);
#else // 0
                    err = hci_le_set_scan_parameters(hciSocket, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0, 1000);
#endif // 0
                    if (err == 0)
                    {
                         hci_le_set_scan_enable(hciSocket, 0x00, 1, 1000);
                         err = hci_le_set_scan_enable(hciSocket, 0x01, 1, 1000);
                         if (err == 0)
                         {
                              adapterState = "Scanning!";
                              scanning = 1;
                         }
                     }
                 }

                 printf("adapterState %s\n", adapterState);
                 if (!scanning)
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
                // read event
                hciEventLen = read(hciSocket, hciEventBuf, sizeof(hciEventBuf));
                leMetaEvent = (evt_le_meta_event *)(hciEventBuf + (1 + HCI_EVENT_HDR_SIZE));
                hciEventLen -= (1 + HCI_EVENT_HDR_SIZE);

                if (!scanning)
                {
                    // ignore, not scanning
                   continue;
                }

                printf (".");

               if (leMetaEvent->subevent != 0x02)
               {
                   continue;
               }

               leAdvertisingInfo = (le_advertising_info *)(leMetaEvent->data + 1);
	       ba2str(&leAdvertisingInfo->bdaddr, btAddress);

               printf("event %s,%s,\n", btAddress, (leAdvertisingInfo->bdaddr_type == LE_PUBLIC_ADDRESS) ? "public" : "random");
               hci_le_set_scan_enable(hciSocket, 0x00, 1, 1000);
               if (0 == memcmp(peer_addr,btAddress, strlen(peer_addr)))
               {
                    err = hci_le_create_conn(hciSocket,
                                             htobs(0x0010),
                                             htobs(0x0010),
                                             0,
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
                         err = pthread_create(&m_l2cap_thread, NULL, l2cap_thread_start, leAdvertisingInfo);
                         if(err == 0)
                         {
                              printf ("L2CAP thread setup to handle ATT traffic, hurrah!\n");
                         } 
                    }
               }
               hci_le_set_scan_enable(hciSocket, 0x01, 1, 1000);
          }
          else
          {
               printf ("*");
          }
     }

    // restore original filter
    setsockopt(hciSocket, SOL_HCI, HCI_FILTER, &oldHciFilter, sizeof(oldHciFilter));

    // disable LE scan
    hci_le_set_scan_enable(hciSocket, 0x00, 0, 1000);

    // Send HCI Reset to avoid restart issues.
    if (HCI_RESET_CMD_LEN != write(hciSocket, m_hci_reset_cmd, HCI_RESET_CMD_LEN))
    {
         printf ("Failed to send HCI reset\n");
    }
    l2cap_thread_cleanup();
    printf ("Bye!\n");
    pthread_join(m_l2cap_thread, NULL);

    close(hciSocket);

    return 0;
}

