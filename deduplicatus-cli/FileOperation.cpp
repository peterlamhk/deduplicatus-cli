//
//  FileOperation.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 2/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#include <sstream>
#include <iostream>
#include <string>
#include <regex>
#include <map>
#include <vector>
#include <time.h>
#include <tomcrypt.h>
#include <libgen.h>
#include <tbb/tbb.h>
#include <random>
#include <algorithm>
#include "FileOperation.h"
#include "WebAuth.h"
#include "Level.h"
#include "Dropbox.h"
#include "OneDrive.h"
#include "Box.h"
#include "Chunk.h"
#include "leveldb/write_batch.h"
#include "define.h"
#include "tool.h"

extern "C" {
    #include "rabinpoly.h"
}

using namespace std;

FileOperation::FileOperation(Config *c) {
    FileOperation::c = c;
}

int FileOperation::listFile(Level *db, string path) {
    // in this function, separate codes for two storage modes
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        int i, j;
        bool exist = false;
        string folderName, folderuuid;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());

        // print current directory
        for (it->Seek("folder::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                folderuuid = db->get(it->key().ToString());
            } else {
                // folder::lastModified
                time_t t = stoi(db->get(it->key().ToString()));
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                exist = true;
                cout << "  Last Modified\t\tSize\tName" << endl;
                printf("D %s\t0\t.\n", date);
            }
        }

        if (!exist) {
            cout << "No such file or directory" << endl;
            return ERR_NONE;
        }

        // append trailing '/' for easier regex handling
        if (path.back() != '/') {
            path += "/";
        }

        // print all subfolders in pwd
        for (it->Seek("folder::" + path), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                folderuuid = db->get(it->key().ToString());
            } else {
                // folder::lastModified
                time_t t = stoi(db->get(it->key().ToString()));
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                // folder name
                string s = it->key().ToString();
                regex rgx ("^folder::"+path+"([^\\/]+)::");
                regex pwd ("^folder::"+path+"::");
                smatch match;
                if (regex_match(s, match, pwd)) {
                    continue;
                } else if (regex_search(s, match, rgx)) {
                    folderName = match[1];
                    printf("D %s\t0\t%s\n", date, folderName.c_str());
                }
            }
        }

        // remove trailing '/' for mkdir
        if (path != "/" && path.back() == '/') {
            path.erase(path.length()-1);
        }

        // print files in pwd
        for (it->Seek("folder::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "folder::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_FOLDER_KEY == 0) {
                // folder::id
                string uuid = db->get(it->key().ToString());
                string fileuuid, name, versions, lastVersion, lastModified, lastSize;
                for (it->Seek("file::" + uuid + "::"), j = 0; it->Valid() && it->key().ToString() < "file::" + uuid + "::\xFF"; it->Next(), j++) {
                    switch (j % NUM_FILE_KEY) {
                    case 0: {
                        break;
                    }
                    case 1: {
                        lastSize = db->get(it->key().ToString());
                        break;
                    }
                    case 2: {
                        // lastVersion = db->get(it->key().ToString());
                        break;
                    }
                    case 3: {
                        name = db->get(it->key().ToString());
                        break;
                    }
                    case 4: {
                        time_t t = stoi(db->get(it->key().ToString()));
                        struct tm *tm = localtime(&t);
                        char date[20];
                        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
                        lastModified = string(date);
                        break;
                    }
                    case 5: {
                        versions = db->get(it->key().ToString());

                        if( versions.find(";") == string::npos ) {
                            cout << "F " << lastModified << "\t" << lastSize << "\t" << name << endl;
                        } else {
                            cout << "V " << lastModified << "\t" << lastSize << "\t" << name << endl;
                        }
                        break;
                    }
                    }
                }
                break;
            }
        }

    } else { }

    return ERR_NONE;
}

int FileOperation::listCloud(Level *db, WebAuth *wa) {
    // in this function, no need to separate codes for two storage modes
    cout << "Cloud ID\t\t\t\tProvider\tDisplay Name\tUsed\tQuota" << endl;

    // iterate the list of cloud to get account info
    leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
    for ( it->Seek("clouds::account::");
            it->Valid() && it->key().ToString() < "clouds::account::\xFF";
            it->Next() ) {

        smatch sm;
        regex_match(it->key().ToString(), sm, regex("^clouds::account::([0-9a-z\\-]+)::type$"));
        if ( sm.size() > 0 ) {
            string cloudid = sm[1];
            CloudStorage *cloud = nullptr;

            if ( it->value().ToString().compare(Dropbox::type()) == 0 ) {
                cloud = new Dropbox(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if ( it->value().ToString().compare(OneDrive::type()) == 0 ) {
                cloud = new OneDrive(db->get("clouds::account::" + cloudid + "::accessToken"));
            }
            if ( it->value().ToString().compare(Box::type()) == 0 ) {
                cloud = new Box(db->get("clouds::account::" + cloudid + "::accessToken"));
            }

            cloud->accountInfo(db, wa, cloudid);

            char buf[10];
            string displayName = ( cloud->displayName.length() > 0 ) ?
                                 cloud->displayName :
                                 db->get("clouds::account::" + cloudid + "::accountName");

            cout << cloudid << "\t" <<
                 cloud->brandName() << "\t" <<
                 displayName << "\t" <<
                 readable_fs(cloud->space_used, buf) << "\t" <<
                 readable_fs(cloud->space_quota, buf) << endl;
        }
    }

    return ERR_NONE;
}

int FileOperation::listVersion(Level *db, const char *path) {
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        string filepath = (string) path;
        string filedir = dirname((char *) path);
        string filename = basename((char *) path);
        string folderid, verions;

        if ( !db->isKeyExists("folder::" + filedir + "::id") ) {
            cerr << "Error: path not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        folderid = db->get("folder::" + filedir + "::id");
        if ( !db->isKeyExists("file::" + folderid + "::" + filename + "::name") ) {
            cerr << "Error: path not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // obtain a list of verions
        verions = db->get("file::" + folderid + "::" + filename + "::versions");

        // iterate all versions and display version details to user
        cout << "File Versions of " << filepath << endl;
        cout << endl;
        cout << "Last modified\t\tSize\tVersion ID" << endl;

        char *ch = (char *) verions.c_str();
        char *p = strtok(ch, ";");

        while( p != NULL ) {
            string v = (string) p;

            {
                string modified = db->get("version::" + v + "::modified");
                string size     = db->get("version::" + v + "::size");

                time_t t = stoi(modified);
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                cout << date << "\t" << size << "\t" << v << endl;
            }

            p = strtok(NULL, ";");
        }

    } else { }

    return ERR_NONE;

}

int FileOperation::makeDirectory(Level *db, const char *path, const char *cloud) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        if ( db->isKeyExists("folder::" + (string)path + "::id") ) {
            cerr << "Error: Directory already exists." << endl;
            return ERR_FOLDER_EXISTS;
        }

        if ( !db->isKeyExists("folder::" + (string)dirname((char *)path) + "::id") ) {
            cerr << "Error: Parent directory not exists." << endl;
            return ERR_PARENT_FOLDER_NOT_EXISTS;
        }

        stringstream timestamp;
        timestamp << time(NULL);

        leveldb::WriteBatch batch;
        string folderid = uuid();

        batch.Put("folder::" + (string)path + "::id", folderid);
        batch.Put("folder::" + (string)path + "::lastModified", timestamp.str());
        batch.Put("folderid::" + folderid, (string)path);

        // update parent directory's last modified timestamp
        string parent = dirname((char *) path);
        batch.Put("folder::" + parent + "::lastModified", timestamp.str());

        // commit data change to leveldb
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }

        cout << "Folder created." << endl;

    } else { }

    return ERR_NONE;
}

