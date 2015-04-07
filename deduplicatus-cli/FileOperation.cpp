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
#include "FileOperation.h"
#include "Level.h"
#include "define.h"

#define NUM_FOLDER_KEY 2
#define NUM_FILE_KEY 5
#define NUM_VERSION_KEY 3

FileOperation::FileOperation(Config *c) {
    FileOperation::c = c;
}

int FileOperation::listFile(Level *db, string path) {
    // in this function, no need to separate codes for two storage modes
    // if needed, use the following statements:
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        int i, j;
        string folderName, folderuuid;
        bool pwd = false;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        cout << "Last Modified\t\tName\tSize\tUUID" << endl;
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
                regex rgx ("^folder::/([0-9a-z\\-]+)::");
                smatch match;
                if (regex_search(s, match, rgx)) {
                    folderName = match[1];
                }
                if (!pwd) {
                    printf("%s\t.\t0\t%s\n", date, folderuuid.c_str());
                    pwd = true;
                } else {
                    printf("%s\t%s\t0\t%s\n", date, folderName.c_str(), folderuuid.c_str());
                }
            }
        }

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

int FileOperation::listVersion(Level *db, string path) {
  if (c->user_mode.compare(c->mode_deduplication) == 0) {
      int i, j;
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
