#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "wsocket.h"
//static SOCKET_BASE sock_base;
///////////////////////////////////////////////////////////////
#define ssprintf(dest,fmt,...)  snprintf(dest,sizeof(dest)-1,fmt,##__VA_ARGS__)

static int static_debug_en;

void Set_Socket_Debug(int en) {
    static_debug_en = en;
}

bool Get_Socket_Debug() {
    return static_debug_en;
}

//void Socket_TRACE(int nColor,char* fmt,...) {
//    if (static_debug_en) {
//        va_list ap;
//        char sFormat[64] = {0};
//
//        strlcpy(sFormat, "%s(%d)", sizeof(sFormat) - 1);
//        strlcat(sFormat, fmt, sizeof(sFormat) - 1);
//        if (nColor) printf(sFormat,__func__,__LINE__,ap);
//        va_end(ap);
//    }
//}

void Socket_Init(SOCKET_BASE* sock_base, short PortNo) {
    sock_base->m_nRecv = 0;
    sock_base->m_socket = 0;
    sock_base->m_dwRemoteAdd = 0;
    sock_base->m_wRemotePort = 0;
    sock_base->m_PortNo = PortNo;
    sock_base->m_socket = INVALID_SOCKET;
    memset(sock_base->m_host_ip, 0x00, 4);
    memset(sock_base->m_buffer, 0x00, MAX_BUFF_SIZE);
}

/*
DWORD Socket_GetRemoteAdd();
WORD Socket_GetRemotePort();
void Socket_SetRemotePort(WORD port);
short Socket_GetPortNo();
void Socket_SetPortNo(short portNo);
bool CSocket_CheckSocketVersion();
void Socket_TRACE(const char* msg, int nColor = 0);
void Socket_TRACE(LPSTR msg, int nColor = 0);
int GetLastError(bool print = true);
DWORD IPconvertA2N(const char* ip);
LPSTR IPconvertN2A(DWORD dwIP);
 */

bool CheckSocketVersion() {
    /*
    WORD		wVersionRequested;
    WSADATA		wsaData;
    DWORD		err = 0;

    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if(err == SOCKET_ERROR)
    {
            char	szError[256];

            memset( szError,0x00,256 );
            ssprintf( szError,"WSAStartup Failed\n" );
            return false;
    }
    if(	LOBYTE( wsaData.wVersion ) != 2 ||
            HIBYTE( wsaData.wVersion ) != 2 ) { WSACleanup(); return false; }
     */
    return true;
}

void WSACleanup() {
    return;
}

BYTE* Socket_GetData(SOCKET_BASE* sock_base, int* nSize) {
    *nSize = sock_base->m_nRecv;
    return sock_base->m_buffer;
}

DWORD Socket_GetRemoteAdd(SOCKET_BASE* sock_base) {
    return sock_base->m_dwRemoteAdd;
}

WORD Socket_GetRemotePort(SOCKET_BASE* sock_base) {
    return sock_base->m_wRemotePort;
}

void Socket_SetRemotePort(SOCKET_BASE* sock_base, WORD port) {
    sock_base->m_wRemotePort = port;
}

short Socket_GetPortNo(SOCKET_BASE* sock_base) {
    return sock_base->m_PortNo;
}

void Socket_SetPortNo(SOCKET_BASE* sock_base, short portNo) {
    sock_base->m_PortNo = portNo;
}

DWORD Socket_GetIP(const char *if_name) {
    //    if (sock_base->m_socket >= 0) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct ifreq ifr;
        size_t if_name_len = strlen(if_name);
        struct sockaddr_in *ipaddr;

        if (if_name_len < sizeof (ifr.ifr_name)) {
            snprintf(ifr.ifr_name, sizeof(ifr.ifr_name)-1, "%s", if_name);
            ifr.ifr_name[if_name_len] = 0;
        } else {
            Socket_TRACE(DEBUG_BASE_BLACK, "ERR: interface name (%s) is too long\n", if_name);
            return 0;
        }

        if (ioctl(sock, SIOCGIFADDR, &ifr) == -1) {
            Socket_TRACE(DEBUG_BASE_BLACK, "ERROR: get ip address failed.error = %s\n", strerror(errno));
            return 0;
        }

        ipaddr = (struct sockaddr_in *) &ifr.ifr_addr;
        //	printf("\nLocal IP Address is %s\n", inet_ntoa(ipaddr->sin_addr));
        if (sock != INVALID_SOCKET) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
        return ipaddr->sin_addr.s_addr;
    }
    return 0;
}

