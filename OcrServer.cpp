#include "OcrServer.h"

OcrServer* OcrServer::pInstance = NULL;

OcrServer* OcrServer::getInstance()
{
    if( pInstance == NULL )
    {
        pInstance = new OcrServer();
    }
    return pInstance;
}

void OcrServer::readTimeoutCB( int intFd )
{
    connectionMap::iterator it;
    it = mapConnection.find( intFd );
    if( it == mapConnection.end() )
    {
        return;
    }
    SocketConnection* pConnection = it->second;
    LOG(WARNING) << "read timeout, fd=" << intFd;
    delete pConnection;
}

static void readTimeoutCallback( EV_P_ ev_timer *timer, int revents )
{
    (void)loop;
    (void)revents;
    OcrServer::getInstance()->readTimeoutCB( ((SocketConnection *)timer->data)->intFd );
}

void OcrServer::writeTimeoutCB( int intFd )
{
    connectionMap::iterator it;
    it = mapConnection.find( intFd );
    if( it == mapConnection.end() )
    {
        return;
    }
    SocketConnection* pConnection = it->second;
    LOG(WARNING) << "write timeout, fd=" << intFd;
    delete pConnection;
}

static void writeTimeoutCallback( EV_P_ ev_timer *timer, int revents )
{
    (void)loop;
    (void)revents;
    OcrServer::getInstance()->writeTimeoutCB( ((SocketConnection *)timer->data)->intFd );
}

void OcrServer::ackHandshake( SocketConnection *pConnection )
{
    if( pConnection->outBufList.empty() )
    {
        return;
    }

    SocketBuffer *outBuf = pConnection->outBufList.front();
    if( outBuf->intSentLen < outBuf->intLen )
    {
        int n = send( pConnection->intFd, outBuf->data + outBuf->intSentLen, outBuf->intLen - outBuf->intSentLen, 0 );
        if( n > 0 )
        {
            outBuf->intSentLen += n;
        } else {
            if( errno==EAGAIN || errno==EWOULDBLOCK )
            {
                return;
            } else {
                delete pConnection;
                return;
            }
        }
    }

    if( outBuf->intSentLen >= outBuf->intLen )
    {
        LOG(INFO) << "handshake succ, fd=" << pConnection->intFd;

        pConnection->outBufList.pop_front();
        delete outBuf;

        pConnection->status = csConnected;
        ev_io_stop( pMainLoop, pConnection->writeWatcher );
        ev_timer_stop( pMainLoop, pConnection->writeTimer );
    }
}

void OcrServer::ackMessage( SocketConnection *pConnection )
{
    SocketBuffer *outBuf = NULL;
    while( ! pConnection->outBufList.empty() )
    {
        outBuf = pConnection->outBufList.front();
        if( outBuf->intSentLen < outBuf->intLen )
        {
            int n = send( pConnection->intFd, outBuf->data + outBuf->intSentLen, outBuf->intLen - outBuf->intSentLen, 0 );
            if( n > 0 )
            {
                outBuf->intSentLen += n;
            } else {
                if( errno==EAGAIN || errno==EWOULDBLOCK )
                {
                    return;
                } else {
                    delete pConnection;
                    return;
                }
            }
        }

        if( outBuf->intSentLen >= outBuf->intLen )
        {
            pConnection->outBufList.pop_front();
            delete outBuf;

            if( pConnection->status == csClosing )
            {
                LOG(INFO) << "close succ, fd=" << pConnection->intFd;
                delete pConnection;
                return;
            } else {
                LOG(INFO) << "ack message succ, fd=" << pConnection->intFd;
            }
        }
    }

    ev_io_stop( pMainLoop, pConnection->writeWatcher );
    ev_timer_stop( pMainLoop, pConnection->writeTimer );
}

void OcrServer::writeCB( int intFd )
{
    connectionMap::iterator it;
    it = mapConnection.find( intFd );
    if( it == mapConnection.end() )
    {
        return;
    }
    SocketConnection* pConnection = it->second;

    if( pConnection->status == csAccepted )
    {
        ackHandshake( pConnection );
    } else {
        ackMessage( pConnection );
    }
}

static void writeCallback( EV_P_ ev_io *watcher, int revents )
{
    (void)loop;
    (void)revents;
    OcrServer::getInstance()->writeCB( watcher->fd );
}

