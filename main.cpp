#include "main.h"

int main()
{
    initGLog( "ocr_server" );
    OcrServer::getInstance()->start();
    OcrServer::getInstance()->join();
}
