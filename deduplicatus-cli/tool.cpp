//
//  tool.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include "tool.h"
#include "define.h"
#include <stdlib.h>
#include <string>
#include <curl/curl.h>
#include <sys/stat.h>
#include <CommonCrypto/CommonDigest.h>

using namespace std;

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

string sha1_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    int eof = 0;
    unsigned char buf[MAX_FILE_READ_SIZE];
    
    CC_SHA1_CTX ctx;
    CC_SHA1_Init(&ctx);
    
    while( !eof ) {
        unsigned long fread_size = fread(buf, 1, MAX_FILE_READ_SIZE, fp);
        if( fread_size < MAX_FILE_READ_SIZE ) {
            eof = 1;
        }
        CC_SHA1_Update(&ctx, &buf, (unsigned int) fread_size);
    }
    
    // close file handler
    fclose(fp);
    
    unsigned char *digest = (unsigned char *) malloc(sizeof(unsigned char) * CC_SHA1_DIGEST_LENGTH);
    char *result = (char *) malloc(sizeof(char) * CC_SHA1_DIGEST_LENGTH * 2);
    CC_SHA1_Final(digest, &ctx);
    
    for(int b = 0; b < CC_SHA1_DIGEST_LENGTH; b++) {
        sprintf(&result[b * 2], "%02x", digest[b]);
    }

    // free memory
    free(digest);
    
    return (string) result;
}
