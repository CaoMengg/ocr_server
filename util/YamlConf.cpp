#include "YamlConf.h"

int YamlConf::getInt( const char *key )
{
    if( config[ key ] ) {
        return config[ key ].as<int>();
    }
    return -1;
}