/*
 * if this is the first connection on TCP server side
 * then backup standard IO fd to fd_stdout/fd_stderr
 */
#define STDIO_RESET(fd, STD)      do {\
    if (fd > 0) {\
        dup2(fd,STD);\
        close(fd);\
        fd = 0;\
    }\
} while (0)  
    
#define STDIO_REDIR(fd, sock, STD)  do {\
    STDIO_RESET(fd, STD);\
    fd = dup(STD);\
    dup2(sock,STD);\
} while (0)

void Socket_RedirectStdout(SOCKET_BASE* sock_base, bool on_off) {
    if (sock_base && Socket_IsConnected(sock_base)) {
        static int fd_std_in;
        static int fd_stdout;
        static int fd_stderr;
        if (!!on_off) {
            STDIO_REDIR(fd_std_in, sock_base->m_socket,STDIN_FILENO);
            STDIO_REDIR(fd_stdout, sock_base->m_socket,STDOUT_FILENO);
            STDIO_REDIR(fd_stderr, sock_base->m_socket,STDERR_FILENO);
            Socket_TRACE(DEBUG_BASE_BLACK,"Mapping STDIO to Remote...\n");
        } else {
//            int ttyfd = open("/dev/tty",(O_RDWR), 0644);  
//            dup2(ttyfd,STDOUT_FILENO);
            if (fd_stdout > 0)
                Socket_TRACE(DEBUG_BASE_BLACK,"Unmapping STDIO from Remote...\n");
            STDIO_RESET(fd_std_in, STDIN_FILENO);
            STDIO_RESET(fd_stdout, STDOUT_FILENO);
            STDIO_RESET(fd_stderr, STDERR_FILENO);
        }
    }
}

DWORD Socket_Accept(SOCKET_BASE* sock_base, SOCKET listener) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof (addr);
    sock_base->m_socket = accept(listener, (SOCKADDR*) & addr, &addrlen);
    //addr.sa_family
    if (sock_base->m_socket == INVALID_SOCKET) return 0;
    return addr.sin_addr.s_addr;
}

bool Socket_IsConnected(SOCKET_BASE* sock_base) {
    if (sock_base->m_socket != INVALID_SOCKET) return true;
    else return false;
}

static void Socket_Shutdown(SOCKET_BASE* sock_base, char* caller) {
    DWORD err = 0;

    if (sock_base->m_socket != INVALID_SOCKET) {
        err = shutdown(sock_base->m_socket, SHUT_RDWR);
        err = close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
    }
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"close failed: %s\n", strerror(errno));
    }
}

void Socket_Disconnect(SOCKET_BASE* sock_base) { // Close connection to remote host
    Socket_Shutdown(sock_base, __func__);
}

#define DGRAM_TYPE_MULTICAST    1
#define DGRAM_TYPE_BROADCAST    2

