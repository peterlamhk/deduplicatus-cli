//
//  WebAuth.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "WebAuth.h"
#include "Config.h"
#include "Level.h"
#include "define.h"
#include "tool.h"

using namespace std;
using namespace rapidjson;
ostringstream stream;

size_t write_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

WebAuth::WebAuth(Config *c) {
    // copy reference of Config and init curl handler
    WebAuth::c = c;

    initCurl();
}

WebAuth::~WebAuth() {
    curl_easy_cleanup(curl);
}

void WebAuth::initCurl() {
    curl = curl_easy_init();
    
    if( file_exists(c->client_cookie) ) {
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, c->client_cookie.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, c->client_cookie.c_str());
}

void WebAuth::resetCurl() {
    stream.str("");
    stream.clear();
    
    curl_easy_reset(curl);

    // fix curl_easy_reset reset CURLOPT_COOKIEJAR
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, c->client_cookie.c_str());
}

void WebAuth::showStatus() {
    // if cant read user's data
    if( !c->user_json ) {
        cerr << "Error: No user data found." << endl;
        return;
    }
    
    // request to server for status
    getStatus();
    
    if( isAuth && isLock ) {
        cout << "Signed in as user " << c->user_email << ", and LevelDB is locked to this client." << endl;
    } else {
        cout << "User is not signed in nor locked the LevelDB." << endl;
    }
}

void WebAuth::getStatus() {
    long curl_code = 0;

    // init request status
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_status).c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    
    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    strcpy(query, "lock=");
    
    if( c->user_json && c->user_lock.length() > 0 ) {
        char * q_lock = curl_easy_escape(curl, c->user_lock.c_str(), (int) c->user_lock.length());
        strcat(query, q_lock);
        curl_free(q_lock);
    }

    struct curl_slist *headers = NULL;
    set_header_postform(headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    curl_code = curl_easy_perform(curl);
    
    // free memory and structures
    free(query);
    curl_slist_free_all(headers);
    
    if( curl_code != CURLE_OK ) {
        cerr << "Error: Can't reach server." << endl;
        exit(ERR_SERVER_ERROR);
    }

    // parse JSON
    Document d;
    d.Parse(stream.str().c_str());
    
    Value& v_auth = d["auth"];
    Value& v_email = d["account"];
    Value& v_lock = d["lock"];
    
    // set variable to class
    isAuth = v_auth.GetBool();
    isLock = v_lock.GetBool();
    c->user_email = v_email.GetString();
    
    // reset curl
    resetCurl();
}

int WebAuth::signin(char * email, char * password) {
    long curl_code = 0, http_code = 0;
    
    // init request status
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_signin).c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    
    // build request query
    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    
    strcpy(query, "email=");
    char * q_email = curl_easy_escape(curl, email, 0);
    strcat(query, q_email);
    
    strcat(query, "&password=");
    char * q_password = curl_easy_escape(curl, password, sizeof(password));
    strcat(query, q_password);
    
    struct curl_slist *headers = NULL;
    set_header_postform(headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_code = curl_easy_perform(curl);
    
    // free memory and structures
    free(query);
    curl_slist_free_all(headers);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // reset curl
    resetCurl();
    
    if( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
        // success, continue to download leveldb files
        return downloadLevel();

    } else {
        // failed
        cerr << "Error: Wrong Credentials." << endl;
        return ERR_WRONG_CREDENTIALS;
    }
}

int WebAuth::signout() {
    
    return ERR_NONE;
}

