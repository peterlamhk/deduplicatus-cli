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
    repaired = false;

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
        if( s.IsCorruption() && !repaired ) {
            // try to repair the leveldb
            delete db;

            repaired = true;
            leveldb::RepairDB(currentPath, leveldb::Options());
            cerr << "Warning: Try to repair LevelDB." << endl;

            // open db and try once more time
            leveldb::Options options;
            options.create_if_missing = false;
            s = leveldb::DB::Open(options, currentPath, &db);
            return get(key);
        }
        return string();
    }
}

bool Level::put(string key, string value) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return false;
    }

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    s = db->Put(write_options, key, value);
    if( s.IsIOError() || s.IsCorruption() )  {
        cerr << "Error: LevelDB " << s.ToString() << endl;
        exit(ERR_LEVEL_CORRUPTED);
    } else {
        return s.ok();
    }
}

bool Level::remove(string key) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return false;
    }

    leveldb::WriteOptions write_options;
    write_options.sync = true;
    s = db->Delete(write_options, key);
    if( s.IsIOError() || s.IsCorruption() )  {
        cerr << "Error: LevelDB " << s.ToString() << endl;
        exit(ERR_LEVEL_CORRUPTED);
    } else {
        return s.ok();
    }
}

bool Level::isKeyExists(string key) {
    if( !db ) {
        cerr << "Warning: LevelDB not opened." << endl;
        return false;
    }
    
    string value;
    s = db->Get(leveldb::ReadOptions(), key, &value);
    if( s.IsIOError() || s.IsCorruption() )  {
        cerr << "Error: LevelDB " << s.ToString() << endl;
        exit(ERR_LEVEL_CORRUPTED);
    } else {
        return s.ok();
    }
}

leveldb::DB *Level::getDB() {
    return db;
}
