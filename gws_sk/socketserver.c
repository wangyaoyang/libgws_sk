
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>

#include "wsocket.h"

/////////////////////////////////////////////////////////////////////////
bool UdpServer_Init(SOCKET_UDP_SERVER* server, short PortNo,
                    PIPE_CALLBACK_ON_SEND pOnSend,
                    PIPE_CALLBACK_ON_RECV pOnRecv) {
    memset (server,0x00,sizeof(SOCKET_UDP_SERVER));
//    SocketPipe_Init();
//    SocketPipe_CreatePipe();
    bool bInit = SocketConn_Init(&server->m_connection,PortNo,pOnSend,pOnRecv);
    server->m_udpsvrThread = INVALID_THREAD;
    pthread_mutex_init(&server->m_udpsvrEvent, NULL);
    return bInit;
}

static void UdpServer_CleanUp(SOCKET_UDP_SERVER* server) {
    if (Socket_IsConnected(&server->m_connection.sock_base))
        SocketConn_DisConnect(&server->m_connection);
}

static char* UdpServer_GetRemoteIP(SOCKET_UDP_SERVER* server) {
    if (server->m_connection.sock_base.m_dwRemoteAdd != 0) {
        return IPconvertN2A(server->m_connection.sock_base.m_dwRemoteAdd);
    } else if (strlen(server->m_connection.m_multicastIP) > 0) {
        return server->m_connection.m_multicastIP;
    } else return NULL;
}

void UdpServer_StopListenThread(SOCKET_UDP_SERVER* server) {
    UdpServer_CleanUp(server);
    if (server->m_udpsvrThread != INVALID_THREAD) {
        pthread_mutex_lock(&server->m_udpsvrEvent);
        pthread_join(server->m_udpsvrThread, NULL);
        pthread_mutex_unlock(&server->m_udpsvrEvent);
        pthread_mutex_destroy(&server->m_udpsvrEvent);
        server->m_udpsvrThread = INVALID_THREAD;
    }
//    SocketPipe_Exit();
}

static bool UdpServer_ListenState(SOCKET_UDP_SERVER* server) {
    if (pthread_mutex_trylock(&server->m_udpsvrEvent) == 0) {
        pthread_mutex_unlock(&server->m_udpsvrEvent);
        return true;
    }
    printf("\n%s(%d)\n",__func__,__LINE__);
    return false;
}

void UdpServer_SetDestIpPort(SOCKET_BASE* sock,DWORD ip,WORD port) {
    if (sock) {
        if (ip)     sock->m_dwRemoteAdd = ip;
        if (port)   sock->m_wRemotePort = port;
    }
}

DWORD UdpServer_GetDestIP(SOCKET_BASE* sock) {
    if (sock)
        return sock->m_dwRemoteAdd;
    return 0;
}

void UdpServer_SetDestAddr(SOCKET_BASE* sock,LPSTR ip,WORD port) {
    if (sock) {
        if (ip)     sock->m_dwRemoteAdd = IPconvertA2N(ip);
        if (port)   sock->m_wRemotePort = port;
    }
}

static void* UdpServer_MainLoop(LPVOID lp) {
    if (lp) {
        SOCKET_UDP_SERVER* server = (SOCKET_UDP_SERVER*) lp;
        short nPort = Socket_GetPortNo(&server->m_connection.sock_base);

        printf("UDP server(0x%x) start listening on port : %d\n",
                        server->m_connection.sock_base.m_socket,nPort);
        while (UdpServer_ListenState(server)) {
            if (Socket_IsConnected(&server->m_connection.sock_base)) { //if already connected,detect incomming data
                BYTE buf_send[MAX_BUFF_SIZE];
                int nSend = 0;
                char*   remoteIP = NULL;
                DWORD   remotePort = 0;
                if (server->m_connection.m_pOnSend &&
                    (nSend = server->m_connection.m_pOnSend(&server->m_connection.sock_base,buf_send)) > 0 &&
                    (remoteIP = UdpServer_GetRemoteIP(server)) &&
                    (remotePort = Socket_GetRemotePort(&server->m_connection.sock_base)) > 0)
                    SocketConn_UDP_SendS(&server->m_connection,remoteIP,remotePort,buf_send, nSend);
            } else {
                Socket_TRACE(DEBUG_UDPS_BLACK,"UDP server Disconnected\n");
                break;
            }
            usleep(100000);
        }
        UdpServer_CleanUp(server);
        Socket_TRACE(DEBUG_UDPS_BLACK,"UDP server Stop Listening\n");
    }
    return NULL;
}

