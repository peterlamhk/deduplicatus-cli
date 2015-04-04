//
//  Config.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <string>
#include <fstream>
#include "rapidjson/document.h"
#include "Config.h"
#include "tool.h"

using namespace std;
using namespace rapidjson;

Config::Config() {
    user_json = false;
    
    // set default values for system-specific variables
    client_cookie = "client.jar";
    client_data = "client.json";

//    web_front = "https://deduplicatus.retepmal.com";
    web_front = "http://localhost";
    path_status = "/client/status";
    path_signin = "/client/signin";
    path_signout = "/client/signout";
    path_lock = "/client/lock";
    path_unlock = "/client/unlock";
    path_newVersion = "/client/newVersion";
    path_download = "/client/download";
    path_upload = "/client/upload";
    path_commit = "/client/commit";
    path_refreshToken = "/client/refreshToken";
    
    // load JSON file to read user's data (if any)
    const char * filename = client_data.c_str();
    FILE * jsonFile = fopen(filename, "r");
    long jsonSize;
    char * userdata;
    
    if( jsonFile != NULL ) {
        // obtain file size
        fseek(jsonFile, 0, SEEK_END);
        jsonSize = std::ftell(jsonFile);
        rewind(jsonFile);
        
        // allocate char array to contain the whole file
        userdata = (char *) malloc(sizeof(char) * jsonSize);

        // copy the file into array
        fread(userdata, 1, jsonSize, jsonFile);
        
        // parse JSON
        Document d;
        d.Parse(userdata);
        free(userdata);
        
        Value& v_uid = d["userid"];
        Value& v_version = d["version"];
        Value& v_lock = d["lock"];
        
        // set as variables in this class
        user_id = v_uid.GetString();
        user_version = v_version.GetString();
        user_lock = v_lock.GetString();
        user_email = "";
        user_json = true;
    }
    
    // close file handler
    fclose(jsonFile);
};
