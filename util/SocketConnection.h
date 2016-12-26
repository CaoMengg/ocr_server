#ifndef SOCKETCONNECTION_H
#define SOCKETCONNECTION_H

#include <stdio.h>
#include <string>
#include <list>
#include <stdlib.h>
#include <unistd.h>
#include <ev.h>
#include "SocketBuffer.h"

typedef std::list<SocketBuffer *> bufferList;

enum enumConnectionStatus
{
    csInit,
    csAccepted,
    csConnected,
    csClosing,
};

class SocketConnection
{
    public:
        SocketConnection() {
            inBuf = new SocketBuffer( 1024 );
            picBuf = new SocketBuffer( 10240 );

            readWatcher = new ev_io();
            writeWatcher = new ev_io();
            readTimer = new ev_timer();
            readTimer->data = this;
            writeTimer = new ev_timer();
            writeTimer->data = this;
        }
        ~SocketConnection() {
            if( readWatcher && pLoop ) {
                ev_io_stop( pLoop, readWatcher );
                delete readWatcher;
            }
            if( writeWatcher && pLoop ) {
                ev_io_stop( pLoop, writeWatcher );
                delete writeWatcher;
            }

            if( readTimer && pLoop ) {
                ev_timer_stop( pLoop, readTimer );
                delete readTimer;
            }
            if( writeTimer && pLoop ) {
                ev_timer_stop( pLoop, writeTimer );
                delete writeTimer;
            }

            if( intFd ) {
                close( intFd );
            }

            if( inBuf ) {
                delete inBuf;
            }

            if( picBuf ) {
                delete picBuf;
            }

            while( ! outBufList.empty() ) {
                delete outBufList.front();
                outBufList.pop_front();
            }
        }
        struct ev_loop *pLoop = NULL;
        int intFd = 0;
        enumConnectionStatus status = csInit;
        ev_io *readWatcher = NULL;
        ev_io *writeWatcher = NULL;
        ev_timer *readTimer = NULL;
        ev_tstamp readTimeout = 3.0;
        ev_timer *writeTimer = NULL;
        ev_tstamp writeTimeout = 3.0;

        SocketBuffer *inBuf = NULL;
        SocketBuffer *picBuf = NULL;
        bufferList outBufList;
};

#endif