bool Socket_Bind(SOCKET_BASE* sock_base, int type, short port, LPSTR multicastIP) {
    int err = 0;
    int ReceiveBufferSize = MAX_BUFF_SIZE;
    int SendBufferSize = MAX_BUFF_SIZE;
    DWORD mode = SOCKET_ASYNCHRONOUS;

    if (!CheckSocketVersion()) return false;
    /* The Windows Sockets DLL is acceptable. Proceed. */
    // Open a socket to listen for incoming connections.
    Socket_Shutdown(sock_base, __func__);
    sock_base->m_socket = socket(AF_INET, type, 0); //type = SOCK_STREAM or SOCK_DGRAM
    if (sock_base->m_socket == INVALID_SOCKET) {
        Socket_TRACE(DEBUG_BASE_BLACK,"Socket Create Failed %s\n", strerror(errno));
        return false;
    }
    // Set the receive buffer size...
    err = setsockopt(sock_base->m_socket, SOL_SOCKET, SO_RCVBUF,
            (char*) &ReceiveBufferSize, sizeof (ReceiveBufferSize));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( SO_RCVBUF ) failed: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    // ...and the send buffer size for our new socket
    err = setsockopt(sock_base->m_socket, SOL_SOCKET, SO_SNDBUF,
            (char*) &SendBufferSize, sizeof (SendBufferSize));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( SO_SNDBUF ) failed: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    // Set the blocking and nonblocking mode
    //err = ioctlsocket( opt_socket,FIONBIO,&mode );	//Windows version
    fcntl(sock_base->m_socket, F_SETFL, O_NONBLOCK); //Linux version
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"Socket Bind Failed:Error on setting blocking/nonblocking mode,error=%s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    IN_ADDR IpAddress;
    SOCKADDR_IN localAddr;
    int dgramType = 0;

    if (multicastIP) { //printf("\ncast address = %s...\n",multicastIP);
        if (memcmp(multicastIP, "255.255.255.255", 15) == 0)
            dgramType = DGRAM_TYPE_BROADCAST;
        else dgramType = DGRAM_TYPE_MULTICAST;
    } else multicastIP = "";

    IpAddress.s_addr = htonl(INADDR_ANY);
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(port);
    localAddr.sin_addr = IpAddress;


    switch (type) {
        case SOCK_STREAM: break;
        case SOCK_DGRAM:
            if (port != 0) {
                if (dgramType == DGRAM_TYPE_BROADCAST) { //broadcast
                    const int so_broadcast = 1;
                    const int loop = false;
                    err = setsockopt(sock_base->m_socket, SOL_SOCKET, SO_BROADCAST,
                            (char*) &so_broadcast, sizeof (so_broadcast));
                    if (err == SOCKET_ERROR) {
                        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( Multicast ) failed: %s\n", strerror(errno));
                        close(sock_base->m_socket);
                        sock_base->m_socket = INVALID_SOCKET;
                        WSACleanup();
                        return false;
                    }
                } else if (dgramType == DGRAM_TYPE_MULTICAST) { //multicast
                    const int routenum = 1;
                    const int loop = false;
                    err = setsockopt(sock_base->m_socket, IPPROTO_IP,
                            IP_MULTICAST_TTL, (char*) &routenum, sizeof (routenum));
                    if (err == SOCKET_ERROR) {
                        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( Multicast ) failed: %s\n", strerror(errno));
                        close(sock_base->m_socket);
                        sock_base->m_socket = INVALID_SOCKET;
                        WSACleanup();
                        return false;
                    }
                    err = setsockopt(sock_base->m_socket, IPPROTO_IP,
                            IP_MULTICAST_LOOP, (char*) &loop, sizeof (loop));
                    if (err == SOCKET_ERROR) {
                        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( Multicast ) failed: %s\n", strerror(errno));
                        close(sock_base->m_socket);
                        sock_base->m_socket = INVALID_SOCKET;
                        WSACleanup();
                        return false;
                    }
                }
            }
            break;
        default:;
    }
    // Bind our server to the agreed upon port number.  See
    // commdef.h for the actual port number.
    if (type != SOCK_DGRAM || port != 0)
        err = bind(sock_base->m_socket, (PSOCKADDR) & localAddr, sizeof (localAddr));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"Socket Bind(port %d) Failed,error=%s\n", port,strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }
    // Prepare to accept client connections.  Allow up to 5 pending connections.
    switch (type) {
        case SOCK_STREAM:
            err = listen(sock_base->m_socket, 5);
            if (err == SOCKET_ERROR) {
                Socket_TRACE(DEBUG_BASE_BLACK,"Socket Listen Failed,error=%s\n", strerror(errno));
                close(sock_base->m_socket);
                sock_base->m_socket = INVALID_SOCKET;
                WSACleanup();
                return false;
            }
            break;
        case SOCK_DGRAM:
            if (port != 0 && dgramType == DGRAM_TYPE_MULTICAST) {
                struct ip_mreq mreq;
                memset(&mreq, 0x00, sizeof (mreq));
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                mreq.imr_multiaddr.s_addr = inet_addr(multicastIP);
                err = setsockopt(sock_base->m_socket,
                        IPPROTO_IP,
                        IP_ADD_MEMBERSHIP,
                        (char*) &mreq,
                        sizeof (mreq));
                if (err == SOCKET_ERROR) {
                    Socket_TRACE(DEBUG_BASE_BLACK,"Failed to join Multicast group %s, "
                            "GATEWAY not set? (error = %s)\n", multicastIP, strerror(errno));
                    sock_base->m_mcast = false;
                } else sock_base->m_mcast = true;
            }
            break;
        default:;
    }
    if (port) printf("%s Socket(0x%x) Bind OK on port : %d\n", (type == SOCK_STREAM) ? "TCP" : 
                        (type == SOCK_DGRAM ? "UDP" : "OTHER"), (unsigned int) sock_base->m_socket, (int)port);
    return true;
}

