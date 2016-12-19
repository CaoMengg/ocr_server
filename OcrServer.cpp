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

void OcrServer::ackQuery( SocketConnection *pConnection )
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
                LOG(INFO) << "ack query succ, fd=" << pConnection->intFd;
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
        ackQuery( pConnection );
    }
}

static void writeCallback( EV_P_ ev_io *watcher, int revents )
{
    (void)loop;
    (void)revents;
    OcrServer::getInstance()->writeCB( watcher->fd );
}

void OcrServer::recvHandshake( SocketConnection *pConnection )
{
    int n = recv( pConnection->intFd, pConnection->inBuf->data + pConnection->inBuf->intLen, pConnection->inBuf->intSize - pConnection->inBuf->intLen, 0 );
    if( n > 0 )
    {
        pConnection->inBuf->intLen += n;
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

void OcrServer::parseQuery( SocketConnection *pConnection )
{
    SocketBuffer* outBuf;
    outBuf = new SocketBuffer( pConnection->inBuf->intLen );
    outBuf->intLen = 4;
    outBuf->data[0] = 'a';
    outBuf->data[1] = 'b';
    outBuf->data[2] = 'c';
    outBuf->data[3] = '\n';

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

void OcrServer::recvQuery( SocketConnection *pConnection )
{
    int n = recv( pConnection->intFd, pConnection->inBuf->data + pConnection->inBuf->intLen, pConnection->inBuf->intSize - pConnection->inBuf->intLen, 0 );
    if( n > 0 )
    {
        pConnection->inBuf->intLen += n;
        if( pConnection->inBuf->data[pConnection->inBuf->intLen-1] == '\n' )
        {
            LOG(INFO) << "recv query, fd=" << pConnection->intFd;
            ev_timer_stop( pMainLoop, pConnection->readTimer );
            parseQuery( pConnection );
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
        recvQuery( pConnection );
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
    pConnection->status = csConnected;
    mapConnection[ acceptFd ] = pConnection;

    ev_io_init( pConnection->readWatcher, readCallback, acceptFd, EV_READ );
    ev_io_start( pMainLoop, pConnection->readWatcher );

    ev_init( pConnection->readTimer, readTimeoutCallback );
    ev_timer_set( pConnection->readTimer, pConnection->readTimeout, 0.0 );
    ev_timer_start( pMainLoop, pConnection->readTimer );

    ev_io_init( pConnection->writeWatcher, writeCallback, acceptFd, EV_WRITE );
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
