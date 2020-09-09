#ifndef _MASTER_H_
#define _MASTER_H_

#include "network.h"

class master_server: public basic_server {
public:
    master_server(hostaddr ip, hostport port);
};


#endif
