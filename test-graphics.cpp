// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_lines.json
// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_stations.json
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>

using json = nlohmann::json;

class Point {
private:
    float m_lat;
    float m_lon;

public:
    Point(float lat, float lon) : m_lat(lat), m_lon(lon) {}

    float getLat() const { return m_lat; }
    float getLon() const { return m_lon; }
};

class Waterway {
private:
    std::string m_name;
    std::vector<Point> m_geometry;

public:
    Waterway(std::string name, std::vector<Point> geometry)
        : m_name(std::move(name)), m_geometry(std::move(geometry)) {}

    const std::string& getName() const { return m_name; }
    const std::vector<Point>& getGeometry() const { return m_geometry; }
};

class Map {
private:
    std::vector<Waterway> m_waterways;

public:
    void addWaterway(Waterway waterway) {
        m_waterways.push_back(std::move(waterway));
    }

    const std::vector<Waterway>& getWaterways() const { return m_waterways; }

    static Map fromJson(const json& data) {
        Map map;

        if (!data.contains("elements") || !data["elements"].is_array()) {
            return map; 
        }

        for (const auto& element : data["elements"]) {
            if (element.contains("tags") && element["tags"].contains("waterway")) {
                
                std::string name = "Unnamed Waterway";
                if (element["tags"].contains("name")) {
                    name = element["tags"]["name"].get<std::string>();
                }

                std::vector<Point> geometry;

                if (element.contains("geometry") && element["geometry"].is_array()) {
                    for (const auto& pt : element["geometry"]) {
                        if (pt.contains("lat") && pt.contains("lon")) {
                            geometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                        }
                    }
                } 
                else if (element.contains("members") && element["members"].is_array()) {
                    for (const auto& member : element["members"]) {
                        if (member.contains("geometry") && member["geometry"].is_array()) {
                            for (const auto& pt : member["geometry"]) {
                                if (pt.contains("lat") && pt.contains("lon")) {
                                    geometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                                }
                            }
                        }
                    }
                }

                if (!geometry.empty()) {
                    map.addWaterway(Waterway(std::move(name), std::move(geometry)));
                }
            }
        }
        return map;
    }
};

int main() {
    std::ifstream file("test-map.json");
    if (!file.is_open()) {
        std::cerr << "Error: Could not open test-map.json" << std::endl;
        return 1;
    }

    try {
        json data = json::parse(file);
        
        Map myMap = Map::fromJson(data);

        std::cout << "Successfully parsed " << myMap.getWaterways().size() << " waterways:\n" << std::endl;
        
        for (const auto& waterway : myMap.getWaterways()) {
            std::cout << "Waterway: " << waterway.getName() << std::endl;
            std::cout << "(" << waterway.getGeometry().size() << " points):" << std::endl;
            for (const auto& point : waterway.getGeometry()) {
                std::cout << "  [" << point.getLat() << ", " << point.getLon() << "]" << std::endl;
            }
        }

    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing error: " << e.what() << std::endl;
        return 1;
    } 

    return 0;
}