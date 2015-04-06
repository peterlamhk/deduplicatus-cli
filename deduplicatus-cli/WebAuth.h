//
//  WebAuth.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__WebAuth__
#define __deduplicatus_cli__WebAuth__

#include <stdio.h>
#include <curl/curl.h>
#include "Config.h"
#include "Level.h"

class WebAuth {
public:
    WebAuth(Config *);
    ~WebAuth();
    
    bool isAuth;
    bool isLock;
    void getStatus();
    void showStatus();
    int signin(char *, char *);
    int signout(bool);
    int sync();
    int unlock();
    bool refreshToken(Level *, string);

private:
    CURL *curl;
    Config *c;
    
    void initCurl();
    void resetCurl();
    int downloadLevel();
};

#endif /* defined(__deduplicatus_cli__WebAuth__) */