int WebAuth::downloadLevel() {
    long curl_code = 0, http_code = 0;
    
    // init request lock
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_lock).c_str());
    
    struct curl_slist *headers = NULL;
    set_header_postform(headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_code = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
        // success, parse response JSON to download all leveldb files
        Document d;
        d.Parse(stream.str().c_str());
        
        Value& v_userid = d["userid"];
        Value& v_lockid = d["lockid"];
        Value& v_versionid = d["versionid"];
        Value& v_files = d["files"];

        string userid = v_userid.GetString();
        string versionid = v_versionid.GetString();
        string lockid = v_lockid.GetString();
        string directory = lockid;
        
        // create user's directory using lockid as name, error if exists
        DIR* dir = opendir(directory.c_str());
        if( dir ) {
            // directory exists
            cerr << "Error: Directory in use." << endl;
            closedir(dir);
            return ERR_LOCAL_ERROR;
            
        } else if( ENOENT == errno ) {
            if( mkdir(directory.c_str(), S_IRWXU) != 0 ) {
                cerr << "Error: Can't create directory." << endl;
                return ERR_LOCAL_ERROR;
            }

        } else {
            cerr << "Error: Local file permission denied." << endl;
            return ERR_LOCAL_ERROR;
        }
        
        // destroy curl
        resetCurl();
        
        // download leveldb files into directory
        for( Value::ConstMemberIterator itr = v_files.MemberBegin();
            itr != v_files.MemberEnd(); ++itr ) {

            string filename = itr->name.GetString();
            string checksum = itr->value.GetString();
            string targetFilename = directory + "/" + filename;
            bool success = false;
            int tries = 1;
            
            do {
                FILE *fp = fopen(targetFilename.c_str(), "wb");

                curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_download + "/" + versionid + "/" + filename).c_str());
                curl_easy_setopt(curl, CURLOPT_POST, 0);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_NETWORK_TIMEOUT);
                curl_easy_perform(curl);
            
                // reset enviornment
                resetCurl();
                fclose(fp);
                
                // check sha1 for current file
                string sha1 = sha1_file(targetFilename.c_str());
                if( checksum.compare(sha1) == 0 ) {
                    success = true;
                } else {
                    cerr << "Warning: downloaded file checksum incorrect, retry." << endl;
                }
                
                tries++;
            } while( !success && tries < MAX_NETWORK_TRIES );

            if( !success ) {
                cerr << "Error: Failed to download leveldb files." << endl;
                return ERR_NETWORK_ERROR;
            }
        }
        
        // open leveldb to check userid and versionid
        Level db;
        db.open(directory);
        
        string level_userid = db.get("metafile::userid");
        string level_versionid = db.get("metafile::version");

        if( userid.compare(level_userid) != 0 ||
           versionid.compare(level_versionid) != 0 ) {
            cerr << "Error: Incorrect leveldb downloaded." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        
        // prepare DOM for storing client state
        Document store;
        store.SetObject();
        Document::AllocatorType& allocator = store.GetAllocator();
        
        // set JSON document values
        {
            Value s_userid;
            Value s_version;
            Value s_lock;
        
            s_userid.SetString(userid.c_str(), (unsigned) userid.length(), allocator);
            s_version.SetString(versionid.c_str(), (unsigned) versionid.length(), allocator);
            s_lock.SetString(lockid.c_str(), (unsigned) lockid.length(), allocator);
            
            store.AddMember("userid", s_userid, allocator);
            store.AddMember("version", s_version, allocator);
            store.AddMember("lock", s_lock, allocator);
        }
        
        // stringify the DOM
        StringBuffer buffer;
        Writer<StringBuffer> writer(buffer);
        store.Accept(writer);
        string jsonString = buffer.GetString();
        
        // write to file
        FILE * jsonFile = fopen(c->client_data.c_str(), "w");
        fwrite(jsonString.c_str(), sizeof(char), jsonString.length(), jsonFile);
        fclose(jsonFile);
        
        cout << "Success." << endl;
        return ERR_NONE;
        
    } else {
        // failed
        if( http_code == 400 ) {
            cerr << "Error: LevelDB currently locked by another client." << endl;
            return ERR_LEVEL_LOCKED;
            
        } else {
            cerr << "Error: Can't lock levelDB because server error." << endl;
            return ERR_SERVER_ERROR;
        }
    }
}