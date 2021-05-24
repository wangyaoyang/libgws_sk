#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>

#include "wsocket.h"

static SOCKET_PIPE socket_pipe;

#define PIPE_PARENT_ERR         socket_pipe.m_pipe_er[0]
#define PIPE_CHILD_ERR          socket_pipe.m_pipe_er[1]
#define PIPE_PARENT_READ        socket_pipe.m_pipe_up[0]
#define PIPE_CHILD_WRITE        socket_pipe.m_pipe_up[1]
#define PIPE_CHILD_READ         socket_pipe.m_pipe_dn[0]
#define PIPE_PARENT_WRITE       socket_pipe.m_pipe_dn[1]

bool SocketPipe_Init() {
    socket_pipe.m_pipe_er[0] = -1;
    socket_pipe.m_pipe_er[1] = -1;
    socket_pipe.m_pipe_up[0] = -1;
    socket_pipe.m_pipe_up[1] = -1;
    socket_pipe.m_pipe_dn[0] = -1;
    socket_pipe.m_pipe_dn[1] = -1;
}

void SocketPipe_Exit() {
    if (socket_pipe.m_pipe_er[0] > 0) close(socket_pipe.m_pipe_er[0]);
    if (socket_pipe.m_pipe_er[1] > 0) close(socket_pipe.m_pipe_er[1]);
    if (socket_pipe.m_pipe_up[0] > 0) close(socket_pipe.m_pipe_up[0]);
    if (socket_pipe.m_pipe_up[1] > 0) close(socket_pipe.m_pipe_up[1]);
    if (socket_pipe.m_pipe_dn[0] > 0) close(socket_pipe.m_pipe_dn[0]);
    if (socket_pipe.m_pipe_dn[1] > 0) close(socket_pipe.m_pipe_dn[1]);
}

void SocketPipe_CreatePipe() {
    if (socket_pipe.m_pipe_er[0] > 0) close(socket_pipe.m_pipe_er[0]);
    if (socket_pipe.m_pipe_er[1] > 0) close(socket_pipe.m_pipe_er[1]);
    if (socket_pipe.m_pipe_up[0] > 0) close(socket_pipe.m_pipe_up[0]);
    if (socket_pipe.m_pipe_up[1] > 0) close(socket_pipe.m_pipe_up[1]);
    if (socket_pipe.m_pipe_dn[0] > 0) close(socket_pipe.m_pipe_dn[0]);
    if (socket_pipe.m_pipe_dn[1] > 0) close(socket_pipe.m_pipe_dn[1]);
    pipe(socket_pipe.m_pipe_er);
    pipe(socket_pipe.m_pipe_up);
    pipe(socket_pipe.m_pipe_dn);
}

int* SocketPipe_GetRecvPipe() {
    return &PIPE_PARENT_WRITE;
} //reading end of the recv pipe

int* SocketPipe_GetSendPipe() {
    return &PIPE_PARENT_READ;
} //writing end of the send pipe

bool SocketPipe_RedirectStdIO() {
    if (PIPE_CHILD_WRITE > 1 && PIPE_CHILD_READ > 1) {
        close(PIPE_PARENT_ERR);
        close(PIPE_PARENT_READ);
        close(PIPE_PARENT_WRITE);
        dup2(PIPE_CHILD_ERR, STDERR_FILENO);
        dup2(PIPE_CHILD_READ, STDIN_FILENO);
        dup2(PIPE_CHILD_WRITE, STDOUT_FILENO);
        close(PIPE_CHILD_READ);
        close(PIPE_CHILD_WRITE);
        close(PIPE_CHILD_ERR);
        return true;
    }
    return false;
}

int SocketPipe_TryRead(int fd, BYTE* buff, int nMax) {
    if (fd >= 0 && NULL != buff && nMax > 0) {
        fd_set rdfd;
        struct timeval tv;

        FD_ZERO(&rdfd);
        FD_SET(fd, &rdfd);
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        int ret = select(fd + 1, &rdfd, NULL, NULL, &tv);
        if (ret > 0) {
            if (FD_ISSET(fd, &rdfd))
                return read(fd, buff, nMax);
        } else return 0;
    } else return 0;
}

