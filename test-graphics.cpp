// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_lines.json
// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_stations.json

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

using json = nlohmann::json;

struct waterway {
    std::string name;
};

struct map {
    std::vector<struct waterway> waterways;
};



int main() {
    std::ifstream file("test-map.json");
    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }
    try {
        json data = json::parse(file);
        for (const auto& item : data["elements"]) {
            if (item["tags"].contains("waterway")) {
                if (item["tags"].contains("name"))
                    std::cout << item["tags"]["name"] << std::endl;
                else 
                    std::cout << "waterway" << std::endl; 
                
                if (!item.contains("members")) {
                    std::cout << item["geometry"] << std::endl; 
                } else {
                }
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}