//
//  OneDrive.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 6/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__OneDrive__
#define __deduplicatus_cli__OneDrive__

#include <stdio.h>
#include "CloudStorage.h"

using namespace std;

class OneDrive: public CloudStorage {
public:
    OneDrive(string);
    static string type() { return "onedrive"; }
    string brandName() override;
    void accountInfo(Level *, WebAuth *, string) override;

private:
    string path_base;
    string path_account_info;
};

#endif /* defined(__deduplicatus_cli__OneDrive__) */
