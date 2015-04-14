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
#include <vector>
#include <tbb/tbb.h>
#include <random>
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
    // in this function, no need to separate codes for two storage modes
    // if needed, use the following statements:
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
                cout << "Last Modified\t\tName\tSize\tUUID" << endl;
                printf("%s\t.\t0\t%s\n", date, folderuuid.c_str());
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
                regex rgx ("^folder::"+path+"([0-9a-z\\-]+)::");
                regex pwd ("^folder::"+path+"::");
                smatch match;
                if (regex_match(s, match, pwd)) {
                    continue;
                } else if (regex_search(s, match, rgx)) {
                    folderName = match[1];
                    printf("%s\t%s\t0\t%s\n", date, folderName.c_str(), folderuuid.c_str());
                }
            }
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
                        time_t t = stoi(db->get(it->key().ToString()));
                        struct tm *tm = localtime(&t);
                        char date[20];
                        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", tm);
                        lastModified = string(date);

                        string s = it->key().ToString();
                        regex rgx ("^file::" + uuid + "::([0-9a-z\\-]+)::");
                        smatch match;
                        if (regex_search(s, match, rgx)) {
                            fileuuid = match[1];
                        }
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
                        // versions = db->get(it->key().ToString());
                        cout << lastModified << "\t" << name << "\t" << lastSize << "\t" << fileuuid << endl;
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

int FileOperation::listVersion(Level *db, string path) {
    if (c->user_mode.compare(c->mode_deduplication) == 0) {
        int i;
        string modified, size, chunks;
        leveldb::Iterator *it = db->getDB()->NewIterator(leveldb::ReadOptions());
        cout << "Modified\t\tSize\tChunks" << endl;
        for (it->Seek("version::" + path + "::"), i = 0; it->Valid() && it->key().ToString() < "version::" + path + "::\xFF"; it->Next(), i++) {
            if (i % NUM_VERSION_KEY == 0) {
                chunks = db->get(it->key().ToString());
            } else if (i % NUM_VERSION_KEY == 1 ) {
                modified = db->get(it->key().ToString());
            } else {
                size = db->get(it->key().ToString());
                cout << modified << "\t" << size << "\t" << chunks << endl;
            }
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

        db->put("folder::" + (string)path + "::id", uuid());
        db->put("folder::" + (string)path + "::lastModified", timestamp.str());
        cout << "Folder created." << endl;

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

int FileOperation::putFile(Level *db, const char *path, const char *remotepath, const char *cloud) {
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
            cout << folderid << endl;

        } else {
            cerr << "Error: target remote directory not exists." << endl;
            return ERR_LOCAL_ERROR;
        }

        // init tiger hash function for chunks
        //Hash_state md;
        //unsigned char *tiger_hash = (unsigned char *) malloc(sizeof(unsigned char) * tiger_desc.hashsize);
        Hash_state md;
        unsigned char *tiger_hash = (unsigned char *) malloc(sizeof(unsigned char) * sha1_desc.hashsize);

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
        sha1_init(&md);

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
                sha1_process(&md, &buf[rp->frag_start], rp->frag_size);

                if( rp->block_done ) {
                    sha1_done(&md, tiger_hash);

                    char *result = (char *) malloc(sizeof(char) * sha1_desc.hashsize * 2);
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
                    sha1_init(&md);

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
                            break;
                        case 1:
                            clouds[1] = new OneDrive(accessToken);
                            cloudFolderIds.push_back(cloudFolderId);
                            cloudIds.push_back(cloudid);
                            break;
                        case 2:
                            clouds[2] = new Box(accessToken);
                            cloudFolderIds.push_back(cloudFolderId);
                            cloudIds.push_back(cloudid);
                            break;
                    }
                }
            }
        }

        tbb::concurrent_vector<string> containerList;

        // stdout all container file needed to upload (debug used)
        cout << endl << "Container UUID\t\t\t\t\t\t\t" << "Path" << endl;
        for( map<string, string>::iterator it = containerToBeUpload.begin();
             it != containerToBeUpload.end();
             it++ ) {
            cout << it->first << "\t" << it->second << endl;

            // TODO: hard code how many copies and upload to which cloud
            db->put("container::" + it->first + "::store::0::cloudid", cloud);

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

    } else { }

    return ERR_NONE;
}

int FileOperation::getFile(Level *db, const char *remote, const char *local, const char *reference) {
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
        if( !db->isKeyExists("version::" + versionid + "::checksum") ) {
            cerr << "Error: file version not found." << endl;
            return ERR_CLOUD_ERROR;
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
                unsigned long long actually = fwrite(emptyContent, sizeof(char), remaining, localFile);

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

        // -- debug use only: list containers needed to download
        cout << "Container Needed" << endl;
        {
            vector<string>::const_iterator it = containerNeeded.begin();
            while( it != containerNeeded.end() ) {
                cout << *it << endl;
                string ccloudid = db->get("container::" + *it + "::store::0::cloudid");
                cloudList.push_back(types[db->get("clouds::account::" + ccloudid + "::type")]);
                ++it;
            }
        }
        cout << "----------------" << endl << endl;

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
