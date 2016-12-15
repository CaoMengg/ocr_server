#ifndef YAMLCONF_H
#define YAMLCONF_H

#include "yaml/include/yaml.h"

class YamlConf
{
    public:
        YamlConf( const char *fileName ) {
            config = YAML::LoadFile( fileName );
        }
        ~YamlConf() {
        }
        YAML::Node config;

        int getInt( const char *key );
};

#endif