/////////////////////////////////////////////////////////////////////////////////

static int SocketConn_Default_OnSend(SOCKET_BASE* sock_base,BYTE* buf_send) {
    int nSend = MAX_BUFF_SIZE;
    if ((nSend = SocketPipe_TryRead(PIPE_PARENT_READ, buf_send, nSend)) > 0) return nSend;
    else nSend = SocketPipe_TryRead(PIPE_PARENT_ERR, buf_send, MAX_BUFF_SIZE);
    return nSend;
}

static void SocketConn_Default_OnRecv(SOCKET_BASE* sock_base,BYTE* buf_recv, int nRecv) {
    if (nRecv > 0 && buf_recv && PIPE_PARENT_WRITE > 1)
        write(PIPE_PARENT_WRITE, buf_recv, nRecv);
}
///////////////////////////////////////////////////////////////////////////////

bool SocketConn_Init(SOCKET_CONN* socket_conn, short PortNo,
        PIPE_CALLBACK_ON_SEND pOnSend, PIPE_CALLBACK_ON_RECV pOnRecv) {
    Socket_Init(&socket_conn->sock_base, PortNo);
    memset(socket_conn->m_multicastIP, 0x00, 16);
    socket_conn->m_receiveThread = INVALID_THREAD;
    pthread_cond_init(&socket_conn->m_cond, NULL);
    pthread_mutex_init(&socket_conn->m_lock, NULL);
    pthread_mutex_init(&socket_conn->m_receiveEvent, NULL);
    socket_conn->m_pOnSend = (NULL == pOnSend) ? SocketConn_Default_OnSend : pOnSend;
    socket_conn->m_pOnRecv = (NULL == pOnRecv) ? SocketConn_Default_OnRecv : pOnRecv;
    return true;
}

void SocketConn_DisConnect(SOCKET_CONN* socket_conn) {
    Socket_Disconnect(&socket_conn->sock_base);
    if (socket_conn->m_receiveThread != INVALID_THREAD) {
        pthread_mutex_lock(&socket_conn->m_receiveEvent);
        pthread_join(socket_conn->m_receiveThread, NULL);
        pthread_mutex_unlock(&socket_conn->m_receiveEvent);
        socket_conn->m_receiveThread = INVALID_THREAD;
    }
}

void SocketConn_Exit(SOCKET_CONN* socket_conn) {
    pthread_mutex_destroy(&socket_conn->m_receiveEvent);
    pthread_mutex_destroy(&socket_conn->m_lock);
    pthread_cond_destroy(&socket_conn->m_cond);
    SocketConn_DisConnect(socket_conn);
}

static bool SocketConn_ReceiveState(SOCKET_CONN* pConn) {
    if (pthread_mutex_trylock(&pConn->m_receiveEvent) == 0) {
        pthread_mutex_unlock(&pConn->m_receiveEvent);
        return true;
    }
    return false;
}

static void SocketConn_ReceivePolling(SOCKET_CONN* pConn, BYTE cMode) {
    int nRecv = 0;
    int nSend = 0;
    bool stat = false;
    bool conn = false;
    char sMode[4][10] = { "Unknown", "TCP Server", "TCP Client", "UDP" };

    Socket_TRACE(DEBUG_CONN_GREEN, "\tSocket Start Receiving, %s:%d\n",
                                sMode[cMode], pConn->sock_base.m_PortNo);
    while ((stat = SocketConn_ReceiveState(pConn)) && (conn = Socket_IsConnected(&pConn->sock_base))) {
        switch (cMode) {
            case MODE_TCPSERVER:
                SocketConn_TCP_Recv(pConn, MAX_BUFF_SIZE); //The handling routine is in another thread
                break;
            case MODE_TCPCLIENT:
            {//Both the recieving and handling routine are in the same thread
                BYTE buf_send[MAX_BUFF_SIZE];
                if (pConn->m_pOnSend && (nSend = pConn->m_pOnSend(&pConn->sock_base,buf_send)) > 0)
                    Socket_SendData(&pConn->sock_base, (char*) buf_send, nSend);
                if (pConn->m_pOnRecv && (nRecv = SocketConn_TCP_Recv(pConn, MAX_BUFF_SIZE)) > 0)
                    pConn->m_pOnRecv(&pConn->sock_base,pConn->sock_base.m_buffer, nRecv);
            }
                break;
            case MODE_UDPIP: //Both the recieving and handling routine are in the same thread
                nRecv = Socket_RecvFromN(&pConn->sock_base, &pConn->sock_base.m_dwRemoteAdd,
                        pConn->sock_base.m_wRemotePort, MAX_BUFF_SIZE);
                if (pConn->m_pOnRecv)
                    pConn->m_pOnRecv(&pConn->sock_base,pConn->sock_base.m_buffer, nRecv);
                break;
            default:;
        }
        usleep(50000);
    }
    Socket_TRACE(DEBUG_CONN_GREEN, "\tSocket Stop Receiving (state = %d, connected = %d)\n", stat, conn);
}

