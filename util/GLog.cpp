#include "GLog.h"

void initGLog( const char *logName )
{
    google::InitGoogleLogging( logName );

    FLAGS_log_dir = "./log";
    FLAGS_logbufsecs = 0;       //实时输出
    FLAGS_max_log_size = 1000;  //最大1000M, 使用时间rolling时无效
    FLAGS_stop_logging_if_full_disk = true;

    std::string infoLogName = "./log/";
    infoLogName += logName;
    infoLogName += ".log.";
    std::string warningLogName = "./log/";
    warningLogName += logName;
    warningLogName += ".log.wf.";
    google::SetLogDestination( google::INFO, infoLogName.c_str() );
    google::SetLogDestination( google::WARNING, warningLogName.c_str() );
    //google::SetLogDestination( google::ERROR, warningLogName.c_str() );
}
