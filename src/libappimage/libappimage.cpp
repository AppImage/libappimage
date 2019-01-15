/**
 * Implementation of the C interface functions
 */
// system
#include <cstring>
#include <sstream>

// libraries
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

// local
#include <XdgUtils/DesktopEntry/DesktopEntry.h>
#include <appimage/core/AppImage.h>
#include "utils/HashLib.h"
#include "utils/UrlEncoder.h"

#ifdef LIBAPPIMAGE_DESKTOP_INTEGRATION
#include <appimage/desktop_integration/IntegrationManager.h>
#endif

using namespace appimage::core;

namespace bf = boost::filesystem;

extern "C" {

/* Check if a file is an AppImage. Returns the image type if it is, or -1 if it isn't */
int appimage_get_type(const char* path, bool verbose) {
    try {
        AppImage appImage(path);
        return appImage.getFormat();
    } catch (...) {
        return -1;
    }
}

char** appimage_list_files(const char* path) {
    char** result = nullptr;
    try {
        AppImage appImage(path);

        std::vector<std::string> files;
        for (auto itr = appImage.files(); itr != itr.end(); ++itr)
            if (!(*itr).empty())
                files.emplace_back(*itr);


        result = static_cast<char**>(malloc(sizeof(char*) * (files.size() + 1)));
        for (int i = 0; i < files.size(); i++)
            result[i] = strdup(files[i].c_str());

        result[files.size()] = nullptr;
    } catch (...) {
        // Create empty string list
        result = static_cast<char**>(malloc(sizeof(char*)));
        result[0] = nullptr;

        return result;
    }

    return result;
}

void appimage_string_list_free(char** list) {
    for (char** ptr = list; ptr != NULL && *ptr != NULL; ptr++)
        free(*ptr);

    free(list);
}

bool
appimage_read_file_into_buffer_following_symlinks(const char* appimage_file_path, const char* file_path, char** buffer,
                                                  unsigned long* buf_size) {
    *buffer = nullptr;
    *buf_size = 0;

    try {
        AppImage appImage(appimage_file_path);

        std::vector<char> data;

        bool found = false;
        for (auto itr = appImage.files(); itr != itr.end(); ++itr)
            if (*itr == file_path) {
                data = std::vector<char>(std::istreambuf_iterator<char>(itr.read()), std::istreambuf_iterator<char>());
                found = true;
                break;
            }

        if (!found)
            return false;

        *buffer = static_cast<char*>(malloc(sizeof(char) * data.size()));
        memcpy(*buffer, data.data(), data.size());

        *buf_size = data.size();

        return true;
    } catch (...) {
        return false;
    }
}

void appimage_extract_file_following_symlinks(const char* appimage_file_path, const char* file_path,
                                              const char* target_file_path) {
    try {
        AppImage appImage(appimage_file_path);

        std::vector<char> data;

        for (auto itr = appImage.files(); itr != itr.end(); ++itr)
            if (*itr == file_path) {
                bf::ofstream output(target_file_path);
                output << itr.read().rdbuf();
                output.close();

                break;
            }

    } catch (...) {
    }
}


/*
 * Checks whether an AppImage's desktop file has set X-AppImage-Integrate=false.
 * Useful to check whether the author of an AppImage doesn't want it to be integrated.
 *
 * Returns >0 if set, 0 if not set, <0 on errors.
 */
int appimage_shall_not_be_integrated(const char* path) {
    try {
        AppImage appImage(path);
        XdgUtils::DesktopEntry::DesktopEntry entry;
        // Load Desktop Entry
        for (auto itr = appImage.files(); itr != itr.end(); ++itr) {
            const auto& entryPath = *itr;
            if (entryPath.find(".desktop") != std::string::npos && entryPath.find('/') == std::string::npos) {
                itr.read() >> entry;
                break;
            }
        }

        auto integrateEntryValue = entry.get("Desktop Entry/X-AppImage-Integrate", "true");

        boost::to_lower(integrateEntryValue);
        boost::algorithm::trim(integrateEntryValue);

        return integrateEntryValue == "false";
    } catch (...) {
        return -1;
    }
}


/*
 * Checks whether an AppImage's desktop file has set Terminal=true.
 * Useful to check whether the author of an AppImage doesn't want it to be integrated.
 *
 * Returns >0 if set, 0 if not set, <0 on errors.
 */
int appimage_is_terminal_app(const char* path) {
    try {
        AppImage appImage(path);

        std::vector<char> data;

        XdgUtils::DesktopEntry::DesktopEntry entry;
        // Load Desktop Entry
        for (auto itr = appImage.files(); itr != itr.end(); ++itr) {
            const auto& entryPath = *itr;
            if (entryPath.find(".desktop") != std::string::npos && entryPath.find("/") == std::string::npos) {
                itr.read() >> entry;
                break;
            }
        }

        auto terminalEntryValue = entry.get("Desktop Entry/Terminal");

        boost::to_lower(terminalEntryValue);
        boost::algorithm::trim(terminalEntryValue);

        return terminalEntryValue == "true";
    } catch (...) {
        return -1;
    }
}


/* Return the md5 hash constructed according to
 * https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#THUMBSAVE
 * This can be used to identify files that are related to a given AppImage at a given location */
char* appimage_get_md5(const char* path) {
    using namespace appimage::utils;

    if (path == nullptr)
        return nullptr;

    try {
        const auto& canonicalPath = bf::weakly_canonical(path);

        if (canonicalPath.empty())
            return nullptr;

        std::string uri = "file://" + UrlEncoder::encode(canonicalPath.string());

        auto md5raw = HashLib::md5(uri);
        auto md5Str = HashLib::toHex(md5raw);

        return strdup(md5Str.c_str());
    } catch (...) {
        return nullptr;
    }
}

#ifdef LIBAPPIMAGE_DESKTOP_INTEGRATION
using namespace appimage::desktop_integration;

/*
 * Register an AppImage in the system
 * Returns 0 on success, non-0 otherwise.
 */
int appimage_register_in_system(const char* path, bool verbose) {
    try {
        IntegrationManager manager;
        manager.registerAppImage(path);
        manager.generateThumbnails(path);

        return 0;
    } catch (...) {
        return 1;
    }
}


/* Unregister an AppImage in the system */
int appimage_unregister_in_system(const char* path, bool verbose) {
    try {
        IntegrationManager manager;
        manager.unregisterAppImage(path);
        manager.removeThumbnails(path);

        return 0;
    } catch (...) {
        return 1;
    }
}

/* Check whether AppImage is registered in the system already */
bool appimage_is_registered_in_system(const char* path) {
    // To check whether an AppImage has been integrated, we just have to check whether the desktop file is in place

    try {
        IntegrationManager manager;
        return manager.isARegisteredAppImage(path);
    } catch (...) {
        return false;
    }
}


#ifdef LIBAPPIMAGE_THUMBNAILER

/* Create AppImage thumbanil according to
 * https://specifications.freedesktop.org/thumbnail-spec/0.8.0/index.html
 */
void appimage_create_thumbnail(const char* appimage_file_path, bool verbose) {
    try {
        IntegrationManager manager;
        manager.generateThumbnails(appimage_file_path);
    } catch (...) {}
}

#endif // LIBAPPIMAGE_THUMBNAILER
#endif // LIBAPPIMAGE_DESKTOP_INTEGRATION

} // extern "C"