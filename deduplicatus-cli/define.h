//
//  error.h
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#ifndef deduplicatus_cli_error_h
#define deduplicatus_cli_error_h

#define FILE_MANAGER_ENABLED false

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
#define ERR_CLOUD_ERROR 11
#define ERR_INVALID_INPUT 12
#define ERR_FOLDER_EXISTS 13
#define ERR_PARENT_FOLDER_NOT_EXISTS 14

#define MAX_LEN_EMAIL 255
#define MAX_LEN_PASSWORD 255
#define MAX_LEN_UUID 36
#define MAX_LEN_QUERY 1024

#define MAX_NETWORK_TRIES 5
#define MAX_NETWORK_TIMEOUT 15
#define MAX_NETWORK_TIMEOUT_CLOUD 60

#define MAX_FILE_READ_SIZE 65536 * 2
#define AVG_CONTAINER_SIZE 1024 * 1024 * 4

#define NUM_FOLDER_KEY 2
#define NUM_FILE_KEY 5
#define NUM_VERSION_KEY 3

#define RB_WINDOW_SIZE 32
#define RB_MIN_BLOCK_SIZE 1024
#define RB_AVG_BLOCK_SIZE 8192
#define RB_MAX_BLOCK_SIZE 65536

#endif
