//
//  Level.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 4/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include "Level.h"
#include "define.h"
#include "leveldb/db.h"

using namespace std;

Level::~Level() {
    delete db;
}

void Level::open(string path) {
    if( db && path.compare(currentPath) == 0) {
        return;
    }

    leveldb::Options options;
    options.create_if_missing = false;
    s = leveldb::DB::Open(options, path, &db);
    
    if( s.ok() ) {
        currentPath = path;
    } else {
        cerr << "Error: Can't open LevelDB." << endl;
        exit(ERR_LEVEL_CORRUPTED);
    }
}

string Level::get(string key) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return string();
    }
    
    string value;
    s = db->Get(leveldb::ReadOptions(), key, &value);
    if( s.ok() ) {
        return value;
    } else {
        cerr << "Warning: LevelDB " << s.ToString() << key << endl;
        return string();
    }
}

bool Level::put(string key, string value) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return false;
    }

    return db->Put(leveldb::WriteOptions(), key, value).ok();
}

bool Level::remove(string key) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return false;
    }
    
    return db->Delete(leveldb::WriteOptions(), key).ok();
}

leveldb::DB *Level::getDB() {
    return db;
}
