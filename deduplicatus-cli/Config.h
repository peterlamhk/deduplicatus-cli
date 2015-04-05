//
//  Config.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__Config__
#define __deduplicatus_cli__Config__

#include <stdio.h>
#include <string>

using namespace std;

class Config {
public:
    Config();

    // User's data stored in json file
    bool user_json;
    string user_id;
    string user_lock;
    string user_email;
    
    // Path of user's data in local
    string client_cookie;
    string client_data;
    
    // Endpoints of DeDuplicatus backend
    string web_front;
    string path_status;
    string path_signin;
    string path_signout;
    string path_lock;
    string path_unlock;
    string path_newVersion;
    string path_download;
    string path_upload;
    string path_commit;
    string path_refreshToken;
    
    // Endpoints of supported cloud storages

};

#endif /* defined(__deduplicatus_cli__Config__) */