void OcrServer::parseHandshake( SocketConnection *pConnection )
{
    char *begin = strstr((char*)(pConnection->inBuf->data), "Sec-WebSocket-Key:");
    begin += 19;
    char *end = strchr(begin, '\r');
    int len = end - begin;

    char clientKey[50];
    memcpy( clientKey, begin, len );
    clientKey[len] = '\0';

    char joinedKey[100];
    sprintf( joinedKey, "%s%s", clientKey, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" );

    const char* base64Code = "abc";

    SocketBuffer* outBuf = new SocketBuffer( 200 );
    sprintf( (char*)(outBuf->data), "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n", base64Code );
    outBuf->intLen = strlen( (char*)(outBuf->data) );
    pConnection->outBufList.push_back( outBuf );

    pConnection->inBuf->intLen = 0;
    pConnection->inBuf->intExpectLen = 0;

    ev_io_init( pConnection->writeWatcher, writeCallback, pConnection->intFd, EV_WRITE );
    ev_io_start( pMainLoop, pConnection->writeWatcher );

    ev_timer_set( pConnection->writeTimer, pConnection->writeTimeout, 0.0 );
    ev_timer_start( pMainLoop, pConnection->writeTimer );
}

void OcrServer::recvHandshake( SocketConnection *pConnection )
{
    int n = recv( pConnection->intFd, pConnection->inBuf->data + pConnection->inBuf->intLen, pConnection->inBuf->intSize - pConnection->inBuf->intLen, 0 );
    if( n > 0 )
    {
        pConnection->inBuf->intLen += n;

        if( pConnection->inBuf->data[pConnection->inBuf->intLen-4]=='\r' && pConnection->inBuf->data[pConnection->inBuf->intLen-3]=='\n' &&
                pConnection->inBuf->data[pConnection->inBuf->intLen-2]=='\r' && pConnection->inBuf->data[pConnection->inBuf->intLen-1]=='\n' )
        {
            //接收到完整握手
            LOG(INFO) << "recv handshake, fd=" << pConnection->intFd;
            ev_timer_stop( pMainLoop, pConnection->readTimer );
            parseHandshake( pConnection );
        }
    } else if( n == 0 )
    {
        delete pConnection;
        return;
    } else {
        if( errno==EAGAIN || errno==EWOULDBLOCK )
        {
            return;
        } else {
            delete pConnection;
            return;
        }
    }
}

void OcrServer::parseMessage( SocketConnection *pConnection )
{
    SocketBuffer* outBuf;
    int intPayloadLen = pConnection->inBuf->data[1] & 0x7f;
    int intRealLen = 0;

    if( intPayloadLen == 126 )
    {
        intRealLen = int(pConnection->inBuf->data[2])*256 + int(pConnection->inBuf->data[3]);
        outBuf = new SocketBuffer( intRealLen + 4 );
        outBuf->intLen = intRealLen + 4;
        outBuf->data[0] = 0x81;
        outBuf->data[1] = intPayloadLen;
        outBuf->data[2] = pConnection->inBuf->data[2];
        outBuf->data[3] = pConnection->inBuf->data[3];

        int curIndex = 4;
        int i;
        for( i=8; i<8+intRealLen; ++i )
        {
            outBuf->data[ curIndex++ ] =  pConnection->inBuf->data[i] ^ pConnection->inBuf->data[(i-8)%4 + 4];
        }
    } else {
        intRealLen = intPayloadLen;
        outBuf = new SocketBuffer( intRealLen + 2 );
        outBuf->intLen = intRealLen + 2;
        outBuf->data[0] = 0x81;
        outBuf->data[1] = intPayloadLen;

        int curIndex = 2;
        int i;
        for( i=6; i<6+intRealLen; ++i )
        {
            outBuf->data[ curIndex++ ] =  pConnection->inBuf->data[i] ^ pConnection->inBuf->data[(i-6)%4 + 2];
        }
    }
    pConnection->outBufList.push_back( outBuf );

    pConnection->inBuf->intLen = 0;
    pConnection->inBuf->intExpectLen = 0;

    ev_io_start( pMainLoop, pConnection->writeWatcher );

    ev_timer_set( pConnection->writeTimer, pConnection->writeTimeout, 0.0 );
    ev_timer_start( pMainLoop, pConnection->writeTimer );
}

void OcrServer::closeConnection( SocketConnection *pConnection )
{
    SocketBuffer* outBuf = new SocketBuffer( 2 );
    outBuf->data[0] = 0x88;
    outBuf->data[1] = 0;
    outBuf->intLen = 2;
    pConnection->outBufList.push_back( outBuf );

    pConnection->status = csClosing;
    ev_io_start(pMainLoop, pConnection->writeWatcher);

    ev_timer_set( pConnection->writeTimer, pConnection->writeTimeout, 0.0 );
    ev_timer_start( pMainLoop, pConnection->writeTimer );
}

void OcrServer::recvMessage( SocketConnection *pConnection )
{
    int n = recv( pConnection->intFd, pConnection->inBuf->data + pConnection->inBuf->intLen, pConnection->inBuf->intSize - pConnection->inBuf->intLen, 0 );
    if( n > 0 )
    {
        if( pConnection->inBuf->intExpectLen == 0 )
        {
            if( (pConnection->inBuf->data[0] & 0x0f) == 0x01 && (pConnection->inBuf->data[1] & 0x80) == 0x80 )
            {
                //text frame
                int intPayloadLen = pConnection->inBuf->data[1] & 0x7f;
                if( intPayloadLen == 126 )
                {
                    intPayloadLen = (unsigned char)(pConnection->inBuf->data[3]) | (unsigned char)(pConnection->inBuf->data[2]) << 8;
                } else if( intPayloadLen == 127 )
                {
                    LOG(WARNING) << "unsupported payload len, close";
                    ev_io_stop(pMainLoop, pConnection->readWatcher);
                    closeConnection( pConnection );
                    return;
                }
                pConnection->inBuf->intExpectLen = intPayloadLen + 6;    //first 2 byte and 4 mask byte
            } else if( (pConnection->inBuf->data[0] & 0x0f) == 0x08 )
            {
                //close frame
                ev_io_stop(pMainLoop, pConnection->readWatcher);
                closeConnection( pConnection );
                return;
            } else {
                LOG(WARNING) << "unsupported message, close";
                ev_io_stop(pMainLoop, pConnection->readWatcher);
                closeConnection( pConnection );
                return;
            }

            //set timer
            ev_timer_set( pConnection->readTimer, pConnection->readTimeout, 0 );
            ev_timer_start( pMainLoop, pConnection->readTimer );
        }
        pConnection->inBuf->intLen += n;

        if( pConnection->inBuf->intLen >= pConnection->inBuf->intExpectLen )
        {
            //接收到完整frame
            LOG(INFO) << "recv message, fd=" << pConnection->intFd;
            ev_timer_stop( pMainLoop, pConnection->readTimer );
            parseMessage( pConnection );
        }
    } else if( n == 0 )
    {
        delete pConnection;
        return;
    } else {
        if( errno==EAGAIN || errno==EWOULDBLOCK )
        {
            return;
        } else {
            delete pConnection;
            return;
        }
    }
}

void OcrServer::readCB( int intFd )
{
    connectionMap::iterator it;
    it = mapConnection.find( intFd );
    if( it == mapConnection.end() )
    {
        return;
    }
    SocketConnection* pConnection = it->second;

    if( pConnection->inBuf->intLen >= pConnection->inBuf->intSize )
    {
        pConnection->inBuf->enlarge();
    }

    if( pConnection->status == csAccepted )
    {
        recvHandshake( pConnection );
    } else if( pConnection->status == csConnected )
    {
        recvMessage( pConnection );
    }
}

static void readCallback( EV_P_ ev_io *watcher, int revents )
{
    (void)loop;
    (void)revents;
    OcrServer::getInstance()->readCB( watcher->fd );
}

void OcrServer::acceptCB()
{
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int acceptFd = accept( intListenFd, (struct sockaddr*)&ss, &slen );
    if( acceptFd == -1 )
    {
        close( acceptFd );
        return;
    }
    int flag = fcntl(acceptFd, F_GETFL, 0);
    fcntl(acceptFd, F_SETFL, flag | O_NONBLOCK);
    LOG(INFO) << "accept fd=" << acceptFd;

    SocketConnection* pConnection = new SocketConnection();
    pConnection->pLoop = pMainLoop;
    pConnection->intFd = acceptFd;
    pConnection->status = csAccepted;
    mapConnection[ acceptFd ] = pConnection;

    ev_io_init( pConnection->readWatcher, readCallback, acceptFd, EV_READ );
    ev_io_start( pMainLoop, pConnection->readWatcher );

    ev_init( pConnection->readTimer, readTimeoutCallback );
    ev_timer_set( pConnection->readTimer, pConnection->readTimeout, 0.0 );
    ev_timer_start( pMainLoop, pConnection->readTimer );

    ev_init( pConnection->writeTimer, writeTimeoutCallback );
}

static void acceptCallback( EV_P_ ev_io *watcher, int revents )
{
    (void)loop;
    (void)watcher;
    (void)revents;
    OcrServer::getInstance()->acceptCB();
}

void OcrServer::run()
{
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons( intListenPort );

    intListenFd = socket(AF_INET, SOCK_STREAM, 0);
    int flag = 1;
    setsockopt(intListenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));
    flag = fcntl(intListenFd, F_GETFL, 0);
    fcntl(intListenFd, F_SETFL, flag | O_NONBLOCK);

    int intRet;
    intRet = bind( intListenFd, (struct sockaddr*)&sin, sizeof(sin) );
    if( intRet != 0 )
    {
        LOG(WARNING) << "bind fail";
        return;
    }

    intRet = listen( intListenFd, 255 );
    if( intRet != 0 )
    {
        LOG(WARNING) << "listen fail";
        return;
    }
    LOG(INFO) << "server start, listen succ port=" << intListenPort << " fd=" << intListenFd;

    ev_io_init( listenWatcher, acceptCallback, intListenFd, EV_READ );
    ev_io_start( pMainLoop, listenWatcher );
    ev_run( pMainLoop, 0 );
}

void *runOcrServer( void* )
{
    OcrServer::getInstance()->run();
    return NULL;
}

int OcrServer::start()
{
    pthread_attr_t attr;
    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );
    int intRet;
    intRet = pthread_create( &(intThreadId), &attr, runOcrServer, NULL );
    pthread_attr_destroy( &attr );

    if( intRet == 0 )
    {
        LOG(INFO) << "start thread succ";
        return 0;
    } else {
        LOG(WARNING) << "start thread fail";
        return 1;
    }
}

int OcrServer::join()
{
    void *pStatus;
    int intRet;
    intRet = pthread_join( intThreadId, &pStatus );

    if( intRet == 0 )
    {
        return 0;
    } else {
        return 1;
    }
}
