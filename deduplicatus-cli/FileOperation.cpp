//
//  FileOperation.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include "FileOperation.h"
#include "define.h"

FileOperation::FileOperation(Config *c) {
    FileOperation::c = c;
}

int FileOperation::listCloud(Level *db) {
    // in this function, no need to separate codes for two storage modes
    // if needed, use the following statements:
    if( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        
    } else {
        
    }
    
    return ERR_NONE;
}
