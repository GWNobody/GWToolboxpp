// #include "stdafx.h"


#include "PluginUtils.h"

#include <ShlObj.h>
#include <Windows.h>


namespace fs = std::filesystem;

static std::error_code errcode; // Not thread safe

bool PathGetDocumentsPath(fs::path& out, const wchar_t* suffix = nullptr)
{
    wchar_t temp[MAX_PATH];
    const HRESULT result = SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, temp);

    if (FAILED(result)) {
        fwprintf(stderr, L"%S: SHGetFolderPathW failed (HRESULT:0x%lX)\n", __func__, result);
        return false;
    }
    if (!temp[0]) {
        fwprintf(stderr, L"%S: SHGetFolderPathW returned empty path\n", __func__);
        return false;
    }
    fs::path p = temp;
    if (suffix && suffix[0]) {
        p = p.append(suffix);
    }
    out.assign(p);
    return true;
}


bool PathGetComputerName(fs::path& out)
{
    wchar_t temp[MAX_COMPUTERNAME_LENGTH + 1];
    int written = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(temp, reinterpret_cast<DWORD*>(&written))) {
        fwprintf(stderr, L"%S: Cannot get computer name. (%lu)\n", __func__, GetLastError());
        return false;
    }
    if (!temp[0]) {
        fwprintf(stderr, L"%S: GetComputerNameW returned no valid computer name.\n", __func__);
        return false;
    }
    out.assign(temp);
    return true;
}

bool PathCreateDirectorySafe(const fs::path& path)
{
    bool result;
    if (!PathIsDirectorySafe(path, &result)) {
        return false;
    }
    if (result) {
        // Already a valid directory
        return true;
    }
    result = create_directories(path, errcode);
    if (errcode.value()) {
        fwprintf(stderr, L"%S: create_directories failed for %s (%lu, %S)\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
        return false;
    }
    if (!result) {
        fwprintf(stderr, L"%S: Failed to create directory %s\n", __func__, path.wstring().c_str());
        return false;
    }
    return true;
}

bool PathIsDirectorySafe(const fs::path& path, bool* out)
{
    // std::error_code errcode;
    *out = is_directory(path, errcode);
    switch (errcode.value()) {
        case 0:
            break;
        case 3:
        case 2:
            *out = false;
            return true; // 2 = The system cannot find the file specified
        default:
            fwprintf(stderr, L"%S: is_directory failed %s (%lu, %S).\n", __func__, path.wstring().c_str(), errcode.value(), errcode.message().c_str());
            return false;
    }
    return true;
}

typedef unsigned char uchar;
static const std::string b = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; //=

std::string base64_encode(const std::string& in)
{
    std::string out;

    int val = 0, valb = -6;
    for (uchar c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in)
{
    std::string out;

    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[b[i]] = i;

    int val = 0, valb = -8;
    for (uchar c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
