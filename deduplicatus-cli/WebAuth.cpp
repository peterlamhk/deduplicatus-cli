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
#include <map>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
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

WebAuth::WebAuth(Config *c) {
    // copy reference of Config and init curl handler
    WebAuth::c = c;

    initCurl();
}

WebAuth::~WebAuth() {
    curl_easy_cleanup(curl);
}

void WebAuth::initCurl() {
    // ensure the stream is empty before any curl request
    stream.str("");
    stream.clear();
    
    // easy init curl handler
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

    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    strcpy(query, "lock=");
    
    if( c->user_json && c->user_lock.length() > 0 ) {
        char * q_lock = curl_easy_escape(curl, c->user_lock.c_str(), (int) c->user_lock.length());
        strcat(query, q_lock);
        curl_free(q_lock);
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    curl_code = curl_easy_perform(curl);
    
    // free memory and structures
    free(query);

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
    isLock = v_lock.GetBool() || ( c->user_mode.compare(c->mode_file_manager) == 0 );
    c->user_email = v_email.GetString();
    
    // reset curl
    resetCurl();
}

int WebAuth::signin(char * email, char * password) {
    long curl_code = 0, http_code = 0;
    
    // init request status
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_signin).c_str());
    
    // build request query
    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    
    strcpy(query, "email=");
    char * q_email = curl_easy_escape(curl, email, 0);
    strcat(query, q_email);
    
    strcat(query, "&password=");
    char * q_password = curl_easy_escape(curl, password, sizeof(password));
    strcat(query, q_password);
    
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_code = curl_easy_perform(curl);
    
    // free memory and structures
    free(query);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    // reset curl
    resetCurl();
    
    if( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
        // success, continue to download leveldb files
        cout << "Account signed in." << endl;
        return downloadLevel();

    } else {
        // failed
        cerr << "Error: Wrong Credentials." << endl;
        return ERR_WRONG_CREDENTIALS;
    }
}

int WebAuth::downloadLevel() {
    long curl_code = 0, http_code = 0;
    
    // init request lock
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_lock).c_str());
    
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_code = curl_easy_perform(curl);

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

        int createDirResult = createDirectory(directory, true);
        if( createDirResult != ERR_NONE ) {
            return createDirResult;
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
        Level *db = new Level();
        db->open(directory);
        
        string level_userid = db->get("metafile::userid");
        string level_versionid = db->get("metafile::version");
        string level_mode = db->get("clouds::storageMode");

        if( userid.compare(level_userid) != 0 ||
           versionid.compare(level_versionid) != 0 ) {
            cerr << "Error: Incorrect leveldb downloaded." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        
        // close leveldb
        delete db;
        
        // prepare DOM for storing client state
        Document store;
        store.SetObject();
        Document::AllocatorType& allocator = store.GetAllocator();
        
        // set JSON document values
        {
            Value s_userid;
            Value s_version;
            Value s_lock;
            Value s_mode;
        
            s_userid.SetString(userid.c_str(), (unsigned) userid.length(), allocator);
            s_version.SetString(versionid.c_str(), (unsigned) versionid.length(), allocator);
            s_lock.SetString(lockid.c_str(), (unsigned) lockid.length(), allocator);
            s_mode.SetString(level_mode.c_str(), (unsigned) level_mode.length(), allocator);
            
            store.AddMember("userid", s_userid, allocator);
            store.AddMember("version", s_version, allocator);
            store.AddMember("lock", s_lock, allocator);
            store.AddMember("mode", s_mode, allocator);
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
        
        cout << "LevelDB downloaded." << endl;
        return ERR_NONE;
        
    } else {
        // failed
        if( http_code == 400 ) {
            cerr << "Error: LevelDB currently locked by another client." << endl;
            return ERR_LEVEL_LOCKED;
            
        } else if( http_code == 412 ) {
            cerr << "Error: Please finalize the cloud storage setting on the webside." << endl;
            return ERR_LEVEL_NOT_FINALIZED;
            
        } else {
            cerr << "Error: Can't lock levelDB because server error." << endl;
            return ERR_SERVER_ERROR;
        }
    }
}

int WebAuth::sync() {
    long curl_code = 0, http_code = 0;
    map<string, string> filesHash;
    
    // hash all leveldb files into string map
    DIR *dir;
    struct dirent *dirp;
    
    if( (dir = opendir(c->user_lock.c_str())) == NULL ) {
        cerr << "Error: Can't access levelDB files." << endl;
        return ERR_LOCAL_ERROR;
    }

    while( (dirp = readdir(dir)) != NULL ) {
        if( dirp->d_name[0] == '.' ) {
            // skip if it is a hidden file
            continue;
        }
        string path = c->user_lock + "/" + string(dirp->d_name);
        filesHash.insert(std::make_pair(string(dirp->d_name), sha1_file(path.c_str())));
    }
    closedir(dir);
    free(dirp);

    // prepare JSON request to /client/newVersion
    Document d;
    d.SetObject();
    Document::AllocatorType& allocator = d.GetAllocator();
    {
        // set "lock" variable
        Value s_lock;
        s_lock.SetString(c->user_lock.c_str(), (unsigned) c->user_lock.length(), allocator);
        d.AddMember("lock", s_lock, allocator);
        
        // set "files" variable
        Value files(kObjectType);
        for( map<string, string>::iterator it = filesHash.begin(); it != filesHash.end(); ++it ) {
            Value key(it->first.c_str(), allocator);
            Value val(it->second.c_str(), allocator);
            
            files.AddMember(key, val, allocator);
        }
        d.AddMember("files", files, allocator);
    }
    
    // stringify the JSON
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);
    string jsonString = buffer.GetString();
    
    // post first JSON request
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_newVersion).c_str());
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonString.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

    curl_code = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
        cerr << "Error: Can't declare new version. Code: " << http_code << endl;
        return ERR_SERVER_ERROR;
    }

    // parse the response, obtain the new version id and files required to upload
    Document r;
    r.Parse(stream.str().c_str());

    // reset curl
    resetCurl();

    Value& v_next_version = r["nextVersion"];
    Value& v_files = r["files"];
    
    string next_version = v_next_version.GetString();

    // upload files to /upload/:versionid/:filename
    for( Value::ConstValueIterator itr = v_files.Begin(); itr != v_files.End(); ++itr) {
        string filename = itr->GetString();
        string path = c->user_lock + "/" + string(basename((char *) filename.c_str()));
        string endpoint = c->web_front + c->path_upload + "/" + next_version + "/" + filename;
        
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;
        curl_formadd(&formpost,
                     &lastptr,
                     CURLFORM_COPYNAME, "upload",
                     CURLFORM_FILE, path.c_str(),
                     CURLFORM_END);
        curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        
        curl_code = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
            cerr << "Error: Can't upload file " << filename << ". Code: " << http_code << endl;
            return ERR_SERVER_ERROR;
        }
        
        // reset curl
        curl_formfree(formpost);
        resetCurl();
    }

    // add "versionid" into JSON
    {
        // set "lock" variable
        Value s_versionid;
        d.AddMember("versionid", v_next_version, allocator);
    }
    
    // stringify the JSON
    buffer.Clear();
    writer.Reset(buffer);
    d.Accept(writer);
    jsonString = buffer.GetString();
    
    // post second JSON request to /client/commit for commit new version
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_commit).c_str());

    headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonString.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    
    curl_code = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
        cerr << "Error: Can't commit new version. Code: " << http_code << endl;
        return ERR_SERVER_ERROR;
    }
    
    // reset curl
    resetCurl();
    
    // if done, update the local leveldb for metafile::version => new version
    Level *db = new Level();
    db->open(c->user_lock);
    db->put("metafile::version", next_version);
    delete db;
    
    cout << "LevelDB synced." << endl;
    return ERR_NONE;
}

