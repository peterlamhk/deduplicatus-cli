//
//  Chunk.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 9/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef deduplicatus_cli_Chunk_h
#define deduplicatus_cli_Chunk_h

struct Chunk {
    unsigned long long start;
    unsigned long long size;
    char *checksum;
};

#endif
