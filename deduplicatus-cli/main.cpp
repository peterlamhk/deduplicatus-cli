//
//  main.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 30/3/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include <string>
#include <libgen.h>
#include "leveldb/db.h"
#include "Config.h"
#include "WebAuth.h"
#include "FileOperation.h"
#include "define.h"
#include "tool.h"
#include <CommonCrypto/CommonDigest.h>

using namespace std;

void showUsage(const char *);
int main(int, const char * []);

int main(int argc, const char * argv[]) {
    Config *c = new Config();
    WebAuth *wa = new WebAuth(c);
    bool operationFound = false;
    unsigned operationResult = ERR_NONE;
    
    // exec: status
    if( !operationFound && argc == 2 && strcmp(argv[1], "status") == 0 ) {
        wa->showStatus();
        
        operationFound = true;
        operationResult = ERR_NONE;
    }
    
    // exec: signin <email> <password>
    if( !operationFound && argc == 4 && strcmp(argv[1], "signin") == 0 ) {
        operationFound = true;
        wa->getStatus();

        // reject if current status is signed-in
        if( wa->isAuth ) {
            cout << "Error: Please sign out to continue." << endl;
            operationResult = ERR_ALREADY_SIGNIN;
            
        } else {
            char * email = (char *) malloc(sizeof(char) * MAX_LEN_EMAIL);
            char * password = (char *) malloc(sizeof(char) * MAX_LEN_PASSWORD);
            strcpy(email, argv[2]);
            strcpy(password, argv[3]);

            // sigin client and download leveldb in current folder
            operationResult = wa->signin(email, password);
        }
    }

    // exec: signout
    if( !operationFound && argc == 2 && strcmp(argv[1], "signout") == 0 ) {
        operationFound = true;
        wa->getStatus();
        
        // reject if current status is not signed-in
        if( !wa->isAuth ) {
            cout << "Error: User is not signed in." << endl;
            operationResult = ERR_NOT_SIGNIN;
            
        } else {
            if( wa->isLock ) {
                // wa->sync();
                // wa->unlock();
            } else {
                cerr << "Warning: LevelDB is not locked, local changes are discarded." << endl;
            }
            operationResult = wa->signout();
        }
    }
    
    // exec: sync
    if( !operationFound && argc == 2 && strcmp(argv[1], "sync") == 0 ) {
        operationFound = true;
        wa->getStatus();
        
        // reject if current status is signed-in
        if( !wa->isLock ) {
            cout << "Error: Can't perform any this operation because LevelDB is not locked." << endl;
            operationResult = ERR_LEVEL_NOT_LOCKED;
            
        } else {
            // operationResult = wa->sync();
        }
    }
    
    if( !operationFound && argc == 3 && strcmp(argv[1], "ls") == 0 ) {}
    if( !operationFound && argc == 4 && strcmp(argv[1], "ls-version") == 0 ) {}
    if( !operationFound && argc == 4 && strcmp(argv[1], "put") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "get") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "mv") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "cp") == 0 ) {}
    if( !operationFound && argc >= 3 && argc <= 4 && strcmp(argv[1], "rm") == 0 ) {}
    if( !operationFound && argc == 3 && strcmp(argv[1], "mkdir") == 0 ) {}
    if( !operationFound && argc == 3 && strcmp(argv[1], "rmdir") == 0 ) {}
    
    // show usage if no operation is done
    if( !operationFound ) {
        showUsage(argv[0]);
        cout << argv[0] << endl;
    }

    // delete classess
    delete wa;
    
    return operationResult;
}

void showUsage(const char * path) {
    char * executable = basename((char *) path);
    
    cout << "Command-line interface for DeDuplicatus." << endl;
    cout << endl;
    cout << "Usage:" << endl;
    cout << "\t" << executable << " status" << endl;
    cout << "\t" << executable << " signin <email> <password>" << endl;
    cout << "\t" << executable << " signout" << endl;
    cout << "\t" << executable << " sync" << endl;
    cout << "\t" << executable << " ls <path>" << endl;
    cout << "\t" << executable << " ls-version <path>" << endl;
    cout << "\t" << executable << " put <local> <remote>" << endl;
    cout << "\t" << executable << " get <remote> (<version-id>) <local>" << endl;
    cout << "\t" << executable << " mv <original> (<version-id>) <new>" << endl;
    cout << "\t" << executable << " cp <original> (<version-id>) <new>" << endl;
    cout << "\t" << executable << " rm <path> (<version-id>)" << endl;
    cout << "\t" << executable << " mkdir <path>" << endl;
    cout << "\t" << executable << " rmdir <path>" << endl;
    cout << endl;
    cout << "Caution:" << endl;
    cout << "\t! LevelDB files save at current folder." << endl;
    cout << "\t! Ensure to sync or sign out after all file operations. Otherwise the file system won't be saved." << endl;
}