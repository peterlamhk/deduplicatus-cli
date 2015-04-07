//
//  Box.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 6/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <sstream>
#include <string>
#include <curl/curl.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "WebAuth.h"
#include "Box.h"
#include "tool.h"

using namespace std;
using namespace rapidjson;
ostringstream stream_box;

Box::Box(string token) {
    accessToken = token;
    
    // define cloud storage endpoints
    path_base = "https://api.box.com/2.0";
    path_account_info = "/users/me";
}

string Box::brandName() {
    return "Box";
}

void Box::accountInfo(Level *db, WebAuth *wa, string cloudid) {
    long curl_code = 0, http_code = 0;
    int refreshOAuth = 0;
    bool success = false;
    
    do {
        // init curl request
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, (path_base + path_account_info).c_str());
        
        // set oauth header
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_box);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        
        curl_code = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
        
        if( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
            success = true;
            
            // parse response JSON for information
            Document d;
            d.Parse(stream_box.str().c_str());
            
            Value& v_name = d["name"];
            Value& v_quota = d["space_amount"];
            Value& v_used = d["space_used"];
            
            displayName = v_name.GetString();
            space_quota = v_quota.GetUint64();
            space_used = v_used.GetUint64();
            
        } else {
            // non-200 response, try to refresh access token
            refreshOAuth++;
            
            if( refreshOAuth == 1 ) {
                wa->refreshToken(db, cloudid);
                accessToken = db->get("clouds::account::" + cloudid + "::accessToken");
            }
        }
    } while( !success && refreshOAuth == 1 );
}
