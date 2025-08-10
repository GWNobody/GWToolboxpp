#pragma once

#include <filesystem>
// Get basename of current running executable
bool PathGetDocumentsPath(std::filesystem::path& out, const wchar_t* suffix);

// Get computername
bool PathGetComputerName(std::filesystem::path& out);

// create_directories without catch; returns false on failure
bool PathCreateDirectorySafe(const std::filesystem::path& path);

// is_directory without catch; returns false on failure
bool PathIsDirectorySafe(const std::filesystem::path& path, bool* out);

std::string base64_encode(const std::string& in);
std::string base64_decode(const std::string& in);