bool Socket_Connect(SOCKET_BASE* sock_base, int byte1, int byte2, int byte3, int byte4, int port) {
    IN_ADDR RemoteIpAddress;
    SOCKADDR_IN RemoteAddr;
    int ReceiveBufferSize = MAX_BUFF_SIZE;
    int SendBufferSize = MAX_BUFF_SIZE;
    int err;
    unsigned long mode = SOCKET_ASYNCHRONOUS;

    if (!CheckSocketVersion()) return false;
    // Open a socket using the internet Address family and TCP
    sock_base->m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_base->m_socket == INVALID_SOCKET) {
        Socket_TRACE(DEBUG_BASE_BLACK,"DoEcho: socket failed: %s\n", strerror(errno));
        return false;
    }
    // Set the receive buffer size...
    err = setsockopt(sock_base->m_socket, SOL_SOCKET, SO_RCVBUF,
            (char*) &ReceiveBufferSize, sizeof (ReceiveBufferSize));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( SO_RCVBUF ) failed: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        return false;
    }
    // ...and the send buffer size for our new socket
    err = setsockopt(sock_base->m_socket, SOL_SOCKET, SO_SNDBUF,
            (char*) &SendBufferSize, sizeof (SendBufferSize));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"setsockopt( SO_SNDBUF ) failed: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        return false;
    }
    // Connect to an agreed upon port on the host.  See the
    // commdef.h file for the actual port number
    ZeroMemory(&RemoteAddr, sizeof (RemoteAddr));

    //char	addr[4] = { byte4,byte3,byte2,byte1 };
    BYTE addr[4] = {byte1, byte2, byte3, byte4};
    RemoteIpAddress.s_addr = *(DWORD*) addr;
    RemoteAddr.sin_family = AF_INET;
    RemoteAddr.sin_port = htons(port);
    RemoteAddr.sin_addr = RemoteIpAddress;

    err = connect(sock_base->m_socket, (PSOCKADDR) & RemoteAddr, sizeof (RemoteAddr));
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"DoEcho: connect failed: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        return false;
    }
    // Set the blocking and nonblocking mode
    //err = ioctlsocket( clientsocket,FIONBIO,&mode );
    err = fcntl(sock_base->m_socket, F_SETFL, O_NONBLOCK); //Linux version
    if (err == SOCKET_ERROR) {
        Socket_TRACE(DEBUG_BASE_BLACK,"Connect Failed,error on setting blocking/nonblocking mode,error=%s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        return false;
    }
    return true;
}

