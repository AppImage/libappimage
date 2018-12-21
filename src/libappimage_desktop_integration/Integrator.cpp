// system
#include <cstdlib>
#include <sstream>

// libraries
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <linuxdeploy/desktopfile/desktopfile.h>

// local
#include "appimage/core/AppImage.h"
#include "appimage/core/Exceptions.h"
#include "utils/XdgUserDirs.h"
#include "utils/HashLib.h"
#include "appimage/desktop_integration/Integrator.h"

namespace bf = boost::filesystem;

using namespace appimage::core;

namespace appimage {
    namespace desktop_integration {
        /**
         * Implementation of the opaque pointer patter for the integrator class
         * see https://en.wikipedia.org/wiki/Opaque_pointer
         *
         * Contain a set of helper methods that will be used at the integrator class to fulfill the different task
         */
        class Integrator::priv {
            friend class Integrator;

            std::string userDataDir;

            std::unique_ptr<AppImage> appImage;


            // Resources required to perform the AppImage desktop integration
            struct desktop_integration_resources {
                std::shared_ptr<linuxdeploy::desktopfile::DesktopFile> desktopFile;
            };

            desktop_integration_resources getDesktopIntegrationResources() {
                desktop_integration_resources resources;
                for (auto fileItr = appImage->files(); fileItr != fileItr.end(); ++fileItr) {
                    const auto& fileName = *fileItr;

                    if (isMainDesktopFile(fileName))
                        resources.desktopFile.reset(new linuxdeploy::desktopfile::DesktopFile(fileItr.read()));
                }

                return resources;
            }

            bool isMainDesktopFile(const std::string& fileName) const {
                return fileName.find(".desktop") != std::string::npos &&
                       fileName.find('/') == std::string::npos;
            }


            /**
             * @brief Build the file path were the AppImage desktop file should be copied in order to achieve the desktop
             * integration.
             *
             * The desktop file path is made by the following rule:
             * "$XDG_DATA_HOME/applications/<vendor id>_<uuid>-<application name scaped>.desktop"
             * where:
             *  - vendor id = appimagekit
             *  - uuid = appimage path md5 sum
             *  - application name scaped: the application name as in the Name entry at desktop file inside the AppImage
             *                              trime and with spaces replaced by underscores
             *
             * @param resources
             * @return desktop file path
             */
            std::string buildDesktopFilePath(const desktop_integration_resources& resources) {
                // Get AppImage path md5
                std::istringstream is(appImage->getPath());
                auto digest = utils::HashLib::md5(is);
                std::string pathMD5 = utils::HashLib::toHex(digest);

                // Get application name
                linuxdeploy::desktopfile::DesktopFileEntry applicationNameEntry;
                if (!resources.desktopFile->getEntry("Desktop Entry", "Name", applicationNameEntry))
                    throw AppImageReadError("Error while reading AppImage desktop file. Missing Name entry.");

                // scape application name to make a valid desktop file name part
                std::string applicationNameScaped = applicationNameEntry.value();
                boost::trim(applicationNameScaped);
                boost::replace_all(applicationNameScaped, " ", "_");

                // assemble the desktop file path
                std::string desktopFileName = "appimagekit_" + pathMD5 + "-" + applicationNameScaped + ".desktop";
                bf::path expectedDesktopFilePath(userDataDir + "/applications/" + desktopFileName);

                return expectedDesktopFilePath.string();
            }

            /**
             * Open the AppImage file at <path>
             * @param path
             */
            void openAppImage(const std::string& path) {
                // Open the AppImage File
                auto* ptr = new AppImage(path);
                appImage.reset(ptr);
            }
        };

        Integrator::Integrator(const std::string& path) : d_ptr(new priv) {
            d_ptr->openAppImage(path);
            d_ptr->userDataDir = utils::XdgUserDirs::data();
        }

        Integrator::Integrator(const std::string& path, const std::string& xdgDataDir) : d_ptr(new priv) {
            d_ptr->openAppImage(path);
            d_ptr->userDataDir = xdgDataDir;
        }

        Integrator::~Integrator() = default;

        void Integrator::integrate() {
            auto resources = d_ptr->getDesktopIntegrationResources();

        }

        std::string Integrator::getDesktopFilePath() {
            auto resources = d_ptr->getDesktopIntegrationResources();
            return d_ptr->buildDesktopFilePath(resources);
        }
    }
}