int FileOperation::removeDirectory(Level *db, const char *path, const char *cloud) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        string target = (string) path;

        // reject removing root folder
        if( target.compare("/") == 0 ) {
            cerr << "Error: root directory cannot be removed." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check if target directory exists
        if ( !db->isKeyExists("folder::" + target + "::id") ) {
            cerr << "Error: directory not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check if target directory contains subfolders
        string search_path = ( target.back() != '/' ) ? target + "/" : target;

        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        for ( it->Seek("folder::" + search_path);
             it->Valid() && it->key().ToString() < "folder::" + search_path + "\xFF";
             it->Next() ) {
            // if any key matches here, means it contains subfolders
            cerr << "Error: directory not empty." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check if target directory contains file items
        string folderid = db->get("folder::" + target + "::id");

        for ( it->Seek("file::" + folderid + "::");
             it->Valid() && it->key().ToString() < "file::" + folderid + "::\xFF";
             it->Next() ) {
            // if any key matches here, means it contains subfolders
            cerr << "Error: directory not empty." << endl;
            return ERR_LOCAL_ERROR;
        }

        // update parent directory's last modified
        string parent = dirname((char *) path);
        stringstream timestamp;
        timestamp << time(NULL);

        leveldb::WriteBatch batch;
        batch.Put("folder::" + parent + "::lastModified", timestamp.str());
        batch.Delete("folder::" + target + "::id");
        batch.Delete("folder::" + target + "::lastModified");
        batch.Delete("folderid::" + folderid);

        // commit data change to leveldb
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        cout << "Success." << endl;

    } else { }
    
    return ERR_NONE;
}

class UploadTask : public tbb::task {
    Level *db;
    tbb::concurrent_vector<string> containerList;
    CloudStorage **clouds;
    vector<string> cloudIds;
    vector<string> cloudFolderIds;
    map<string, int> types;

    tbb::task* execute() {
        tbb::parallel_for_each(containerList.begin(), containerList.end(), [=](string path) {
            int count = (int)cloudIds.size();
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<int> uni(0, count-1);
            int cloudNum = uni(rng);
            string type = db->get("clouds::account::" + cloudIds[cloudNum] + "::type");
            
            regex rgx ("\\/([a-zA-Z0-9\\-]+)\\.");
            smatch match;
            if (regex_search(path, match, rgx)) {
                db->put("container::" + string(match[1]) + "::store::0::cloudid", cloudIds[cloudNum]);
            }
            clouds[types[type]]->uploadFile(db, cloudFolderIds[cloudNum], path);
        });
        return NULL;
    }
public:
    UploadTask( Level *db_, tbb::concurrent_vector<string> containerList_, CloudStorage **clouds_, vector<string> cloudIds_, vector<string> cloudFolderIds_, map<string, int> types_ )
    : db(db_), containerList(containerList_), clouds(clouds_), cloudIds(cloudIds_), cloudFolderIds(cloudFolderIds_), types(types_) {}
};

class DownloadTask : public tbb::task {
    Level *db;
    vector<string> containerNeeded;
    CloudStorage **cloud;
    string path;
    vector<int> cloudList;

    tbb::task* execute() {
        int i = 0;
        tbb::parallel_for_each(containerNeeded.begin(), containerNeeded.end(), [&i, this](string cid) {
            cloud[cloudList[i]]->downloadFile(db, cid, path + "/" + cid + ".container");
            i++;
        });
        return NULL;
    }
public:
    DownloadTask( Level *db_, vector<string> containerNeeded_, CloudStorage **cloud_, string path_, vector<int> cloudList_ )
    : db(db_), containerNeeded(containerNeeded_), cloud(cloud_), path(path_), cloudList(cloudList_) {}
};

class DeleteTask : public tbb::task {
    Level *db;
    vector<string> containerToBeDeleted;
    CloudStorage **cloud;
    string path;
    vector<int> cloudList;

    tbb::task* execute() {
        int i = 0;
        tbb::parallel_for_each(containerToBeDeleted.begin(), containerToBeDeleted.end(), [&i, this](string cid) {
            cloud[cloudList[i]]->deleteFile(db, cid);
            i++;
        });
        return NULL;
    }
public:
    DeleteTask( Level *db_, vector<string> containerToBeDeleted_, CloudStorage **cloud_, vector<int> cloudList_ )
    : db(db_), containerToBeDeleted(containerToBeDeleted_), cloud(cloud_), cloudList(cloudList_) {}
};

int FileOperation::putFile(Level *db, WebAuth *wa, const char *path, const char *remotepath, const char *cloud) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        // check local file exists
        FILE * targetFile = fopen(path, "rb");
        if( targetFile == NULL ) {
            cerr << "Error: local file not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check remote file exists
        string filehash = sha1_file(path);
        string filedir = dirname((char *) remotepath);
        string filename = basename((char *) remotepath);
        string folderid;
        string versionid = uuid();
        stringstream timestamp;
        timestamp << time(NULL);
        bool isNewFile;

        if( filename.at(filename.length() - 1) == '/' ) {
            cerr << "Error: remote path invalid." << endl;
            return ERR_LOCAL_ERROR;
        }

        if( db->isKeyExists("folder::" + filedir + "::id") ) {
            folderid = db->get("folder::" + filedir + "::id");
            isNewFile = !db->isKeyExists("file::" + folderid + "::" + filename + "::name");

        } else {
            cerr << "Error: target remote directory not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // init tiger hash function for chunks
        Hash_state md;
        unsigned char *tiger_hash = (unsigned char *) malloc(sizeof(unsigned char) * tiger_desc.hashsize);

        // init chunk vector
        vector<Chunk> chunkVector;
        Chunk *ch;

        // init rabin fingerprint
        unsigned int buf_size = MAX_FILE_READ_SIZE;
        unsigned int window_size = RB_WINDOW_SIZE;
        unsigned char buf[buf_size];
        size_t min_block_size = RB_MIN_BLOCK_SIZE;
        size_t avg_block_size = RB_AVG_BLOCK_SIZE;
        size_t max_block_size = RB_MAX_BLOCK_SIZE;

        rabinpoly *rp;
        rp = rabin_init(window_size, avg_block_size, min_block_size, max_block_size);

        // read file and feed the rabin fingerprint function
        unsigned long long total_bytes = 0;
        unsigned long long unique_bytes = 0;
        unsigned long long last_position = 0;
        int eof = 0;
        tiger_init(&md);

        while( !eof ) {
            // read buffer
            unsigned long fread_size = fread(buf, 1, buf_size, targetFile);
            if( fread_size < buf_size ) {
                eof = 1;
            }

            // feed rabin_in
            rabin_in(rp, buf, fread_size, eof);
            while( rabin_out(rp) ) {
                total_bytes += rp->frag_size;
                tiger_process(&md, &buf[rp->frag_start], rp->frag_size);

                if( rp->block_done ) {
                    tiger_done(&md, tiger_hash);

                    char *result = (char *) malloc(sizeof(char) * tiger_desc.hashsize * 2);
                    for (int b = 0; b < 20; b++) {
                        sprintf(&result[b * 2], "%02x", tiger_hash[b]);
                    }

                    // declare new chunk
                    ch = (Chunk *) malloc(sizeof(Chunk));
                    ch->start = last_position;
                    ch->size = (total_bytes - last_position);
                    ch->checksum = result;
                    chunkVector.push_back(*ch);

                    // set new chunk by init new start location and tiger hash
                    last_position = total_bytes;
                    tiger_init(&md);

                    if( rp->eof ) {
                        break;
                    }
                }
            }
        }

        // remove rabin
        free(rp);

        // cache all leveldb write operaion into batch, and write at the end of this function
        leveldb::WriteBatch batch;

        // iterate through the chunks vector and do the followings:
        // 1. concat all checksum into single string for saving as version::chunk
        // 2. check if the chunk is already in leveldb or same chunk in this file
        string versionChunk = "[";
        vector<Chunk> chunksToBeUpload;
        bool newChunk = false;
        unsigned long uniqueChunkCount = 0;

        map<string, unsigned long> countInLeveldb;
        map<string, unsigned long> countInFile;

        for( std::vector<Chunk>::iterator it = chunkVector.begin();
             it != chunkVector.end();
             ++it ) {
            string cs = (string) it->checksum;
            versionChunk = versionChunk + cs + ";";

            if( db->isKeyExists("chunks::" + cs + "::container") ) {
                // chunk IS in leveldb, sum up the number of appearance of chunk in current file
                // and update the reference count = current reference count + number of appearance in this file
                if( countInLeveldb.count(cs) > 0 ) {
                    countInLeveldb[cs] = countInLeveldb.find(cs)->second + 1;
                } else {
                    countInLeveldb.insert(pair<string, unsigned long>(cs, 1));
                }

            } else {
                // chunk NOT in leveldb, i.e. new chunk!
                // also sum up the number of appearance of chunk in current file
                if( countInFile.count(cs) > 0 ) {
                    countInFile[cs] = countInFile.find(cs)->second + 1;
                } else {
                    countInFile.insert(pair<string, unsigned long>(cs, 1));

                    unique_bytes = unique_bytes + it->size;
                    chunksToBeUpload.push_back(*it);
                    uniqueChunkCount++;
                    newChunk = true;
                }
            }
        }

        // remove trailing ; in versionChunk
        if( versionChunk.length() > 1 ) {
            versionChunk.erase(versionChunk.length() - 1, 1);
        }
        versionChunk = versionChunk + "]";

        // add new version into leveldb
        batch.Put("version::" + versionid + "::chunks", versionChunk);
        batch.Put("version::" + versionid + "::modified", timestamp.str());
        stringstream filesize;
        filesize << total_bytes;
        batch.Put("version::" + versionid + "::size", filesize.str());
        batch.Put("version::" + versionid + "::checksum", filehash);

        cout << "Original File Size (bytes):      " << total_bytes << endl;
        cout << "Total Unique Chunk Size (bytes): " << unique_bytes << endl;

        // build container by unique chunks in this file
        map<string, string> containerToBeUpload;

        if( newChunk ) {
            unsigned long currentContainerSize = 0;
            string cachePath = c->user_lock + "-cache";

            int createDirResult = createDirectory(cachePath, false);
            if( createDirResult != ERR_NONE ) {
                return createDirResult;
            }

            // initial the first container
            string containerid = uuid();
            string containerPath = cachePath + "/" + containerid + ".container";
            FILE * containerFile = fopen(containerPath.c_str(), "wb");
            unsigned char *buf;
            unsigned long fread_size;
            stringstream chunkString;

            for( std::vector<Chunk>::iterator it = chunksToBeUpload.begin();
                it != chunksToBeUpload.end();
                ++it ) {
                uniqueChunkCount--;

                // read the chunk size into buffer and write to container
                buf = (unsigned char *) malloc(sizeof(unsigned char *) * it->size);
                string cs = (string) it->checksum;

                fseek(targetFile, it->start, SEEK_SET);
                fread_size = fread(buf, sizeof(unsigned char), it->size, targetFile);

                // write chunk location in container to leveldb
                chunkString.str(""); chunkString << currentContainerSize;
                batch.Put("container::" + containerid + "::chunks::" + cs + "::start", chunkString.str());
                chunkString.str(""); chunkString << it->size;
                batch.Put("container::" + containerid + "::chunks::" + cs + "::size", chunkString.str());
                chunkString.str(""); chunkString << countInFile[cs];
                batch.Put("container::" + containerid + "::chunks::" + cs + "::referenceCount", chunkString.str());
                batch.Put("chunks::" + cs + "::container", containerid);

                fseek(containerFile, currentContainerSize, SEEK_SET);
                fwrite(buf, sizeof(unsigned char), fread_size, containerFile);
                currentContainerSize = currentContainerSize + fread_size;

                // free the read buffer
                free(buf);

                if( currentContainerSize >= AVG_CONTAINER_SIZE || uniqueChunkCount == 0 ) {
                    // close container and save to leveldb
                    fclose(containerFile);
                    batch.Put("container::" + containerid + "::checksum", sha1_file(containerPath.c_str()));

                    // insert record to the list of container to be uploaded
                    containerToBeUpload[containerid] = containerPath;

                    // if there is more chunk to process, init another container
                    if( uniqueChunkCount > 0 ) {
                        containerid = uuid();
                        containerPath = cachePath + "/" + containerid + ".container";
                        containerFile = fopen(containerPath.c_str(), "wb");
                        currentContainerSize = 0;
                    }
                }
            }
        }

        // update reference count if the chunk recorded in leveldb
        for( map<string, unsigned long>::iterator it = countInLeveldb.begin();
             it != countInLeveldb.end();
             it++ ) {
            string cs = it->first;
            unsigned long referenceCount = it->second;

            string containerid = db->get("chunks::" + cs + "::container");
            string originalCount = db->get("container::" + containerid + "::chunks::" + cs + "::referenceCount");
            referenceCount += strtoul(originalCount.c_str(), NULL, 0);

            stringstream countString;
            countString << referenceCount;
            batch.Put("container::" + containerid + "::chunks::" + cs + "::referenceCount", countString.str());
        }

        // insert version into file record or new file record in leveldb
        string listOfVersions = ( isNewFile ) ? versionid : versionid + ";" + db->get("file::" + folderid + "::" + filename + "::versions");
        batch.Put("file::" + folderid + "::" + filename + "::name", filename);
        batch.Put("file::" + folderid + "::" + filename + "::versions", listOfVersions);
        batch.Put("file::" + folderid + "::" + filename + "::lastVersion", versionid);
        batch.Put("file::" + folderid + "::" + filename + "::timestamp", timestamp.str());
        batch.Put("file::" + folderid + "::" + filename + "::lastSize", filesize.str());
        batch.Put("file::" + folderid + "::" + filename + "::lastChecksum", filehash);

        // update parent directory's last modified timestamp
        batch.Put("folder::" + filedir + "::lastModified", timestamp.str());

        // remove all maps and vectors if no longer needed
        countInFile.clear();
        countInLeveldb.clear();
        chunksToBeUpload.clear();
        for( std::vector<Chunk>::iterator it = chunkVector.begin();
            it != chunkVector.end();
            ++it ) {
            free(it->checksum);
        }
        chunkVector.clear();

        // temporary fix for possible expired access token
        bool refreshAccessToken = ( containerToBeUpload.size() > 0 );

        // save 3 cloud object to array
        CloudStorage *clouds[3];
        vector<string> cloudFolderIds;
        vector<string> cloudIds;
        for (int i = 0; i < 3; i++) {
            clouds[i] = NULL;
        }

        // initialize cloud objects
        map<string, int> types = {{"dropbox", 0}, {"onedrive", 1}, {"boxdotnet", 2}};
        string accessToken, cloudid, cloudFolderId, type;
        int i;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        for (it->Seek("clouds::account::"), i = 0; it->Valid() && it->key().ToString() < "clouds::account::\xFF"; it->Next(), i++) {
            if (i % NUM_CLOUD_ACC_KEY == 0) {
                string s = it->key().ToString();
                regex rgx ("^clouds::account::([0-9a-z\\-]+)::");
                smatch match;
                if (regex_search(s, match, rgx)) {
                    cloudid = match[1];
                }

                accessToken = db->get("clouds::account::" + cloudid + "::accessToken");
                cloudFolderId = db->get("clouds::account::" + cloudid + "::folderId");
                type = db->get("clouds::account::" + cloudid + "::type");
                if (clouds[types[type]] == NULL) {
                    switch (types[type]) {
                        case 0:
                            clouds[0] = new Dropbox(accessToken);
                            cloudFolderIds.push_back(cloudFolderId);
                            cloudIds.push_back(cloudid);
                            if( refreshAccessToken ) {
                                clouds[0]->accountInfo(db, wa, cloudid);
                            }
                            break;
                        case 1:
                            clouds[1] = new OneDrive(accessToken);
                            cloudFolderIds.push_back(cloudFolderId);
                            cloudIds.push_back(cloudid);
                            if( refreshAccessToken ) {
                                clouds[1]->accountInfo(db, wa, cloudid);
                            }
                            break;
                        case 2:
                            clouds[2] = new Box(accessToken);
                            cloudFolderIds.push_back(cloudFolderId);
                            cloudIds.push_back(cloudid);
                            if( refreshAccessToken ) {
                                clouds[2]->accountInfo(db, wa, cloudid);
                            }
                            break;
                    }
                }
            }
        }

        tbb::concurrent_vector<string> containerList;
        for( map<string, string>::iterator it = containerToBeUpload.begin();
             it != containerToBeUpload.end();
             it++ ) {
            containerList.push_back(it->second);
        }

        UploadTask *t = new(tbb::task::allocate_root()) UploadTask(db, containerList, clouds, cloudIds, cloudFolderIds, types);

        // async
        // tbb::task::enqueue(*t);

        // sync
        tbb::task::spawn_root_and_wait(*t);

        // commit changes into leveldb
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        cout << "Success." << endl;

    } else { }

    return ERR_NONE;
}

int FileOperation::getFile(Level *db, WebAuth *wa, const char *remote, const char *local, const char *reference) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        string filedir = dirname((char *) remote);
        string filename = basename((char *) remote);
        string folderid;
        string versionid;

        // search for the correct folderid and versionid
        if( db->isKeyExists("folder::" + filedir + "::id") ) {
            folderid = db->get("folder::" + filedir + "::id");

            if( db->isKeyExists("file::" + folderid + "::" + filename + "::name") ) {
                versionid = (reference == NULL) ? db->get("file::" + folderid + "::" + filename + "::lastVersion") : (string) reference;
            } else {
                cerr << "Error: remote file not found." << endl;
                return ERR_CLOUD_ERROR;
            }
        } else {
            cerr << "Error: remote file not found." << endl;
            return ERR_CLOUD_ERROR;
        }

        // search if the target version exists
        {
            string versionsInFile = db->get("file::" + folderid + "::" + filename + "::versions");
            if( versionsInFile.find(versionid) == string::npos ||
                !db->isKeyExists("version::" + versionid + "::checksum") ) {
                cerr << "Error: file version not found." << endl;
                return ERR_CLOUD_ERROR;
            }
        }

        // obtain filesize
        unsigned long long filesize;
        {
            char *filesizeString = (char *) db->get("version::" + versionid + "::size").c_str();
            filesize = strtoul(filesizeString, NULL, 0);
        }

        // check local file exists
        FILE * localFile = fopen(local, "wb");
        if( localFile == NULL ) {
            cerr << "Error: local file can't be written." << endl;
            return ERR_LOCAL_ERROR;
        }
        {
            unsigned long long remaining = filesize;
            char *emptyContent = (char *) malloc(sizeof(char) * MAX_FILE_READ_SIZE);
            while( remaining > 0 ) {
                unsigned long long writesize = ( remaining > MAX_FILE_READ_SIZE ) ? MAX_FILE_READ_SIZE : remaining;
                unsigned long long actually = fwrite(emptyContent, sizeof(char), writesize, localFile);

                if( actually != writesize ) {
                    cerr << "Error: insufficient disk space." << endl;
                    return ERR_LOCAL_ERROR;
                }
                remaining -= writesize;
            }
            free(emptyContent);
        }

        // obtain chunk list in version
        string chunks = db->get("version::" + versionid + "::chunks");

        // iterate chunks to get all container id needed
        char *ch = (char *) chunks.c_str();
        char *p = strtok(ch, ";[]");

        unsigned long long currentPosition = 0;
        map<string, vector<Chunk>> chunkInContainerOrder;

        while( p != NULL ) {
            string chunkHash = (string) p;
            string containerid = db->get("chunks::" + chunkHash + "::container");

            Chunk *chk = (Chunk *) malloc(sizeof(Chunk));
            chk->container = (char *) containerid.c_str();
            chk->start = currentPosition;
            chk->containerStart = strtoul(db->get("container::" + containerid + "::chunks::" + chunkHash + "::start").c_str(), NULL, 0);
            chk->size = strtoul(db->get("container::" + containerid + "::chunks::" + chunkHash + "::size").c_str(), NULL, 0);

            if( chunkInContainerOrder.count(containerid) == 0 ) {
                chunkInContainerOrder.insert(pair<string, vector<Chunk>>(containerid, vector<Chunk>()));
            }
            chunkInContainerOrder[containerid].push_back(*chk);

            currentPosition += chk->size;
            p = strtok(NULL, ";[]");
        }

        // find the required container id in local cache folder,
        // otherwise download from cloud storage
        vector<string> containerNeeded;
        for( map<string, vector<Chunk>>::iterator it = chunkInContainerOrder.begin();
            it != chunkInContainerOrder.end(); ++it) {
            containerNeeded.push_back(it->first);
        }

        {
            vector<string>::const_iterator it = containerNeeded.begin();
            while( it != containerNeeded.end() ) {
                if( file_exists(c->user_lock + "-cache/" + *it + ".container") ) {
                    containerNeeded.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // temporary fix for possible expired access token
        bool refreshAccessToken = ( containerNeeded.size() > 0 );

        // save 3 cloud object to array
        CloudStorage *clouds[3];
        for (int i = 0; i < 3; i++) {
            clouds[i] = NULL;
        }

        // initialize cloud objects
        map<string, int> types = {{"dropbox", 0}, {"onedrive", 1}, {"boxdotnet", 2}};
        string accessToken, cloudid, cloudFolderId, type;
        int i;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        for (it->Seek("clouds::account::"), i = 0; it->Valid() && it->key().ToString() < "clouds::account::\xFF"; it->Next(), i++) {
            if (i % NUM_CLOUD_ACC_KEY == 0) {
                string s = it->key().ToString();
                regex rgx ("^clouds::account::([0-9a-z\\-]+)::");
                smatch match;
                if (regex_search(s, match, rgx)) {
                    cloudid = match[1];
                }

                accessToken = db->get("clouds::account::" + cloudid + "::accessToken");
                cloudFolderId = db->get("clouds::account::" + cloudid + "::folderId");
                type = db->get("clouds::account::" + cloudid + "::type");
                if (clouds[types[type]] == NULL) {
                    switch (types[type]) {
                        case 0:
                            clouds[0] = new Dropbox(accessToken);
                            if( refreshAccessToken ) {
                                clouds[0]->accountInfo(db, wa, cloudid);
                            }
                            break;
                        case 1:
                            clouds[1] = new OneDrive(accessToken);
                            if( refreshAccessToken ) {
                                clouds[1]->accountInfo(db, wa, cloudid);
                            }
                            break;
                        case 2:
                            clouds[2] = new Box(accessToken);
                            if( refreshAccessToken ) {
                                clouds[2]->accountInfo(db, wa, cloudid);
                            }
                            break;
                    }

                }
            }
        }

        vector<int> cloudList;

        // -- debug use only: list containers needed to download
        {
            vector<string>::const_iterator it = containerNeeded.begin();
            while( it != containerNeeded.end() ) {
                string ccloudid = db->get("container::" + *it + "::store::0::cloudid");
                cloudList.push_back(types[db->get("clouds::account::" + ccloudid + "::type")]);
                ++it;
            }
        }

        DownloadTask *t = new(tbb::task::allocate_root()) DownloadTask(db, containerNeeded, clouds, c->user_lock + "-cache", cloudList);
        tbb::task::spawn_root_and_wait(*t);

        // iterate container to merge back the file
        unsigned char *buf;
        unsigned long fread_size;

        for( map<string, vector<Chunk>>::iterator it = chunkInContainerOrder.begin();
            it != chunkInContainerOrder.end(); ++it) {
            // take one container to open
            vector<Chunk> chunks = it->second;
            string containerPath = c->user_lock + "-cache/" + it->first + ".container";
            FILE * containerFp = fopen(containerPath.c_str(), "rb");

            // loop all chunks required in this container
            vector<Chunk>::iterator itc = chunks.begin();
            while( itc != chunks.end() ) {

                // seek to file location in container to read specific chunk size
                fseek(containerFp, itc->containerStart, SEEK_SET);
                buf = (unsigned char *) malloc(sizeof(unsigned char *) * itc->size);
                fread_size = fread(buf, 1, itc->size, containerFp);

                // seek to file location in resulting file to write chunk
                fseek(localFile, itc->start, SEEK_SET);
                fwrite(buf, sizeof(unsigned char), itc->size, localFile);

                ++itc;
            }

            fclose(containerFp);
        }

        // close file handler
        fclose(localFile);

        // double check the checksum of the original file
        string filehash = db->get("version::" + versionid + "::checksum");
        if( filehash.compare(sha1_file(local)) != 0 ) {
            cerr << "Error: incorrect file content merged." << endl;
            return ERR_LOCAL_ERROR;
        }
        cout << "Success downloaded file to " << local << endl;

    } else { }

    return ERR_NONE;
}

int FileOperation::moveFile(Level *db, const char *original, const char *destination, const char *reference) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        string original_dir = dirname((char *) original);
        string original_name = basename((char *) original);
        string destination_dir = dirname((char *) destination);
        string destination_name = basename((char *) destination);
        string original_folderid, destination_folderid;

        // no destination name provided
        if( destination_name.compare("/") == 0 ) {
            cerr << "Error: no destination filename provided." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check original file exists
        if( db->isKeyExists("folder::" + original_dir + "::id") ) {
            original_folderid = db->get("folder::" + original_dir + "::id");

            if( !db->isKeyExists("file::" + original_folderid + "::" + original_name + "::name") ) {
                cerr << "Error: original file not exists." << endl;
                return ERR_LOCAL_ERROR;
            }
        } else {
            cerr << "Error: original file not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check destination folder exists
        if( db->isKeyExists("folder::" + destination_dir + "::id") ) {
            destination_folderid = db->get("folder::" + destination_dir + "::id");

            if( db->isKeyExists("file::" + destination_folderid + "::" + destination_name + "::name") ) {
                cerr << "Error: destination file already exists." << endl;
                return ERR_LOCAL_ERROR;
            }
        } else {
            cerr << "Error: destination folder not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // count number of versions in file
        size_t numOfVersions;
        {
            string versions = db->get("file::" + original_folderid + "::" + original_name + "::versions");
            numOfVersions = std::count(versions.begin(), versions.end(), ';') + 1;
        }

        // catch all leveldb operation and execute at the end
        leveldb::WriteBatch batch;

        if( reference == NULL || numOfVersions == 1 ) {
            // move all versions in file to destination
            {
                string lastChecksum = db->get("file::" + original_folderid + "::" + original_name + "::lastChecksum");
                string lastSize     = db->get("file::" + original_folderid + "::" + original_name + "::lastSize");
                string lastVersion  = db->get("file::" + original_folderid + "::" + original_name + "::lastVersion");
                string name         = db->get("file::" + original_folderid + "::" + original_name + "::name");
                string timestamp    = db->get("file::" + original_folderid + "::" + original_name + "::timestamp");
                string versions     = db->get("file::" + original_folderid + "::" + original_name + "::versions");

                // write new file
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastChecksum", lastChecksum);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastSize", lastSize);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastVersion", lastVersion);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::name", destination_name);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::timestamp", timestamp);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::versions", versions);

                // remove old file
                batch.Delete("file::" + original_folderid + "::" + original_name + "::lastChecksum");
                batch.Delete("file::" + original_folderid + "::" + original_name + "::lastSize");
                batch.Delete("file::" + original_folderid + "::" + original_name + "::lastVersion");
                batch.Delete("file::" + original_folderid + "::" + original_name + "::name");
                batch.Delete("file::" + original_folderid + "::" + original_name + "::timestamp");
                batch.Delete("file::" + original_folderid + "::" + original_name + "::versions");
            }

        } else {
            // move ONE version to destination

            // check if version exists in file
            string version = (string) reference;
            string versionslist = db->get("file::" + original_folderid + "::" + original_name + "::versions");
            if( versionslist.find(version) == string::npos ||
                !db->isKeyExists("version::" + version + "::checksum") ) {
                cerr << "Error: original file version not exists." << endl;
                return ERR_LOCAL_ERROR;
            }

            // remove version in original file and generate new version list for destination file
            string latest_version = "";
            string new_version = "";
            {
                string original_versions = db->get("file::" + original_folderid + "::" + original_name + "::versions");
                char *ch = (char *) original_versions.c_str();
                char *p = strtok(ch, ";");

                while( p != NULL ) {
                    string v = (string) p;

                    if( v.compare(version) != 0 ) {
                        new_version += v + ";";

                        if( latest_version.length() == 0 ) {
                            latest_version = v;
                        }
                    }

                    p = strtok(NULL, ";");
                }

                // remove trailing ;
                new_version.erase(new_version.length() - 1, 1);
            }

            {
                string lastChecksum  = db->get("version::" + latest_version + "::checksum");
                string lastSize      = db->get("version::" + latest_version + "::size");
                string lastTimestamp = db->get("version::" + latest_version + "::modified");

                // write new value for original file
                batch.Put("file::" + original_folderid + "::" + original_name + "::lastChecksum", lastChecksum);
                batch.Put("file::" + original_folderid + "::" + original_name + "::lastSize", lastSize);
                batch.Put("file::" + original_folderid + "::" + original_name + "::lastVersion", latest_version);
                batch.Put("file::" + original_folderid + "::" + original_name + "::versions", new_version);
                batch.Put("file::" + original_folderid + "::" + original_name + "::timestamp", lastTimestamp);

                // create new file entry for destination file
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastChecksum", db->get("version::" + version + "::checksum"));
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastSize", db->get("version::" + version + "::size"));
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::timestamp", db->get("version::" + version + "::modified"));
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastVersion", version);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::versions", version);
                batch.Put("file::" + destination_folderid + "::" + destination_name + "::name", destination_name);
            }
        }

        // update original and destination folders' last modified
        stringstream timestamp;
        timestamp << time(NULL);

        batch.Put("folder::" + original_dir + "::lastModified", timestamp.str());
        batch.Put("folder::" + destination_dir + "::lastModified", timestamp.str());

        // commit changes into leveldb
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        cout << "Success." << endl;

    } else { }

    return ERR_NONE;
}