int WebAuth::unlock() {
    long curl_code = 0, http_code = 0;
    
    // init request unlock
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_unlock).c_str());
    
    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    strcpy(query, "lock=");
    
    if( c->user_json && c->user_lock.length() > 0 ) {
        char * q_lock = curl_easy_escape(curl, c->user_lock.c_str(), (int) c->user_lock.length());
        strcat(query, q_lock);
        curl_free(q_lock);
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_code = curl_easy_perform(curl);

    // free memory and structures
    free(query);
    resetCurl();
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
        cerr << "Error: Can't unlock levelDB on server. Code: " << http_code << endl;
        return ERR_SERVER_ERROR;
    }

    cout << "LevelDB unlocked." << endl;
    return ERR_NONE;
}

int WebAuth::signout(bool removeDB) {
    // init request unlock
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_signout).c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "/tmp/deduplicatus.jar");
    curl_easy_perform(curl);

    // remove cookie and client state files
    remove(c->client_cookie.c_str());
    remove(c->client_data.c_str());
    cout << "Account signed out." << endl;

    // remove leveldb directory
    if( removeDB ) {
        removeDirectory(c->user_lock);
    }

    // remove cache directory if it exists
    removeDirectory(c->user_lock + "-cache");

    return ERR_NONE;
}

// This function should be invoked ONCE ONLY when a cloud api returning 4xx status.
// Server-side init the request and new access tokens will be obtained.
bool WebAuth::refreshToken(Level *db, string cloudid) {
    long curl_code = 0, http_code = 0;
    
    // reset curl in case any unwanted string in stream
    resetCurl();
    
    // init request refreshToken
    curl_easy_setopt(curl, CURLOPT_URL, (c->web_front + c->path_refreshToken).c_str());
    
    // build request query
    char * query = (char *) malloc(sizeof(char) * MAX_LEN_QUERY);
    
    strcpy(query, "lock=");
    char * q_lock = curl_easy_escape(curl, c->user_lock.c_str(), 0);
    strcat(query, q_lock);
    
    strcat(query, "&cloud=");
    char * q_cloud = curl_easy_escape(curl, cloudid.c_str(), 0);
    strcat(query, q_cloud);
    
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
    curl_code = curl_easy_perform(curl);
    
    // free memory and structures
    free(query);
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if( !( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) ) {
        cerr << "Error: Can't refresh access token for cloud storage." << endl;
        return false;
    }
    
    // parse response to store new access and refresh tokens
    Document d;
    d.Parse(stream.str().c_str());

    Value& v_accessToken = d["accessToken"];
    Value& v_refreshToken = d["refreshToken"];

    db->put("clouds::account::" + cloudid + "::accessToken", v_accessToken.GetString());
    db->put("clouds::account::" + cloudid + "::refreshToken", v_refreshToken.GetString());
    
    // reset curl
    resetCurl();
    
    return true;
}
