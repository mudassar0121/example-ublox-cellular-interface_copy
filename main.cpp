/* mbed Microcontroller Library
 * Copyright (c) 2017 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "UbloxATCellularInterface.h"
#include "UbloxPPPCellularInterface.h"

// If you wish to use LWIP and the PPP cellular interface on the mbed
// MCU, select the line UbloxPPPCellularInterface instead of the line
// UbloxATCellularInterface.  Using the AT cellular interface does not
// require LWIP and hence uses less RAM (significant on C027).  It also
// allows other AT command operations (e.g. sending an SMS) to happen
// during a data transfer.
#define INTERFACE_CLASS  UbloxATCellularInterface
//#define INTERFACE_CLASS  UbloxPPPCellularInterface

// The credentials of the SIM in the board.  If PIN checking is enabled
// for your SIM card you must set this to the required PIN.
#define PIN "0000"

// Network credentials.  You should set this according to your
// network/SIM card.  For C030 boards, leave the parameters as NULL
// otherwise, if you do not know the APN for your network, you may
// either try the fairly common "internet" for the APN (and leave the
// username and password NULL), or you may leave all three as NULL and then
// a lookup will be attempted for a small number of known networks
// (see APN_db.h in mbed-os/features/netsocket/cellular/utils).
#define APN         NULL
#define USERNAME    NULL
#define PASSWORD    NULL

// LEDs
DigitalOut ledRed(LED1, 1);
DigitalOut ledGreen(LED2, 1);
DigitalOut ledBlue(LED3, 1);

// The user button
volatile bool buttonPressed = false;

static void good() {
    ledGreen = 0;
    ledBlue = 1;
    ledRed = 1;
}

static void bad() {
    ledRed = 0;
    ledGreen = 1;
    ledBlue = 1;
}

static void event() {
    ledBlue = 0;
    ledRed = 1;
    ledGreen = 1;
}

static void pulseEvent() {
    event();
    wait_ms(500);
    good();
}

static void ledOff() {
    ledBlue = 1;
    ledRed = 1;
    ledGreen = 1;
}

static void printNtpTime(char * buf, int len)
{
    time_t timestamp = 0;
    struct tm *localTime;
    char timeString[25];
    time_t TIME1970 = 2208988800U;

    if (len >= 43) {
        timestamp |= ((int) *(buf + 40)) << 24;
        timestamp |= ((int) *(buf + 41)) << 16;
        timestamp |= ((int) *(buf + 42)) << 8;
        timestamp |= ((int) *(buf + 43));
        timestamp -= TIME1970;
        localTime = localtime(&timestamp);
        if (localTime) {
            if (strftime(timeString, sizeof(timeString), "%a %b %d %H:%M:%S %Y", localTime) > 0) {
                printf("NTP timestamp is %s.\n", timeString);
            }
        }
    }
}

static void cbButton()
{
    buttonPressed = true;
    pulseEvent();
}

/* This example program for the u-blox C030 and C027 boards instantiates
 * the UbloxAtCellularInterface or UbloxPPPCellularInterface and uses it
 *  to make a simple sockets connection to a server, using 2.pool.ntp.org
 * for UDP and developer.mbed.org for TCP.  For a more comprehensive example,
 * where higher layer protocols make use of the same sockets interface,
 * see example-ublox-mbed-client.
 * Progress may be monitored with a serial terminal running at 9600 baud.
 * The LED on the C030 board will turn green when this program is
 * operating correctly, pulse blue when a sockets operation is completed
 * and turn red if there is a failure.
 */