int FileOperation::copyFile(Level *db, const char *original, const char *destination, const char *reference) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        string original_dir = dirname((char *) original);
        string original_name = basename((char *) original);
        string destination_dir = dirname((char *) destination);
        string destination_name = basename((char *) destination);
        string original_folderid, destination_folderid;

        // no destination name provided
        if( destination_name.compare("/") == 0 ) {
            cerr << "Error: no destination filename provided." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check original file exists
        if( db->isKeyExists("folder::" + original_dir + "::id") ) {
            original_folderid = db->get("folder::" + original_dir + "::id");

            if( !db->isKeyExists("file::" + original_folderid + "::" + original_name + "::name") ) {
                cerr << "Error: original file not exists." << endl;
                return ERR_LOCAL_ERROR;
            }
        } else {
            cerr << "Error: original file not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check destination folder exists
        if( db->isKeyExists("folder::" + destination_dir + "::id") ) {
            destination_folderid = db->get("folder::" + destination_dir + "::id");

            if( db->isKeyExists("file::" + destination_folderid + "::" + destination_name + "::name") ) {
                cerr << "Error: destination file already exists." << endl;
                return ERR_LOCAL_ERROR;
            }
        } else {
            cerr << "Error: destination folder not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // versions to be copied
        string original_version = db->get("file::" + original_folderid + "::" + original_name + "::versions");
        string versions;

        if( reference == NULL ) {
            versions = original_version;
        } else {
            // find if user's input version exists in file or not
            {
                string refversion = (string) reference;
                if( original_version.find(refversion) == string::npos ||
                    !db->isKeyExists("version::" + refversion + "::checksum") ) {
                    cerr << "Error: original file version not exists." << endl;
                    return ERR_LOCAL_ERROR;
                }
            }
            versions = (string) reference;
        }

        // catch all leveldb operation and execute at the end
        leveldb::WriteBatch batch;

        // record the resulting version list for destination file
        string destination_version = "";

        // record the increment of referenceCount in all chunks
        map<string, unsigned long> refCountIncrement;

        // record informations of latest version for inserting file record
        bool firstIter = true;
        string lastVersion, lastChecksum, lastModified, lastSize;

        char *vl = (char *) versions.c_str();
        char *p = strtok(vl, ";");

        while( p != NULL ) {
            string v = (string) p;

            {
                string new_version_id = uuid();
                destination_version += new_version_id + ";";

                string chunks   = db->get("version::" + v + "::chunks");
                string modified = db->get("version::" + v + "::modified");
                string size     = db->get("version::" + v + "::size");
                string checksum = db->get("version::" + v + "::checksum");

                // insert new version into batch
                batch.Put("version::" + new_version_id + "::chunks", chunks);
                batch.Put("version::" + new_version_id + "::modified", modified);
                batch.Put("version::" + new_version_id + "::size", size);
                batch.Put("version::" + new_version_id + "::checksum", checksum);

                if( firstIter ) {
                    lastVersion = v;
                    lastChecksum = checksum;
                    lastModified = modified;
                    lastSize = size;

                    firstIter = false;
                }

                // iterate chunks and mark 1 increment for its referenceCount
                char *tok, *saved;
                for( tok = strtok_r((char *) chunks.c_str(), ";[]", &saved); tok; tok = strtok_r(NULL, ";[]", &saved) ) {
                    string chunkId = (string) tok;

                    if( refCountIncrement.count(chunkId) > 0 ) {
                        refCountIncrement[chunkId] = refCountIncrement.find(chunkId)->second + 1;
                    } else {
                        refCountIncrement.insert(pair<string, unsigned long>(chunkId, 1));
                    }
                }
            }

            p = strtok(NULL, ";");
        }

        // apply increment for chunks list
        for( map<string, unsigned long>::iterator it = refCountIncrement.begin();
            it != refCountIncrement.end();
            it++ ) {

            string chunkId = it->first;
            unsigned long increment = it->second;

            string containerId = db->get("chunks::" + chunkId + "::container");
            string original_count = db->get("container::" + containerId + "::chunks::" + chunkId + "::referenceCount");
            increment += strtoul(original_count.c_str(), NULL, 0);

            stringstream countString;
            countString << increment;

            batch.Put("container::" + containerId + "::chunks::" + chunkId + "::referenceCount", countString.str());
        }

        // remove trailing ;
        destination_version.erase(destination_version.length() - 1, 1);

        // insert file record
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastChecksum", lastChecksum);
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastSize", lastSize);
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::timestamp", lastModified);
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::lastVersion", lastVersion);
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::versions", destination_version);
        batch.Put("file::" + destination_folderid + "::" + destination_name + "::name", destination_name);

        // update destination folder's last modified
        stringstream timestamp;
        timestamp << time(NULL);

        batch.Put("folder::" + destination_dir + "::lastModified", timestamp.str());

        // commit changes into leveldb
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }
        cout << "Success." << endl;

    } else { }

    return ERR_NONE;
}