bool UdpServer_StartListenThread(SOCKET_UDP_SERVER* server, LPSTR multicastIP, short port) {
    UdpServer_StopListenThread(server);
    if (SocketConn_UDP_Bind(&server->m_connection,multicastIP,port)) {
        if (pthread_create(&server->m_udpsvrThread, NULL, UdpServer_MainLoop, server) != 0) {
            server->m_udpsvrThread = INVALID_THREAD;
            return false;
        }
        return true;
    } else UdpServer_CleanUp(server);
    return false;
}

/////////////////////////////////////////////////////////////////////////
bool TcpServer_Init( SOCKET_TCP_SERVER* server, short PortNo,
                        PIPE_CALLBACK_ON_SEND pOnSend,
                        PIPE_CALLBACK_ON_RECV pOnRecv) {
    int pos = 0;
    memset (server,0x00,sizeof(SOCKET_TCP_SERVER));
    SocketPipe_Init();
    SocketPipe_CreatePipe();
    Socket_Init(&server->m_listener,PortNo);
    for (pos = 0; pos < MAX_CONNECTIONS; pos++) {
        SocketConn_Init(&server->m_connection[pos],PortNo,pOnSend,pOnRecv);
    }
    server->m_listenThread = INVALID_THREAD;
    pthread_mutex_init(&server->m_listenEvent, NULL);
    return true;
}

static void TcpServer_CleanUp(SOCKET_TCP_SERVER* server) {
    int pos = 0;
    for (pos = 0; pos < MAX_CONNECTIONS; pos++) {
        if (Socket_IsConnected(&server->m_connection[pos].sock_base))
            SocketConn_DisConnect(&server->m_connection[pos]);
    }
}

void TcpServer_StopListenThread(SOCKET_TCP_SERVER* server) {
    TcpServer_CleanUp(server);
    if (server->m_listenThread != INVALID_THREAD) {
        pthread_mutex_lock(&server->m_listenEvent);
        pthread_join(server->m_listenThread, NULL);
        pthread_mutex_unlock(&server->m_listenEvent);
        pthread_mutex_destroy(&server->m_listenEvent);
        server->m_listenThread = INVALID_THREAD;
    }
    SocketPipe_Exit();
}

static bool TcpServer_ListenState(SOCKET_TCP_SERVER* server) {
    if (pthread_mutex_trylock(&server->m_listenEvent) == 0) {
        pthread_mutex_unlock(&server->m_listenEvent);
        return true;
    }
    return false;
}

static void TcpServer_OnRecv(SOCKET_TCP_SERVER* server,int index, BYTE* pData, int nData) {
    if (0 <= index && index < MAX_CONNECTIONS) {
        SOCKET_CONN* conn = &server->m_connection[index];
        if (Socket_IsConnected(&conn->sock_base) && conn->m_pOnRecv) {
            conn->m_pOnRecv(&conn->sock_base,pData,nData);
        }
    }
}

static void* TcpServer_MainLoop(LPVOID lp) {
    if (lp) {
        SOCKET_TCP_SERVER* server = (SOCKET_TCP_SERVER*) lp;
        int pos = 0;
        DWORD exterr = 0;
        short nPort = Socket_GetPortNo(&server->m_connection[0].sock_base);

//        if (SocketPipe_RedirectStdIO())
//            Socket_TRACE(DEBUG_TCPS_BLACK,"OK on redirecting STDIO ot socket\n");
//        else Socket_TRACE(DEBUG_TCPS_BLACK,"Failed on redirecting STDIO ot socket\n");
        printf("TCP server(0x%x) Start listening on port : %d\n", server->m_listener.m_socket,nPort);
        while (TcpServer_ListenState(server)) {
            for (pos = 0; pos < MAX_CONNECTIONS; pos++) {
                SOCKET_CONN* conn = &server->m_connection[pos];
                if (Socket_IsConnected(&conn->sock_base)) { //if already connected,detect incomming data
                    BYTE buf_send[MAX_BUFF_SIZE];
                    int nSend = 0;
                    if (conn->m_pOnSend && (nSend = conn->m_pOnSend(&conn->sock_base,buf_send)) > 0)
                        SocketConn_TCP_Send(conn,(char*) buf_send, nSend);
                    if (SocketConn_IsUpdate(conn)) {
                        int nData = 0;
                        BYTE* pData = Socket_GetData(&conn->sock_base,&nData);
                        if (nData > 0)
                            TcpServer_OnRecv(server, pos, pData, nData);
                        SocketConn_Release(conn);
                    }
                } else { //if not connected,detect any connect request
                    exterr = SocketConn_Accept(conn,&server->m_listener);
                    switch (exterr) {
                        case 0:
                            printf("\nChannel [%d] : Accpet a connection.\n", pos);
                            if (server->m_redir_stdout) {
                                BYTE* ip = server->m_listener.m_host_ip;
                                Socket_RedirectStdout(&conn->sock_base, true);
                                printf("==== %u.%u.%u.%u ====\r\n#",  ip[0], ip[1], ip[2], ip[3]);
                                fflush(stdout);
                            }
                            break; //�к��룬Accept�ɹ���������һ�ڵ����̽ѯ��
                        case WSAEWOULDBLOCK: //�޺��룬Acceptʧ�ܣ��пսڵ㵫�޺���,��ͷ��ѯ��
                            goto START_POLLING;
                        default:;
                    }
                }
            } //ѭ���������������ͨ������ռ�á�
START_POLLING:
            usleep(50000);
        }
        Socket_TRACE(DEBUG_TCPS_BLACK,"TCP server Stop Listening\n");
    }
    return NULL;
}

