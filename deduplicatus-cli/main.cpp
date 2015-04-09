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

using namespace std;

void showUsage(const char *);
int requireLocked(WebAuth *);
int main(int, const char * []);

int main(int argc, const char * argv[]) {
    Config *c = new Config();
    WebAuth *wa = new WebAuth(c);
    FileOperation *fo = new FileOperation(c);
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
            cerr << "Error: Please sign out to continue." << endl;
            operationResult = ERR_ALREADY_SIGNIN;

        } else {
            char * email = (char *) malloc(sizeof(char) * MAX_LEN_EMAIL);
            char * password = (char *) malloc(sizeof(char) * MAX_LEN_PASSWORD);
            strcpy(email, argv[2]);
            strcpy(password, argv[3]);

            // sigin client and download leveldb in current folder
            operationResult = wa->signin(email, password);

            if( operationResult != ERR_NONE ) {
                // signout the account if there is a problem while locking leveldb
                wa->signout(false);
            }
        }
    }

    // exec: signout
    if( !operationFound && argc == 2 && strcmp(argv[1], "signout") == 0 ) {
        operationFound = true;
        wa->getStatus();

        // reject if current status is not signed-in
        if( !wa->isAuth ) {
            cerr << "Error: User is not signed in." << endl;
            operationResult = ERR_NOT_SIGNIN;

        } else {
            int dbNotSynced = 1;
            if( wa->isLock ) {
                dbNotSynced = wa->sync();
                wa->unlock();
            } else {
                cerr << "Warning: LevelDB is not locked, local changes are discarded." << endl;
            }

            // signout function will remove local leveldb if it is synced
            operationResult = wa->signout(dbNotSynced == 0);
        }
    }

    // exec: sync
    if( !operationFound && argc == 2 && strcmp(argv[1], "sync") == 0 ) {
        operationFound = true;
        wa->getStatus();

        if( (operationResult = requireLocked(wa)) == ERR_NONE ) {
            operationResult = wa->sync();
        }
    }

    // exec: ls-cloud
    if( !operationFound && argc == 2 && strcmp(argv[1], "ls-cloud") == 0 ) {
        operationFound = true;
        wa->getStatus();
        
        if( (operationResult = requireLocked(wa)) == ERR_NONE ) {
            Level *db = new Level();
            db->open(c->user_lock);
            operationResult = fo->listCloud(db, wa);
            
            // ensure to close leveldb handler
            delete db;
        }
    }

    // exec: repair
    if( !operationFound && argc == 2 && strcmp(argv[1], "repair") == 0 ) {
        operationFound = true;
        wa->getStatus();

        if((operationResult = requireLocked(wa)) == ERR_NONE) {
            leveldb::RepairDB(c->user_lock, leveldb::Options());
            cout << "LevelDB (" + c->user_lock + "/) repaired." << endl;
            return ERR_NONE;
        }
    }

    // exec: ls
    if( !operationFound && argc == 3 && strcmp(argv[1], "ls") == 0 ) {
      operationFound = true;
      wa->getStatus();
      if((operationResult = requireLocked(wa)) == ERR_NONE) {
        Level *db = new Level();
        db->open(c->user_lock);

        string path = string(argv[2]);
        operationResult = fo->listFile(db, path);

        // ensure to close leveldb handler
        delete db;
      }
    }

    // exec: ls-version
    if( !operationFound && argc == 3 && strcmp(argv[1], "ls-version") == 0 ) {
        operationFound = true;
        wa->getStatus();
        if((operationResult = requireLocked(wa)) == ERR_NONE) {
          Level *db = new Level();
          db->open(c->user_lock);

          string path = string(argv[2]);
          operationResult = fo->listVersion(db, path);

          // ensure to close leveldb handler
          delete db;
        }
    }
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "put") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "get") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "mv") == 0 ) {}
    if( !operationFound && argc >= 4 && argc <= 5 && strcmp(argv[1], "cp") == 0 ) {}
    if( !operationFound && argc >= 3 && argc <= 4 && strcmp(argv[1], "rm") == 0 ) {}

    // exec: mkdir
    if( !operationFound && argc >= 3 && argc <= 4 && strcmp(argv[1], "mkdir") == 0 ) {
        operationFound = true;
        wa->getStatus();
        
        if( (operationResult = requireLocked(wa)) == ERR_NONE ) {
            Level *db = new Level();
            db->open(c->user_lock);
            operationResult = ( argc == 3 ) ?
                fo->makeDirectory(db, argv[2], "") :     // Deduplication-enabled Mode
                fo->makeDirectory(db, argv[2], argv[3]); // File Manager Mode
            
            // ensure to close leveldb handler
            delete db;
        }
    }
    
    if( !operationFound && argc >= 3 && argc <= 4 && strcmp(argv[1], "rmdir") == 0 ) {}

    // show usage if no operation is done
    if( !operationFound ) {
        showUsage(argv[0]);
        cout << argv[0] << endl;
    }

    // delete classess
    delete wa;

    return operationResult;
}

int requireLocked(WebAuth *wa) {
    if( !wa->isAuth ) {
        cerr << "Error: User is not signed in." << endl;
        return ERR_NOT_SIGNIN;

    } else if( !wa->isLock ) {
        cerr << "Error: Can't perform any this operation because LevelDB is not locked." << endl;
        return ERR_LEVEL_NOT_LOCKED;

    } else {
        return ERR_NONE;
    }
}

void showUsage(const char * path) {
    char * executable = basename((char *) path);

    cout << "Command-line interface for DeDuplicatus." << endl;
    cout << endl;

    cout << "Usage (General):" << endl;
    cout << "\t" << executable << " status" << endl;
    cout << "\t" << executable << " signin <email> <password>" << endl;
    cout << "\t" << executable << " signout" << endl;
    cout << "\t" << executable << " sync" << endl;
    cout << "\t" << executable << " ls-cloud" << endl;
    cout << "\t" << executable << " repair" << endl;
    cout << endl;

    if( FILE_MANAGER_ENABLED ) {
        cout << "Usage (File Manager Mode):" << endl;
        cout << "\t" << executable << " ls <path>" << endl;
        cout << "\t" << executable << " put <local> <remote> <cloud-id>" << endl;
        cout << "\t" << executable << " get <remote> <cloud-id> <local>" << endl;
        cout << "\t" << executable << " mv <original> <cloud-id> <new>" << endl;
        cout << "\t" << executable << " cp <original> <cloud-id> <new>" << endl;
        cout << "\t" << executable << " rm <path> <cloud-id>" << endl;
        cout << "\t" << executable << " mkdir <path> <cloud-id>" << endl;
        cout << "\t" << executable << " rmdir <path> <cloud-id>" << endl;
        cout << endl;
    }

    cout << "Usage (Deduplication-enabled Mode):" << endl;
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