int main()
{
    INTERFACE_CLASS *interface = new INTERFACE_CLASS();
    // If you need to debug the cellular interface, comment out the
    // instantiation above and uncomment the one below.
//    INTERFACE_CLASS *interface = new INTERFACE_CLASS(MDMTXD, MDMRXD,
//                                                     MBED_CONF_UBLOX_CELL_BAUD_RATE,
//                                                     true);
    TCPSocket sockTcp;
    UDPSocket sockUdp;
    SocketAddress udpServer;
    SocketAddress udpSenderAddress;
    SocketAddress tcpServer;
    char buf[1024];
    int x;
#ifdef TARGET_UBLOX_C027
    // No user button on C027
    InterruptIn userButton(NC);
#else
    InterruptIn userButton(SW0);
#endif
    
    // Attach a function to the user button
    userButton.rise(&cbButton);
    
    good();
    printf("Starting up, please wait up to 180 seconds for network registration to complete...\n");
    if (interface->init(PIN)) {
        pulseEvent();
        interface->set_credentials(APN, USERNAME, PASSWORD);
        printf("Registered, connecting to the packet network...\n");
        for (x = 0; interface->connect() != 0; x++) {
            if (x > 0) {
                bad();
                printf("Retrying (have you checked that an antenna is plugged in and your APN is correct?)...\n");
            }
        }
        pulseEvent();
        
        printf("Getting the IP address of \"developer.mbed.org\" and \"2.pool.ntp.org\"...\n");
        if ((interface->gethostbyname("2.pool.ntp.org", &udpServer) == 0) &&
            (interface->gethostbyname("developer.mbed.org", &tcpServer) == 0)) {
            pulseEvent();
            udpServer.set_port(123);
            tcpServer.set_port(80);
            printf("\"2.pool.ntp.org\" address: %s on port %d.\n", udpServer.get_ip_address(), udpServer.get_port());
            printf("\"developer.mbed.org\" address: %s on port %d.\n", tcpServer.get_ip_address(), tcpServer.get_port());
            
            printf("Performing socket operations in a loop (until the user button is pressed on C030 or forever on C027)...\n");
            while (!buttonPressed) {
                // UDP Sockets
                printf("=== UDP ===\n");
                printf("Opening a UDP socket...\n");
                if (sockUdp.open(interface) == 0) {
                    pulseEvent();
                    printf("UDP socket open.\n");
                    sockUdp.set_timeout(10000);
                    printf("Sending time request to \"2.pool.ntp.org\" over UDP socket...\n");
                    memset (buf, 0, sizeof(buf));
                    *buf = '\x1b';
                    if (sockUdp.sendto(udpServer, (void *) buf, 48) == 48) {
                        pulseEvent();
                        printf("Socket send completed, waiting for UDP response...\n");
                        x = sockUdp.recvfrom(&udpSenderAddress, buf, sizeof (buf));
                        if (x > 0) {
                            pulseEvent();
                            printf("Received %d byte response from server %s on UDP socket:\n"
                                   "-------------------------------------------------------\n",
                                   x, udpSenderAddress.get_ip_address());
                            printNtpTime(buf, x);
                            printf("-------------------------------------------------------\n");
                        }
                    }                
                    printf("Closing socket...\n");
                    sockUdp.close();
                    pulseEvent();
                    printf("Socket closed.\n");
                }
                
                // TCP Sockets
                printf("=== TCP ===\n");
                printf("Opening a TCP socket...\n");
                if (sockTcp.open(interface) == 0) {
                    pulseEvent();
                    printf("TCP socket open.\n");
                    sockTcp.set_timeout(10000);
                    printf("Connecting socket to %s on port %d...\n", tcpServer.get_ip_address(), tcpServer.get_port());
                    if (sockTcp.connect(tcpServer) == 0) {
                        pulseEvent();
                        printf("Connected, sending HTTP GET request to \"developer.mbed.org\" over socket...\n");
                        strcpy (buf, "GET /media/uploads/mbed_official/hello.txt HTTP/1.0\r\n\r\n");
                        // Note: since this is a short string we can send it in one go as it will
                        // fit within the default buffer sizes.  Normally you should call sock.send()
                        // in a loop until your entire buffer has been sent.
                        if (sockTcp.send((void *) buf, strlen(buf)) == (int) strlen(buf)) {
                            pulseEvent();
                            printf("Socket send completed, waiting for response...\n");
                            x = sockTcp.recv(buf, sizeof (buf));
                            if (x > 0) {
                                pulseEvent();
                                printf("Received %d byte response from server on TCP socket:\n"
                                       "----------------------------------------------------\n%.*s"
                                       "----------------------------------------------------\n",
                                        x, x, buf);
                            }
                        }
                    }
                    printf("Closing socket...\n");
                    sockTcp.close();
                    pulseEvent();
                    printf("Socket closed.\n");
                }
                wait_ms(5000);
#ifndef TARGET_UBLOX_C027
                printf("[Checking if user button has been pressed]\n");
#endif
            }
            
            pulseEvent();
            printf("User button was pressed, stopping...\n");
            interface->disconnect();
            interface->deinit();
            ledOff();
            printf("Stopped.\n");
        } else {
            bad();
            printf("Unable to get IP address of \"developer.mbed.org\" or \"2.pool.ntp.org\".\n");
        }
    } else {
        bad();
        printf("Unable to initialise the interface.\n");
    }
}

// End Of File