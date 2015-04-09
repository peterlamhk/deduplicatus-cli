//
//  tool.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "tool.h"
#include "define.h"
#include <stdlib.h>
#include <string>
#include <curl/curl.h>
#include <sys/stat.h>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <tomcrypt.h>

using namespace std;

size_t write_data(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}

size_t write_file(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

size_t write_null(void *ptr, size_t size, size_t nmemb, void *data) {
    return size * nmemb;
}

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

string sha1_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    int eof = 0;
    unsigned char buf[MAX_FILE_READ_SIZE];
    unsigned char* hashResult = (unsigned char *) malloc(sizeof(unsigned char) * sha1_desc.hashsize);

    hash_state md;
    sha1_init(&md);

    while( !eof ) {
        unsigned long fread_size = fread(buf, 1, MAX_FILE_READ_SIZE, fp);
        if( fread_size < MAX_FILE_READ_SIZE ) {
            eof = 1;
        }
        sha1_process(&md, buf, (unsigned int) fread_size);
    }

    // close file handler
    fclose(fp);

    sha1_done(&md, hashResult);

    char *result = (char *) malloc(sizeof(char) * sha1_desc.hashsize * 2);
    for (int b = 0; b < 20; b++) {
        sprintf(&result[b * 2], "%02x", hashResult[b]);
    }

    // free memory
    delete[] hashResult;

    return (string) result;
}

char* readable_fs(uint64_t size/*in bytes*/, char *buf) {
    double size_d = (double) size;
    int i = 0;
    const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    while (size_d > 1024) {
        size_d /= 1024;
        i++;
    }
    sprintf(buf, "%.*f %s", i, size_d, units[i]);
    return buf;
}

string uuid() {
    boost::uuids::random_generator gen;
    boost::uuids::uuid u = gen();

    return boost::uuids::to_string(u);
}