int Socket_SendTo(SOCKET_BASE* sock_base, LPSTR ip, int port, LPSTR buffer, DWORD size) {
    int b1, b2, b3, b4;
    char addr[4];
    IN_ADDR ip_add;
    SOCKADDR_IN address;
    int tolen;

    ZeroMemory(&address, sizeof (address));

    sscanf(ip, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    addr[0] = (char) b1;
    addr[1] = (char) b2;
    addr[2] = (char) b3;
    addr[3] = (char) b4;
    ip_add.s_addr = *(DWORD*) addr;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr = ip_add;
    tolen = sizeof (address);
//    if (224 <= b1 && b1 <= 239 && !sock_base->m_mcast)
//        return 0;   //skip multicast packets if multicast is not currently enabled
    int nSend = sendto(sock_base->m_socket, buffer, size, 0, (PSOCKADDR) & address, tolen);
    if (nSend == SOCKET_ERROR) {
//        if (224 <= b1 && b1 <= 239 && !sock_base->m_mcast) return SOCKET_ERROR;
        
        Socket_TRACE(DEBUG_BASE_BLACK,"Error on UDP sendto %s:%d, error = %s\n", ip, port,strerror(errno));
        return SOCKET_ERROR;
    }
    return nSend;
}

LPSTR IPconvertN2A(DWORD dwIP) {
    static char sIP[16];
    DWORD b1 = 0x000000ff & (dwIP >> 24);
    DWORD b2 = 0x000000ff & (dwIP >> 16);
    DWORD b3 = 0x000000ff & (dwIP >> 8);
    DWORD b4 = 0x000000ff & dwIP;

    memset(sIP, 0x00, 16);
    snprintf(sIP, sizeof(sIP)-1, "%d.%d.%d.%d", b1, b2, b3, b4);
    return sIP;
}

DWORD IPconvertA2N(const char* ip) {
    DWORD dwIP = 0;
    DWORD b1 = 0, b2 = 0, b3 = 0, b4 = 0;
    sscanf(ip, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    dwIP = (0xff000000 & (b1 << 24)) |
            (0x00ff0000 & (b2 << 16)) |
            (0x0000ff00 & (b3 << 8)) |
            (0x000000ff & b4);
    return dwIP;
}

int Socket_RecvFromN(SOCKET_BASE* sock_base, DWORD* ip, int port, DWORD size) {
    IN_ADDR ip_add;
    SOCKADDR_IN address;
    socklen_t fromlen = 0;

    ZeroMemory(&ip_add, sizeof (ip_add));
    ZeroMemory(&address, sizeof (address));

    address.sin_addr.s_addr = *ip;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr = ip_add;
    fromlen = sizeof (address);

    int recvSize = recvfrom(sock_base->m_socket, sock_base->m_buffer, size, 0, (PSOCKADDR) & address, &fromlen);
    if (recvSize <= 0) {
        switch (errno) {
            case WSAEWOULDBLOCK: return 0; //no data
            case WSAENOTSOCK:
                Socket_TRACE(DEBUG_BASE_BLUE,"network disconnected");
                return 0; //
            case WSAECONNRESET:
                Socket_TRACE(DEBUG_BASE_BLUE,"%d.%d.%d.%d Offline",
                            0x000000ff & (address.sin_addr.s_addr >> 24),
                            0x000000ff & (address.sin_addr.s_addr >> 16),
                            0x000000ff & (address.sin_addr.s_addr >> 8),
                            0x000000ff & address.sin_addr.s_addr);
                return 0; //
            default:;
        }
        Socket_TRACE(DEBUG_BASE_BLACK,"Socket Error: %s\n", strerror(errno));
        close(sock_base->m_socket);
        sock_base->m_socket = INVALID_SOCKET;
        return SOCKET_ERROR;
    }

    *ip = address.sin_addr.s_addr;
    sock_base->m_nRecv = recvSize;

    return recvSize;
}

LPSTR Socket_RecvFromS(SOCKET_BASE* sock_base, LPSTR ip, int port, DWORD* size) { //return remote IP
    DWORD dwIP = IPconvertA2N(ip);
    *size = Socket_RecvFromN(sock_base, &dwIP, port, *size);
    if (*size <= 0) return NULL;
    return IPconvertN2A(dwIP);
}

int Socket_SendData(SOCKET_BASE* sock_base, LPSTR buffer, DWORD size) {
    int result = 0;
    int nRetry = 0;

    for (nRetry = 3000; nRetry > 0; usleep(1000), nRetry--) {
        if ((result = send(sock_base->m_socket, buffer, size, 0))
                != SOCKET_ERROR) return result;
        if (errno != WSAEWOULDBLOCK) break;
    }
    Socket_Shutdown(sock_base, __func__);
    Socket_TRACE(DEBUG_BASE_BLACK,"Socket Send Failed,error=%s\n", strerror(errno));
    return 0;
}

int Socket_RecvData(SOCKET_BASE* sock_base, DWORD size) {
    int result = 0;
    result = recv(sock_base->m_socket, (LPSTR) sock_base->m_buffer, size, 0);
    switch (result) {
        case 0:
            Socket_TRACE(DEBUG_BASE_BLUE,"The remote side has shut down the connection gracefully.\n");
            Socket_Shutdown(sock_base, __func__);
            return -1;
        case SOCKET_ERROR:
            switch (errno) {
                case WSAEWOULDBLOCK: return 0; //�����ӣ������
                case WSAENOTSOCK:
                    Socket_TRACE(DEBUG_BASE_BLUE,"The remote side has shut down the connection gracefully.\n");
                    return 0; //
                    //			case WSAECONNRESET:		printf("\n�Զ�����");		return 0;	//
                    //			case WSAECONNRESET:		TRACE_LOG(error_log,"The connection has been reset by remote side.\n");
                    //			case WSAENETDOWN:		TRACE_LOG(error_log,"The Windows Sockets implementation has detected that the network subsystem has failed.\n");
                    //			case WSAECONNABORTED:	TRACE_LOG(error_log,"The virtual circuit was aborted due to timeout or other failure.\n");
                    //			case WSAECONNRESET:		TRACE_LOG(error_log,"The virtual circuit was reset by the remote side.\n");
                default:
                    Socket_Shutdown(sock_base, __func__);
                    Socket_TRACE(DEBUG_BASE_BLACK,"Socket Recv Failed, Cause [%s]\n", strerror(errno));
                    return -1;
            }
            break;
        default:;
    }
    sock_base->m_nRecv = result;
    return result;
}
