#ifndef OCRSERVER_H
#define OCRSERVER_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <ev.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/pem.h>
#include <map>

#include "GLog.h"
#include "YamlConf.h"
#include "SocketConnection.h"

typedef std::map<int, SocketConnection*> connectionMap;
typedef std::pair<int, SocketConnection*> connectionPair;

class OcrServer
{
    private:
        OcrServer()
        {
            config = new YamlConf( "conf/ocr_server.yaml" );
            intListenPort = config->getInt( "listen" );
            listenWatcher = new ev_io();
        }
        ~OcrServer()
        {
            if( listenWatcher )
            {
                ev_io_stop(pMainLoop, listenWatcher);
                delete listenWatcher;
            }

            if( intListenFd )
            {
                close( intListenFd );
            }
        }
        static OcrServer *pInstance;

        YamlConf *config = NULL;
        pthread_t intThreadId = 0;
        struct ev_loop *pMainLoop = EV_DEFAULT;
        int intListenPort = 0;
        int intListenFd = 0;
        ev_io *listenWatcher = NULL;
        connectionMap mapConnection;
    public:
        static OcrServer *getInstance();
        void run();
        int start();
        int join();
        void acceptCB();
        void readCB( int intFd );
        void writeCB( int intFd );
        void readTimeoutCB( int intFd );
        void writeTimeoutCB( int intFd );
        void recvHandshake( SocketConnection *pConnection );
        void recvQuery( SocketConnection *pConnection );
        void parseQuery( SocketConnection *pConnection );
        void ackHandshake( SocketConnection *pConnection );
        void ackQuery( SocketConnection *pConnection );
        void closeConnection( SocketConnection *pConnection );
};

#endif
