#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

class Process
{
    public:
        Process( int pId ) {
            intPId = pId;
            intStatus = 0;
        }

        int intPId;
        int intStatus;
};

class ProcessManager
{
    public:
        ProcessManager() {
        }
        ~ProcessManager() {
        }

        int intSize = 0;
};

#endif
