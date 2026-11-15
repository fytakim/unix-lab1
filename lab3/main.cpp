#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>

using namespace std;
namespace fs = std::filesystem;

struct FileInfo {
    string path;
    string hash;
};

bool sha1_file(const string& name, string& result)
{
    ifstream in(name, ios::binary);
    if (!in)
        return false;

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    char block[4096];
    while (in.read(block, sizeof(block)) || in.gcount() > 0)
        SHA1_Update(&ctx, block, in.gcount());
    if (in.bad())
        return false;

    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1_Final(digest, &ctx);

    result = "";
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        char two[3];
        snprintf(two, sizeof(two), "%02x", digest[i]);
        result += two;
    }
    return true;
}

bool same_device(const string& path1, const string& path2)
{
    struct stat st1, st2;
    if (stat(path1.c_str(), &st1) != 0)
        return false;
    if (stat(path2.c_str(), &st2) != 0)
        return false;
    return st1.st_dev == st2.st_dev;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        cout << "Использование: ./main <каталог>" << endl;
        return 1;
    }

    vector<FileInfo> files;
    int seen = 0, failed = 0;

    try {
        for (fs::directory_entry e : fs::recursive_directory_iterator(argv[1], fs::directory_options::skip_permission_denied)) {
            if (e.is_regular_file() && !e.is_symlink()) {
                seen++;
                string path = e.path().string();
                string hash;

                if (!sha1_file(path, hash)) {
                    cout << "Ошибка: не удалось прочитать " << path << endl;
                    failed++;
                    continue;
                }

                FileInfo info;
                info.path = path;
                info.hash = hash;
                files.push_back(info);
            }
        }
    } catch (fs::filesystem_error& err) {
        cout << "Ошибка: " << err.what() << endl;
        failed++;
    }

    sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
        return a.hash < b.hash;
    });

    int replaced = 0, was_linked = 0, other_disk = 0;

    for (int i = 1; i < files.size(); i++) {
        if (files[i].hash != files[i - 1].hash)
            continue;

        string path = files[i].path;

        int j = i - 1;
        while (j >= 0 && files[j].hash == files[i].hash && !same_device(files[j].path, path))
            j--;

        if (j < 0 || files[j].hash != files[i].hash) {
            cout << "Пропуск: " << path << " (другой диск)" << endl;
            other_disk++;
            continue;
        }

        string original = files[j].path;
        error_code ec;

        if (fs::equivalent(original, path, ec)) {
            was_linked++;
        } else {
            string temp = path + ".tmp";
            if (link(original.c_str(), temp.c_str()) != 0) {
                cout << "Ошибка: " << path << ": " << strerror(errno) << endl;
                failed++;
            } else if (rename(temp.c_str(), path.c_str()) != 0) {
                cout << "Ошибка: " << path << ": " << strerror(errno) << endl;
                unlink(temp.c_str());
                failed++;
            } else {
                cout << path << " -> " << original << endl;
                replaced++;
            }
        }
    }

    cout << "Файлов: " << seen
         << ", заменено на ссылки: " << replaced
         << ", уже были ссылками: " << was_linked
         << ", пропущено: " << other_disk
         << ", ошибок: " << failed << endl;
    return 0;
}
