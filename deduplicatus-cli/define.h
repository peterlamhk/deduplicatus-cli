//
//  error.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef deduplicatus_cli_error_h
#define deduplicatus_cli_error_h

#define ERR_NONE 0
#define ERR_ALREADY_SIGNIN 1
#define ERR_NOT_SIGNIN 2
#define ERR_LEVEL_LOCKED 3
#define ERR_LEVEL_NOT_LOCKED 4
#define ERR_LEVEL_CORRUPTED 5
#define ERR_LEVEL_NOT_FINALIZED 6
#define ERR_WRONG_CREDENTIALS 7
#define ERR_SERVER_ERROR 8
#define ERR_LOCAL_ERROR 9
#define ERR_NETWORK_ERROR 10

#define MAX_LEN_EMAIL 255
#define MAX_LEN_PASSWORD 255
#define MAX_LEN_UUID 36
#define MAX_LEN_QUERY 1024

#define MAX_NETWORK_TRIES 5
#define MAX_NETWORK_TIMEOUT 15
#define MAX_NETWORK_TIMEOUT_CLOUD 60

#define MAX_FILE_READ_SIZE 4096

#endif
