#ifndef SOCKETBUFFER_H
#define SOCKETBUFFER_H

#include <stdio.h>
#include <string.h>

class SocketBuffer
{
    public:
        SocketBuffer( int intBufferSize ) {
            intSize = intBufferSize;
            data = new unsigned char[intSize];
        }
        ~SocketBuffer() {
            if( data ) {
                delete[] data;
            }
        }
        void enlarge();

        unsigned char *data = NULL;
        int intSize = 0;
        int intLen = 0;
        int intExpectLen = 0;
        int intSentLen = 0;
};

#endif
