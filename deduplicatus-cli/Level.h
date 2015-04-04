//
//  Level.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 4/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__Level__
#define __deduplicatus_cli__Level__

#include <stdio.h>
#include <string>
#include "leveldb/db.h"

using namespace std;

class Level {
public:
    ~Level();
    
    void open(string);
    string get(string);
    bool put(string, string);
    bool remove(string);
    leveldb::DB *getDB();
    
private:
    leveldb::DB *db;
    leveldb::Status s;
    string currentPath;
};

#endif /* defined(__deduplicatus_cli__Level__) */