bool TcpServer_StartListenThread(SOCKET_TCP_SERVER* server) { //���������߳�
    SOCKET_BASE* listener = &server->m_listener;

    TcpServer_StopListenThread(server);
    if (SocketConn_TCP_Bind(listener)) {
        //short	nPortNo = _1stConnection->m_GetPortNo();
        if (pthread_create(&server->m_listenThread, NULL, TcpServer_MainLoop, server) != 0) {
            server->m_listenThread = INVALID_THREAD;
            return false;
        }
        return true;
    } else TcpServer_CleanUp(server);
    return false;
}

//
int TcpServer_Browse(SOCKET_TCP_SERVER* server, int* index) {
    int length = 0,position;
    for (position = 0; position < MAX_CONNECTIONS; position++) {
        SOCKET_CONN* conn = &server->m_connection[position];
        if (Socket_IsConnected(&conn->sock_base)) {
            length = SocketConn_TCP_Recv(conn,MAX_BUFF_SIZE);
            switch (length) {
                case EOF    : SocketConn_DisConnect(conn);  *index = EOF;   break; //
                case 0      : *index = EOF;                                 break; //
                default     : *index = position; return length; //
            }
        }
    }
    return length;
}

int TcpServer_Recv(SOCKET_TCP_SERVER* server, int index/*input*/) {
    if (index < 0 || index > MAX_CONNECTIONS - 1) return 0;
    SOCKET_CONN* conn = &server->m_connection[index];
    if (Socket_IsConnected(&conn->sock_base))
        return SocketConn_TCP_Recv(conn, MAX_BUFF_SIZE);
    return 0;
}

int TcpServer_Send(SOCKET_TCP_SERVER* server, int index, char* buffer, DWORD size) {
    if (index < 0 || index > MAX_CONNECTIONS - 1) return 0;
    SOCKET_CONN* conn = &server->m_connection[ index ];
    if (Socket_IsConnected(&conn->sock_base))
        return SocketConn_TCP_Send(conn, buffer, size);
    return 0;
}

int TcpServer_Scan(SOCKET_TCP_SERVER* server, int* index/*input*/) { //Ϊ0�����ʾ�����
    int counter = 0;

    for (*index = 0; *index < MAX_CONNECTIONS; *index++) {
        if ((counter = TcpServer_Recv(server, *index)) > 0) return counter;
    }
    return counter;
}

int TcpServer_Broadcast(SOCKET_TCP_SERVER* server, char* buffer, DWORD size) {
    int counter = 0,position;

    for (position = 0; position < MAX_CONNECTIONS; position++) {
        if (TcpServer_Send(server, position, buffer, size) > 0) counter++;
    }
    return counter;
}

bool TcpServer_DisConnect(SOCKET_TCP_SERVER* server, int index) {
    if (index < 0 || index > MAX_CONNECTIONS - 1) return false;
    SOCKET_CONN* conn = &server->m_connection[ index ];
    if (Socket_IsConnected(&conn->sock_base)) {
        SocketConn_DisConnect(conn);
        return true;
    }
    return false;
}

int TcpServer_GetConnectionCount(SOCKET_TCP_SERVER* server) {
    int count = 0,position;
    for (position = 0; position < MAX_CONNECTIONS; position++) {
        SOCKET_CONN* conn = &server->m_connection[ position ];
        if (Socket_IsConnected(&conn->sock_base)) count++;
    }
    return count;
}

BYTE* TcpServer_GetData(SOCKET_TCP_SERVER* server, int index) {
    if (server && 0 <= index && index < MAX_CONNECTIONS) {
        return server->m_connection[index].sock_base.m_buffer;
    }
    return NULL;
}
////////////////////////////////////////////////////////
