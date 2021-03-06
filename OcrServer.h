#ifndef OCRSERVER_H
#define OCRSERVER_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ev.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

#include "curl/curl.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "GLog.h"
#include "YamlConf.h"
#include "SocketConnection.h"
#include "ProcessManager.h"

typedef std::map<int, SocketConnection*> connectionMap;
typedef std::pair<int, SocketConnection*> connectionPair;

typedef std::map<int, Process*> processMap;
typedef std::pair<int, Process*> processPair;

class OcrServer
{
    private:
        OcrServer()
        {
            config = new YamlConf( "conf/ocr_server.yaml" );
            intListenPort = config->getInt( "listen" );
            intWorkerProcesses = config->getInt( "worker_processes" );
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
        struct ev_loop *pMainLoop = EV_DEFAULT;
        int intListenPort = 0;
        int intWorkerProcesses = 1; //worker process num
        int intWorkerId = 0;        //0:master other:worker
        int intListenFd = 0;
        ev_io *listenWatcher = NULL;
        connectionMap mapConnection;
        processMap mapProcess;

        int workerReap = 0;
    public:
        static OcrServer *getInstance();
        void start();
        void signalHandlerCB( int intSigNo );
        void masterLoop();
        void workerLoop();
        void acceptCB();
        void readCB( int intFd );
        void writeCB( int intFd );
        void readTimeoutCB( int intFd );
        void writeTimeoutCB( int intFd );
        void recvQuery( SocketConnection *pConnection );
        void parseQuery( SocketConnection *pConnection );
        void ackQuery( SocketConnection *pConnection );
        void closeConnection( SocketConnection *pConnection );
};

#endif