static void* SocketConn_ReceiveServer(LPVOID lp) {
    if (lp) {
        SOCKET_CONN* pConn = (SOCKET_CONN*) lp;

        SocketConn_ReceivePolling(pConn, MODE_TCPSERVER);
    }
    return 0;
}

static void* SocketConn_ReceiveClient(LPVOID lp) {
    if (lp) {
        SOCKET_CONN* pConn = (SOCKET_CONN*) lp;

        SocketConn_ReceivePolling(pConn, MODE_TCPCLIENT);
    }
    return 0;
}

static void* SocketConn_ReceiveUdpip(LPVOID lp) {
    if (lp) {
        SOCKET_CONN* pConn = (SOCKET_CONN*) lp;

        SocketConn_ReceivePolling(pConn, MODE_UDPIP);
    }
    return 0;
}

bool SocketConn_Connect(SOCKET_CONN* socket_conn, int b1, int b2, int b3, int b4, short port) {
    if (Socket_Connect(&socket_conn->sock_base, b1, b2, b3, b4, socket_conn->sock_base.m_PortNo = port)) {
        socket_conn->sock_base.m_dwRemoteAdd = (0xff000000 & (b1 << 24)) |
                (0x00ff0000 & (b2 << 16)) |
                (0x0000ff00 & (b3 << 8)) |
                (0x000000ff & b4);
        if (pthread_create(&socket_conn->m_receiveThread, NULL, SocketConn_ReceiveClient, socket_conn) != 0) {
            Socket_TRACE(DEBUG_CONN_BLACK, "Failed on creating receiving thread,error=%s\n", strerror(errno));
            socket_conn->m_receiveThread = INVALID_THREAD;
            Socket_Disconnect(&socket_conn->sock_base);
            return false;
        }
        return true;
    } else {
        socket_conn->sock_base.m_dwRemoteAdd = 0;
        return false;
    }
}

bool SocketConn_TCP_Bind(SOCKET_BASE* listener) { //For TCPIP
    return Socket_Bind(listener, SOCK_STREAM, listener->m_PortNo, NULL);
}

bool SocketConn_UDP_Bind(SOCKET_CONN* socket_conn, LPSTR multicastIP, short port) { //For UDPIP
    char* mcast_ip = multicastIP;
    if (port > 0) socket_conn->sock_base.m_PortNo = port;
    if (multicastIP) {
        memcpy(socket_conn->m_multicastIP, multicastIP, 15);
        mcast_ip = socket_conn->m_multicastIP;
    }
    if (Socket_Bind(&socket_conn->sock_base, SOCK_DGRAM, socket_conn->sock_base.m_PortNo, mcast_ip)) {
        if (socket_conn->sock_base.m_PortNo == 0) return true;  //do not receive packets when port == 0
        if (pthread_create(&socket_conn->m_receiveThread, NULL, SocketConn_ReceiveUdpip, socket_conn) != 0) {
            Socket_TRACE(DEBUG_CONN_BLACK, "Socket UDP Binding Failed,error=%s\n", strerror(errno));
            socket_conn->m_receiveThread = INVALID_THREAD;
            Socket_Disconnect(&socket_conn->sock_base);
            return false;
        }
        return true;
    }
    return false;
}

