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

bool file_exists(const string&);
string sha1_file(const char *);
void set_header_postform(struct curl_slist *);
void set_header_postjson(struct curl_slist *);
#endif /* defined(__deduplicatus_cli__tool__) */