int FileOperation::removeFile(Level *db, const char *path, const char *reference) {
    if ( c->user_mode.compare(c->mode_deduplication) == 0 ) {
        string filedir = dirname((char *) path);
        string filename = basename((char *) path);
        string folderid;

        // check if target file exists
        if( db->isKeyExists("folder::" + filedir + "::id") ) {
            folderid = db->get("folder::" + filedir + "::id");

            if( !db->isKeyExists("file::" + folderid + "::" + filename + "::name") ) {
                cerr << "Error: file not exists." << endl;
                return ERR_LOCAL_ERROR;
            }

        } else {
            cerr << "Error: file not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // check target version exists
        string targetversions;
        if( reference != NULL ) {
            {
                string fileversion = db->get("file::" + folderid + "::" + filename + "::versions");
                string version = (string) reference;
                if( fileversion.find(version) == string::npos ||
                   !db->isKeyExists("version::" + version + "::checksum") ) {
                    cerr << "Error: file version not exists." << endl;
                    return ERR_LOCAL_ERROR;
                }
            }
            targetversions = (string) reference;
        } else {
            targetversions = db->get("file::" + folderid + "::" + filename + "::versions");
        }

        // count number of versions in file
        size_t numOfVersions;
        {
            string versions = db->get("file::" + folderid + "::" + filename + "::versions");
            numOfVersions = std::count(versions.begin(), versions.end(), ';') + 1;
        }

        // catch all leveldb operation and execute at the end
        leveldb::WriteBatch batch;

        if( reference == NULL || numOfVersions == 1 ) {
            // remove file record (because the only 1 version in file/ whole file is being removed)
            batch.Delete("file::" + folderid + "::" + filename + "::lastChecksum");
            batch.Delete("file::" + folderid + "::" + filename + "::lastSize");
            batch.Delete("file::" + folderid + "::" + filename + "::lastVersion");
            batch.Delete("file::" + folderid + "::" + filename + "::name");
            batch.Delete("file::" + folderid + "::" + filename + "::timestamp");
            batch.Delete("file::" + folderid + "::" + filename + "::versions");

        } else {
            // generate new list of versions and write to file record
            {
                string new_versions = "";
                string latest_version = "";
                string original_versions = db->get("file::" + folderid + "::" + filename + "::versions");
                char *ch = (char *) original_versions.c_str();
                char *p = strtok(ch, ";");

                while( p != NULL ) {
                    string v = (string) p;

                    if( v.compare(targetversions) != 0 ) {
                        new_versions += v + ";";

                        if( latest_version.length() == 0 ) {
                            latest_version = v;
                        }
                    }

                    p = strtok(NULL, ";");
                }

                // remove trailing ;
                new_versions.erase(new_versions.length() - 1, 1);

                // update file record
                batch.Put("file::" + folderid + "::" + filename + "::versions", new_versions);

                string lastChecksum  = db->get("version::" + latest_version + "::checksum");
                string lastSize      = db->get("version::" + latest_version + "::size");
                string lastTimestamp = db->get("version::" + latest_version + "::modified");
                batch.Put("file::" + folderid + "::" + filename + "::lastChecksum", lastChecksum);
                batch.Put("file::" + folderid + "::" + filename + "::lastSize", lastSize);
                batch.Put("file::" + folderid + "::" + filename + "::timestamp", lastTimestamp);
            }
        }

        // iterate the versions to be removed, mark the decrement of referenceCount for chunks
        map<string, unsigned long> refCountDecrement;

        char *vl = (char *) targetversions.c_str();
        char *p = strtok(vl, ";");

        while( p != NULL ) {
            string v = (string) p;

            {
                string chunks = db->get("version::" + v + "::chunks");

                char *tok, *saved;
                for( tok = strtok_r((char *) chunks.c_str(), ";[]", &saved); tok; tok = strtok_r(NULL, ";[]", &saved) ) {
                    string chunkId = (string) tok;

                    if( refCountDecrement.count(chunkId) > 0 ) {
                        refCountDecrement[chunkId] = refCountDecrement.find(chunkId)->second + 1;
                    } else {
                        refCountDecrement.insert(pair<string, unsigned long>(chunkId, 1));
                    }
                }
            }

            // remove version entry
            batch.Delete("version::" + v + "::chunks");
            batch.Delete("version::" + v + "::modified");
            batch.Delete("version::" + v + "::size");
            batch.Delete("version::" + v + "::checksum");

            p = strtok(NULL, ";");
        }

        // record the containers involved
        vector<string> containersInvolved;

        // apply increment for chunks list
        for( map<string, unsigned long>::iterator it = refCountDecrement.begin();
            it != refCountDecrement.end();
            it++ ) {

            string chunkId = it->first;
            unsigned long decrement = it->second;

            {
                string containerId = db->get("chunks::" + chunkId + "::container");
                string original_count = db->get("container::" + containerId + "::chunks::" + chunkId + "::referenceCount");
                decrement = strtoul(original_count.c_str(), NULL, 0) - decrement;

                stringstream countString;
                countString << decrement;

                batch.Put("container::" + containerId + "::chunks::" + chunkId + "::referenceCount", countString.str());

                if( find(containersInvolved.begin(), containersInvolved.end(), containerId) == containersInvolved.end() ) {
                    containersInvolved.push_back(containerId);
                }
            }
        }

        // apply changes before continues
        leveldb::WriteOptions write_options;
        write_options.sync = true;
        leveldb::Status s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }

        batch.Clear();

        // sum the reference count in container, break if one chunk in container has referenceCount > 0
        vector<string>::const_iterator it = containersInvolved.begin();
        while( it != containersInvolved.end() ) {
            string containerId = *it;

            if( isContainerReferred(db, containerId) ) {
                containersInvolved.erase(it);

            } else {
                // remove container from leveldb records
                leveldb::Iterator *itc = db->getDB()->NewIterator(leveldb::ReadOptions());
                for ( itc->Seek("container::" + containerId + "::");
                     itc->Valid() && itc->key().ToString() < "container::" + containerId + "::\xFF";
                     itc->Next() ) {
                    batch.Delete(itc->key().ToString());

                    {
                        smatch sm;
                        regex_match(itc->key().ToString(), sm, regex("^container::" + containerId + "::chunks::(\\w+)::referenceCount$"));
                        if ( sm.size() > 0 ) {
                            string chunkId = sm[1];
                            batch.Delete("chunks::" + chunkId + "::container");
                        }
                    }
                }

                // remove container from local cache (if exists)
                {
                    string cache_path = c->user_lock + "-cache/" + containerId + ".container";
                    if( file_exists(cache_path) ) {
                        remove(cache_path.c_str());
                    }
                }

                ++it;
            }
        }

        // remove container from cloud storage
        if( containersInvolved.size() > 0 ) {
            // save 3 cloud object to array
            CloudStorage *clouds[3];
            for (int i = 0; i < 3; i++) {
                clouds[i] = NULL;
            }

            // initialize cloud objects
            map<string, int> types = {{"dropbox", 0}, {"onedrive", 1}, {"boxdotnet", 2}};
            string accessToken, cloudid, cloudFolderId, type;
            int i;
            leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
            for (it->Seek("clouds::account::"), i = 0; it->Valid() && it->key().ToString() < "clouds::account::\xFF"; it->Next(), i++) {
                if (i % NUM_CLOUD_ACC_KEY == 0) {
                    string s = it->key().ToString();
                    regex rgx ("^clouds::account::([0-9a-z\\-]+)::");
                    smatch match;
                    if (regex_search(s, match, rgx)) {
                        cloudid = match[1];
                    }

                    accessToken = db->get("clouds::account::" + cloudid + "::accessToken");
                    cloudFolderId = db->get("clouds::account::" + cloudid + "::folderId");
                    type = db->get("clouds::account::" + cloudid + "::type");
                    if (clouds[types[type]] == NULL) {
                        switch (types[type]) {
                            case 0:
                                clouds[0] = new Dropbox(accessToken);
                                break;
                            case 1:
                                clouds[1] = new OneDrive(accessToken);
                                break;
                            case 2:
                                clouds[2] = new Box(accessToken);
                                break;
                        }

                    }
                }
            }

            vector<int> cloudList;
            {
                vector<string>::const_iterator it = containersInvolved.begin();
                while( it != containersInvolved.end() ) {
                    string ccloudid = db->get("container::" + *it + "::store::0::cloudid");
                    cloudList.push_back(types[db->get("clouds::account::" + ccloudid + "::type")]);
                    ++it;
                }
            }
            
            DeleteTask *t = new(tbb::task::allocate_root()) DeleteTask(db, containersInvolved, clouds, cloudList);
            tbb::task::spawn_root_and_wait(*t);
        }

        // apply delete container records
        s = db->getDB()->Write(write_options, &batch);
        if( !s.ok() ) {
            cerr << "Error: can't save file information into leveldb." << endl;
            return ERR_LEVEL_CORRUPTED;
        }

        cout << "Success." << endl;

    } else { }

    return ERR_NONE;
}

