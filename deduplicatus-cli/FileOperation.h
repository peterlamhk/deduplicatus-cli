//
//  FileOperation.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__FileOperation__
#define __deduplicatus_cli__FileOperation__

#include <stdio.h>
#include "Config.h"
#include "WebAuth.h"
#include "Level.h"

using namespace std;

class FileOperation {
public:
    FileOperation(Config *);

    int listFile(Level *, string);
    int listVersion(Level *, string);
    int listCloud(Level *, WebAuth *);
    int makeDirectory(Level *, const char *, const char *);
    int putFile(Level *, const char *, const char *, const char *);

private:
    Config *c;
};

#endif /* defined(__deduplicatus_cli__FileOperation__) */
