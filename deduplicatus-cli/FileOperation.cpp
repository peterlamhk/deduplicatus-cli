//
//  FileOperation.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <sstream>
#include <iostream>
#include <string>
#include <regex>
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

int FileOperation::listCloud(Level *db, WebAuth *wa) {
    // in this function, no need to separate codes for two storage modes
    cout << "Provider\tDisplay Name\tUsed\tQuota" << endl;
    
    // iterate the list of cloud to get account info
    leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
    for( it->Seek("clouds::account::");
        it->Valid() && it->key().ToString() < "clouds::account::\xFF";
        it->Next() ) {
        
        smatch sm;
        regex_match(it->key().ToString(), sm, regex("^clouds::account::([0-9a-z\\-]+)::type$"));
        if( sm.size() > 0 ) {
            string cloudid = sm[1];
            CloudStorage *cloud = nullptr;
            
            if( it->value().ToString().compare(Dropbox::type()) == 0 ) {
                cloud = new Dropbox(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if( it->value().ToString().compare(OneDrive::type()) == 0 ) {
                cloud = new OneDrive(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if( it->value().ToString().compare(Box::type()) == 0 ) {
                cloud = new Box(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            
            cloud->accountInfo(db, wa, cloudid);
            
            char buf[10];
            string displayName = ( cloud->displayName.length() > 0 ) ?
                cloud->displayName :
                db->get("clouds::account::" + cloudid + "::accountName");
            
            cout << cloud->brandName() << "\t" <<
                displayName << "\t" <<
                readable_fs(cloud->space_used, buf) << "\t" <<
                readable_fs(cloud->space_quota, buf) << endl;
        }
    }

    return ERR_NONE;
}
