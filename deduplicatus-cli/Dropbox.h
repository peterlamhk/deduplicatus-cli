//
//  Dropbox.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 6/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__Dropbox__
#define __deduplicatus_cli__Dropbox__

#include <stdio.h>
#include "CloudStorage.h"

using namespace std;

class Dropbox: public CloudStorage {
public:
    Dropbox(string);
    static string type() { return "dropbox"; }
    string brandName() override;
    void accountInfo(Level *, WebAuth *, string) override;
    void uploadFile(string, string) override;

private:
    string path_base;
    string path_account_info;
};

#endif /* defined(__deduplicatus_cli__Dropbox__) */
