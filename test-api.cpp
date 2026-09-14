#include <iostream>
#include <string>
#include <unordered_set>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

struct TransitPrediction {
    std::string vehicleId;
    time_t expectedArrival;
    time_t timeToLive;
    std::string stationName;
};

std::time_t tm_to_utc(std::tm* time_struct) {
#if defined(_WIN32) || defined(_WIN64)
    return _mkgmtime(time_struct);
#else
    return timegm(time_struct); // POSIX standard
#endif
}

int main() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize cURL." << std::endl;
        return 1;    }

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

        std::vector<TransitPrediction> uniqueTrains;

        for (const auto& item : data) {
            if (item.contains("vehicleId") && item.contains("stationName")) {
                TransitPrediction pred;

                pred.vehicleId = item["vehicleId"];
                pred.stationName = item["stationName"];
                
                {
                    std::tm expectedArrival;
                    std::istringstream ss{(std::string)item["expectedArrival"]};
                    ss >> std::get_time(&expectedArrival, "%Y-%m-%dT%H:%M:%S");
                    pred.expectedArrival = tm_to_utc(&expectedArrival);
                }
                {
                    std::tm timeToLive;
                    std::istringstream ss{(std::string)item["timeToLive"]};
                    ss >> std::get_time(&timeToLive, "%Y-%m-%dT%H:%M:%S");
                    pred.timeToLive = tm_to_utc(&timeToLive);
                } 

                time_t currentTime = time(NULL);
                currentTime = tm_to_utc(gmtime(&currentTime));
                if (pred.timeToLive < currentTime)
                    continue;

                auto it = std::find_if(uniqueTrains.begin(), uniqueTrains.end(), [pred](const TransitPrediction& t) {
                    return t.vehicleId == pred.vehicleId;
                });
                
                if (it != uniqueTrains.end() ) {
                    if (it->expectedArrival < pred.expectedArrival)
                        continue;
                    it->expectedArrival = pred.expectedArrival;
                    it->stationName = pred.stationName;
                } else {
                    uniqueTrains.push_back(pred);
                }
            } else {
                std::cout << "Unrecognised Item [" << item << "]" << std::endl;
            }
        }
        
        for (const auto & pred : uniqueTrains) {
            std::cout << "Train ID: " << pred.vehicleId 
                    << " | Arriving at Station: " << pred.stationName 
                    << " | Expected at " << std::asctime(localtime(&pred.expectedArrival))
            ;
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