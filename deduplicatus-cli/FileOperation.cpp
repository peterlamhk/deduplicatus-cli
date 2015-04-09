//
//  FileOperation.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include <string>
#include <regex>
#include <sstream>
#include <iostream>
#include <string>
#include <regex>
#include <time.h>
#include <libgen.h>
#include "FileOperation.h"
#include "WebAuth.h"
#include "Level.h"
#include "Dropbox.h"
#include "OneDrive.h"
#include "Box.h"
#include "define.h"
#include "tool.h"

using namespace std;

FileOperation::FileOperation(Config *c) {
    FileOperation::c = c;
}

int FileOperation::listFile(Level *db, string path) {
    // in this function, no need to separate codes for two storage modes
    // if needed, use the following statements:
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        int i, j;
        bool exist = false;
        string folderName, folderuuid;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());

        // print current directory
        for (it->Seek("folder::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                folderuuid = db->get(it->key().ToString());
            } else {
                // folder::lastModified
                time_t t = stoi(db->get(it->key().ToString()));
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                exist = true;
                cout << "Last Modified\t\tName\tSize\tUUID" << endl;
                printf("%s\t.\t0\t%s\n", date, folderuuid.c_str());
            }
        }

        if (!exist) {
            cout << "No such file or directory" << endl;
            return ERR_NONE;
        }

        // append trailing '/' for easier regex handling
        if (path.back() != '/') {
            path += "/";
        }

        // print all subfolders in pwd
        for (it->Seek("folder::" + path), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                folderuuid = db->get(it->key().ToString());
            } else {
                // folder::lastModified
                time_t t = stoi(db->get(it->key().ToString()));
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                // folder name
                string s = it->key().ToString();
                regex rgx ("^folder::"+path+"([0-9a-z\\-]+)::");
                regex pwd ("^folder::"+path+"::");
                smatch match;
                if (regex_match(s, match, pwd)) {
                    continue;
                } else if (regex_search(s, match, rgx)) {
                    folderName = match[1];
                    printf("%s\t%s\t0\t%s\n", date, folderName.c_str(), folderuuid.c_str());
                }
            }
        }

        // print files in pwd
        for (it->Seek("folder::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                // folder::id
                string uuid = db->get(it->key().ToString());
                string fileuuid, name, versions, lastVersion, lastModified, lastSize;
                for (it->Seek("file::" + uuid + "::"), j = 0; it->Valid() && it->key().ToString() < "file::" + uuid + "::\xFF"; it->Next(), j++) {
                    switch (j % NUM_FILE_KEY) {
                    case 0: {
                        time_t t = stoi(db->get(it->key().ToString()));
                        struct tm *tm = localtime(&t);
                        char date[20];
                        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
                        lastModified = string(date);

                        string s = it->key().ToString();
                        regex rgx ("^file::" + uuid + "::([0-9a-z\\-]+)::");
                        smatch match;
                        if (regex_search(s, match, rgx)) {
                            fileuuid = match[1];
                        }
                        break;
                    }
                    case 1: {
                        lastSize = db->get(it->key().ToString());
                        break;
                    }
                    case 2: {
                        // lastVersion = db->get(it->key().ToString());
                        break;
                    }
                    case 3: {
                        name = db->get(it->key().ToString());
                        break;
                    }
                    case 4: {
                        // versions = db->get(it->key().ToString());
                        cout << lastModified << "\t" << name << "\t" << lastSize << "\t" << fileuuid << endl;
                        break;
                    }
                    }
                }
                break;
            }
        }

    } else {

    }

    return ERR_NONE;
}

int FileOperation::listCloud(Level *db, WebAuth *wa) {
    // in this function, no need to separate codes for two storage modes
    cout << "Cloud ID\t\t\t\tProvider\tDisplay Name\tUsed\tQuota" << endl;

    // iterate the list of cloud to get account info
    leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
    for ( it->Seek("clouds::account::");
            it->Valid() && it->key().ToString() < "clouds::account::\xFF";
            it->Next() ) {

        smatch sm;
        regex_match(it->key().ToString(), sm, regex("^clouds::account::([0-9a-z\\-]+)::type$"));
        if ( sm.size() > 0 ) {
            string cloudid = sm[1];
            CloudStorage *cloud = nullptr;

            if ( it->value().ToString().compare(Dropbox::type()) == 0 ) {
                cloud = new Dropbox(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if ( it->value().ToString().compare(OneDrive::type()) == 0 ) {
                cloud = new OneDrive(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if ( it->value().ToString().compare(Box::type()) == 0 ) {
                cloud = new Box(db->get("clouds::account::" + cloudid + "::accessToken"));
            }

            cloud->accountInfo(db, wa, cloudid);

            char buf[10];
            string displayName = ( cloud->displayName.length() > 0 ) ?
                                 cloud->displayName :
                                 db->get("clouds::account::" + cloudid + "::accountName");

            cout << cloudid << "\t" <<
                 cloud->brandName() << "\t" <<
                 displayName << "\t" <<
                 readable_fs(cloud->space_used, buf) << "\t" <<
                 readable_fs(cloud->space_quota, buf) << endl;
        }
    }

    return ERR_NONE;
}

int FileOperation::listVersion(Level *db, string path) {
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        int i;
        string modified, size, chunks;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        cout << "Modified\t\tSize\tChunks" << endl;
        for (it->Seek("version::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "version::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_VERSION_KEY == 0) {
                chunks = db->get(it->key().ToString());
            } else if (i % NUM_VERSION_KEY == 1 ) {
                modified = db->get(it->key().ToString());
            } else {
                size = db->get(it->key().ToString());
                cout << modified << "\t" << size << "\t" << chunks << endl;
            }
        }
    } else {

    }

    return ERR_NONE;

}

int FileOperation::makeDirectory(Level *db, const char *path, const char *cloud) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        if ( db->isKeyExists("folder::" + (string)path + "::id") ) {
            cerr << "Error: Directory already exists." << endl;
            return ERR_FOLDER_EXISTS;
        }

        if ( !db->isKeyExists("folder::" + (string)dirname((char *)path) + "::id") ) {
            cerr << "Error: Parent directory not exists." << endl;
            return ERR_PARENT_FOLDER_NOT_EXISTS;
        }

        stringstream timestamp;
        timestamp << time(NULL);

        db->put("folder::" + (string)path + "::id", uuid());
        db->put("folder::" + (string)path + "::lastModified", timestamp.str());
        cout << "Folder created." << endl;

    } else {

    }

    return ERR_NONE;
}
