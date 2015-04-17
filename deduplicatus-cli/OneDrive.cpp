//
//  OneDrive.cpp
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
#include "OneDrive.h"
#include "tool.h"

using namespace std;
using namespace rapidjson;
ostringstream stream_onedrive;

namespace http = boost::network::http;
namespace uri = boost::network::uri;
namespace fs = boost::filesystem;

OneDrive::OneDrive(string token) {
    accessToken = token;

    // define cloud storage endpoints
    path_base = "https://api.onedrive.com/v1.0";
    path_account_info = "/drive/";
}

string OneDrive::brandName() {
    return "OneDrive";
}

void OneDrive::accountInfo(Level *db, WebAuth *wa, string cloudid) {
    long curl_code = 0, http_code = 0;
    int refreshOAuth = 0;
    bool success = false;

    do {
        // init curl request
        CURL *curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, (path_base + path_account_info).c_str());

        // set oauth header
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream_onedrive);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

        curl_code = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if( http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK ) {
            success = true;

            // parse response JSON for information
            Document d;
            d.Parse(stream_onedrive.str().c_str());

            Value& v_name = d["owner"]["user"]["displayName"];
            Value& v_quota = d["quota"]["total"];
            Value& v_used = d["quota"]["used"];

            displayName = v_name.GetString();
            space_quota = v_quota.GetUint64();
            space_used = v_used.GetUint64();

        } else {
            // non-200 response, try to refresh access token
            refreshOAuth++;

            if( refreshOAuth == 1 ) {
                wa->refreshToken(db, cloudid);
                accessToken = db->get("clouds::account::" + cloudid + "::accessToken");
            }
        }

        // clear up stream
        stream_onedrive.str("");
        stream_onedrive.clear();
    } while( !success && refreshOAuth == 1 );
}

void OneDrive::uploadFile(Level *db, string folderid, string path) {
    http::client client;
    try {
        fs::path lp(path);
        string rp = "https://api.onedrive.com/v1.0/drive/root:/.deduplicatus/" + lp.filename().string() + ":/content?access_token=" + accessToken;
        http::client::request request(rp);
        http::client::response response = client.put(request, get_file_contents(path.c_str()));

//        // parse response JSON for information
//        Document d;
//        d.Parse(static_cast<std::string>(body(response)).c_str());
//        Value& v_id = d["id"];
//        // TODO: file id strange characters
//        regex rgx ("\\/([a-zA-Z0-9\\-]+)\\.");
//        smatch match;
//        if (regex_search(path, match, rgx)) {
//            // TODO: hard code how many copies
//            db->put("container::"+string(match[1])+"::store::0::fileid", v_id.GetString());
//        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return;
    }
}

void OneDrive::downloadFile(Level *db, string cid, string path) {
    http::client::options options;
    options.follow_redirects(true);
    http::client client_(options);

    try {
        string rp = "https://api.onedrive.com/v1.0/drive/root:/.deduplicatus/" + cid + ".container:/content?access_token=" + accessToken;
        http::client::request request(rp);
        http::client::response response = client_.get(request);

        // TODO: I don't know why follow redirect doesn't work
        const auto headers2 = boost::network::http::headers(response);
        for (const auto h : headers2) {
//            cout << "Header name: " << h.first << "; header value: " << h.second << '\n';
            if (!h.first.compare("Location")) {
                http::client::request request2(h.second);
                http::client c(options);
                http::client::response response2 = c.get(request2);

                std::ofstream ofs(path.c_str());
                ofs << static_cast<std::string>(body(response2)) << std::endl;
            }
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return;
    }


}
