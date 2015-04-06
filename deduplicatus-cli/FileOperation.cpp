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

int FileOperation::listFile(Level *db, char *path) {

    return ERR_NONE;
}