DWORD SocketConn_Accept(SOCKET_CONN* socket_conn, SOCKET_BASE* listener) {
    socket_conn->sock_base.m_dwRemoteAdd = Socket_Accept(&socket_conn->sock_base, listener->m_socket);
    if (socket_conn->sock_base.m_socket == INVALID_SOCKET) {
        int err_no = errno;
        socket_conn->sock_base.m_dwRemoteAdd = 0;
        if (err_no == WSAEWOULDBLOCK) {
            Socket_TRACE(0, "The socket is marked as nonblocking and no connections are present to be accepted.\n");
        } else {
            Socket_TRACE(DEBUG_CONN_BLACK, "Socket (listener = %d) Accept Failed,error=%d\n", listener->m_socket, err_no);
        }
        return err_no;
    }
    if (pthread_create(&socket_conn->m_receiveThread, NULL, SocketConn_ReceiveServer, socket_conn) != 0) {
        Socket_TRACE(DEBUG_CONN_BLACK, "Failed on creating receiving thread,error=%s\n", strerror(errno));
        socket_conn->m_receiveThread = INVALID_THREAD;
        Socket_Disconnect(&socket_conn->sock_base);
        return errno;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//	for UDP
///////////////////////////////////////////////////////////////////////////////
int SocketConn_UDP_SendS(SOCKET_CONN* socket_conn, LPSTR ip, WORD port, LPSTR buffer, DWORD size) {
    return Socket_SendTo(&socket_conn->sock_base, ip, port, buffer, size);
}

int SocketConn_UDP_SendN(SOCKET_CONN* socket_conn, DWORD ip, WORD port, LPSTR buffer, DWORD size) {
    return SocketConn_UDP_SendS(socket_conn, IPconvertN2A(ip), port, buffer, size);
}

int SocketConn_Multicast(SOCKET_CONN* socket_conn, WORD port, LPSTR buffer, DWORD size) {
    if (strlen(socket_conn->m_multicastIP) > 0)
        return SocketConn_UDP_SendS(socket_conn, socket_conn->m_multicastIP, port, buffer, size);
    return 0;
}

int SocketConn_UDP_RecvN(SOCKET_CONN* socket_conn, DWORD* ip, WORD port, DWORD size) {
    return Socket_RecvFromN(&socket_conn->sock_base, ip, port, size);
}

LPSTR SocketConn_UDP_RecvS(SOCKET_CONN* socket_conn, LPSTR ip, WORD port, DWORD* size) {
    return Socket_RecvFromS(&socket_conn->sock_base, ip, port, size);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//	for TCP
///////////////////////////////////////////////////////////////////////////////

int SocketConn_TCP_Send(SOCKET_CONN* socket_conn, char* buffer, DWORD size) {
    return Socket_SendData(&socket_conn->sock_base, buffer, size);
}

int SocketConn_TCP_Recv(SOCKET_CONN* socket_conn, DWORD size) {
    int nRecv = 0;
    pthread_mutex_lock(&socket_conn->m_lock);
    while (socket_conn->sock_base.m_nRecv > 0) {//if m_nRec > 0,means the data received from network last time still not been taken away yet
        pthread_cond_wait(&socket_conn->m_cond, &socket_conn->m_lock);
    }
    nRecv = Socket_RecvData(&socket_conn->sock_base, size);
    pthread_mutex_unlock(&socket_conn->m_lock);
    return nRecv;
}

bool SocketConn_IsUpdate(SOCKET_CONN* socket_conn) {
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 50000000;
    if (pthread_mutex_timedlock(&socket_conn->m_lock, &timeout) == 0) {
        bool bUpdate = (socket_conn->sock_base.m_nRecv > 0) ? true : false;
        if (!bUpdate) pthread_mutex_unlock(&socket_conn->m_lock);
        return bUpdate;
    }
    return false;
}

void SocketConn_Release(SOCKET_CONN* socket_conn) {
    socket_conn->sock_base.m_nRecv = 0;
    pthread_cond_signal(&socket_conn->m_cond);
    pthread_mutex_unlock(&socket_conn->m_lock);
}
///////////////////////////////////////////////////////////////////////////////


