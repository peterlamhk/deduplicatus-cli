//
//  tool.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef __deduplicatus_cli__tool__
#define __deduplicatus_cli__tool__

#include <stdio.h>
#include <string>

using namespace std;

size_t write_data(char *, size_t, size_t, void *);
size_t write_file(void *, size_t, size_t, FILE *);
size_t write_null(void *, size_t, size_t, void *);
bool file_exists(const string&);
string sha1_file(const char *);
void set_header_postform(struct curl_slist *);
void set_header_postjson(struct curl_slist *);
char* readable_fs(uint64_t, char *);
string uuid();
int createDirectory(string, bool);
int removeDirectory(string);
#endif /* defined(__deduplicatus_cli__tool__) */
