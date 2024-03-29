//
//  CloudStorage.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 7/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef deduplicatus_cli_CloudStorage_h
#define deduplicatus_cli_CloudStorage_h
class CloudStorage {
public:
    CloudStorage() {}
    virtual string brandName() { return ""; };
    virtual void accountInfo(Level *, WebAuth *, string) { };
    virtual void uploadFile(Level *, string, string) { };
    virtual void downloadFile(Level *, string, string) { };
    virtual void deleteFile(Level *, string) { };

    string displayName = "";
    string cloudid = "";
    uint64_t space_quota = 0;
    uint64_t space_used = 0;
    string accessToken = "";
};

#endif
