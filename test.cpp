#include <iostream>
#include <string>
#include <unordered_set>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

int main() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL." << std::endl;
        return 1;
    }

    std::string url = "https://api.tfl.gov.uk/Line/bakerloo/Arrivals";
    std::string readBuffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
        return 1;
    }

    try {
        json data = json::parse(readBuffer);

        std::unordered_set<std::string> uniqueTrains;

        for (const auto& item : data) {
            if (item.contains("vehicleId") && item.contains("currentLocation")) {
                std::string vehicleId = item["vehicleId"];
                std::string currentLocation = item["currentLocation"];
                std::string destination = item.value("destinationName", "Unknown Destination");
                int timeToStation = item.value("timeToStation", -1);

                if (uniqueTrains.find(vehicleId) == uniqueTrains.end()) {
                    uniqueTrains.insert(vehicleId);
                    
                    std::cout << "Train ID: " << vehicleId 
                              << " | Location: " << currentLocation 
                              << " (Heading to " << destination
                              << " | Time to Station " << timeToStation 
                              << std::endl;
                }
            } else {
                std::cout << "Unrecognised Item [" << item << "]" << std::endl;
            }
        }

        if (uniqueTrains.empty()) {
            std::cout << "No active trains" << std::endl;
        }

    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_lines.json