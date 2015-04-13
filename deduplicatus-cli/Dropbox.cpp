//
//  Dropbox.cpp
//  deduplicatus-cli
//
//  Created by Cheuk Hin Lam on 6/4/15.
//  Copyright (c) 2015 Peter Lam. All rights reserved.
//

#define BOOST_NETWORK_ENABLE_HTTPS

#include <sstream>
#include <string>
#include <curl/curl.h>
#include <boost/network/protocol/http/client.hpp>
#include <boost/network/uri.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <regex>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "WebAuth.h"
#include "Dropbox.h"
#include "tool.h"

using namespace std;
using namespace rapidjson;
ostringstream stream_dropbox;

namespace http = boost::network::http;
namespace uri = boost::network::uri;
namespace fs = boost::filesystem;

Dropbox::Dropbox(string token) {
    accessToken = token;

    // define cloud storage endpoints
    path_base = "https://api.dropbox.com/1";
    path_account_info = "/account/info";
}

string Dropbox::brandName() {
    return "Dropbox";
}

void Dropbox::accountInfo(Level *db, WebAuth *wa, string cloudid) {
    long curl_code = 0, http_code = 0;
    bool refreshOAuth = false, success = false;

    do {
        // init curl request
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, (path_base + path_account_info).c_str());

        // set oauth header
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_dropbox);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        curl_code = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if ( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
            success = true;

            // parse response JSON for information
            Document d;
            d.Parse(stream_dropbox.str().c_str());

            Value& v_name = d["display_name"];
            Value& v_quota = d["quota_info"]["quota"];
            Value& v_normal = d["quota_info"]["normal"];
            Value& v_shared = d["quota_info"]["shared"];

            displayName = v_name.GetString();
            space_quota = v_quota.GetUint64();
            space_used = v_normal.GetUint64() + v_shared.GetUint64();

        } else {
            // dropbox api do not implement refresh token mechanism
            refreshOAuth = true;
        }
    } while ( !success && !refreshOAuth );
}

void Dropbox::uploadFile(Level *db, string folderid, string path) {
    http::client client;
    try {
        fs::path lp(path);
        string rp = "https://api-content.dropbox.com/1/files_put/auto/.deduplicatus/"  + lp.filename().string();
        http::client::request request(rp);
        request << boost::network::header("Authorization", "Bearer " + accessToken);
        http::client::response response = client.post(request, get_file_contents(path.c_str()));

        cout << static_cast<std::string>(body(response)) << endl;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return;
    }
}

void Dropbox::downloadFile(Level *db, string cid, string path) {
    http::client::options options;
    options.follow_redirects(true);
    http::client client_(options);

    try {
        string rp;
        regex rgx ("\\/([a-zA-Z0-9\\-]+)\\.");
        smatch match;
        if (regex_search(path, match, rgx)) {
            rp = "https://api-content.dropbox.com/1/files/auto/.deduplicatus/"  + string(match[1]) + ".container";
            cout << rp << endl;
        }
        http::client::request request(rp);
        request << boost::network::header("Authorization", "Bearer " + accessToken);
        http::client::response response = client_.get(request);

        std::ofstream ofs(path.c_str());
        ofs << static_cast<std::string>(body(response)) << std::endl;
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return;
    }
}