bool FileOperation::isContainerReferred(Level *db, string containerId) {
    leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
    for ( it->Seek("container::" + containerId + "::");
         it->Valid() && it->key().ToString() < "container::" + containerId + "::\xFF";
         it->Next() ) {

        smatch sm;
        regex_match(it->key().ToString(), sm, regex("(.*)::referenceCount$"));
        if ( sm.size() > 0 ) {
            string referenceCount = it->value().ToString();

            if( strtoul(referenceCount.c_str(), NULL, 0) > 0 ) {
                return true;
            }
        }
    }

    return false;
}

int FileOperation::searchItem(Level *db, const char *k) {
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        string keyword = (string) k;

        if( keyword.length() == 0 ) {
            cerr << "Error: please enter a keyword." << endl;
            return ERR_LOCAL_ERROR;
        }

        // result header
        cout << "  Last Modified\t\tSize\tPath" << endl;

        // setup iterator
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());

        // iterate folder list
        for ( it->Seek("folder::");
             it->Valid() && it->key().ToString() < "folder::\xFF";
             it->Next() ) {
            string currentKey = it->key().ToString();

            string fullpath;
            smatch prefix, postfix, pathmatch;
            regex_match(currentKey, prefix, regex("^folder::(.+)" + keyword + "([^\\/]*)::lastModified$"));
            regex_match(currentKey, postfix, regex("^folder::(.*)" + keyword + "([^\\/]+)::lastModified$"));
            if ( (prefix.size() + postfix.size()) > 0 ||
                currentKey.compare("folder::" + keyword + "::lastModified") == 0 ) {
                regex_match(currentKey, pathmatch, regex("^folder::(.*)::lastModified$"));

                time_t t = stoi(it->value().ToString());
                struct tm *tm = localtime(&t);
                char date[20];
                strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                fullpath = pathmatch[1];
                printf("D %s\t0\t%s\n", date, fullpath.c_str());
            }
        }

        // get root folder id
        string rootfolder = db->get("folder::/::id");

        // iterate file list
        for ( it->Seek("file::");
             it->Valid() && it->key().ToString() < "file::\xFF";
             it->Next() ) {
            string currentKey = it->key().ToString();

            smatch prefix, postfix, fullmatch;
            regex_match(currentKey, prefix, regex("^file::([0-9a-z\\-]+)::(.+)" + keyword + "(.*)::name$"));
            regex_match(currentKey, postfix, regex("^file::([0-9a-z\\-]+)::(.+)" + keyword + "(.*)::name$"));
            regex_match(currentKey, fullmatch, regex("^file::([0-9a-z\\-]+)::" + keyword + "::name$"));
            if ( (prefix.size() + postfix.size() + fullmatch.size()) > 0 ) {
                string filename = it->value().ToString(), folderid;

                if( prefix.size() > 0 ) { folderid = prefix[1]; }
                if( postfix.size() > 0 ) { folderid = postfix[1]; }
                if( fullmatch.size() > 0 ) { folderid = fullmatch[1]; }

                {
                    string folderpath, modified, versions, size;
                    if( folderid.compare(rootfolder) == 0 ) {
                        folderpath = "/";
                    } else {
                        folderpath = db->get("folderid::" + folderid);
                    }

                    modified = db->get("file::" + folderid + "::" + filename + "::timestamp");
                    versions = db->get("file::" + folderid + "::" + filename + "::versions");
                    size = db->get("file::" + folderid + "::" + filename + "::lastSize");

                    time_t t = stoi(modified);
                    struct tm *tm = localtime(&t);
                    char date[20];
                    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);

                    if( versions.find(";") == string::npos ) {
                        cout << "F " << date << "\t" << size << "\t" << folderpath << filename << endl;
                    } else {
                        cout << "V " << date << "\t" << size << "\t" << folderpath << filename << endl;
                    }
                }
            }
        }

    } else { }

    return ERR_NONE;
}
