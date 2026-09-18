// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_lines.json
// https://github.com/oobrien/vis/blob/master/tubecreature/data/tfl_stations.json
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <blend2d/blend2d.h>
#include <SFML/Graphics.hpp>
#include <cstdint>

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

    float m_minLat = 51.25f;
    float m_maxLat = 51.72f;
    float m_minLon = -0.57f;
    float m_maxLon = 0.37f;

public:
    void addWaterway(Waterway waterway) {
        m_waterways.push_back(std::move(waterway));
    }

    const std::vector<Waterway>& getWaterways() const { return m_waterways; }
    
    void render(BLContext& ctx, int width, int height) const {
        if (m_waterways.empty()) {
            std::cerr << "Warning: Map is empty." << std::endl;
            return;
        }

        ctx.set_stroke_style(BLRgba32(0xFFE3A34F)); 
        ctx.set_stroke_width(2.0);                 
        ctx.set_stroke_join(BL_STROKE_JOIN_ROUND);  
        ctx.set_stroke_caps(BL_STROKE_CAP_ROUND); 

        float padding = 40.0f;
        float usableWidth = width - (padding * 2.0f);
        float usableHeight = height - (padding * 2.0f);

        float lonRange = (m_maxLon - m_minLon) > 0.0f ? (m_maxLon - m_minLon) : 1.0f;
        float latRange = (m_maxLat - m_minLat) > 0.0f ? (m_maxLat - m_minLat) : 1.0f;

        auto project = [&](float lat, float lon) -> BLPoint {
            float normX = (lon - m_minLon) / lonRange;
            float normY = (m_maxLat - lat) / latRange;

            return BLPoint(
                padding + (normX * usableWidth),
                padding + (normY * usableHeight)
            );
        };

        for (const auto& waterway : m_waterways) {
            const auto& geom = waterway.getGeometry();
            if (geom.empty()) continue;

            BLPath path;
            BLPoint start = project(geom[0].getLat(), geom[0].getLon());
            path.move_to(start.x, start.y);

            for (size_t i = 1; i < geom.size(); ++i) {
                BLPoint next = project(geom[i].getLat(), geom[i].getLon());
                path.line_to(next.x, next.y);
            }

            ctx.stroke_path(path);
        }
    }

    static Map fromJson(const json& data) {
        Map map;

        if (!data.contains("elements") || !data["elements"].is_array()) {
            return map; 
        }

        for (const auto& element : data["elements"]) {
            if (element.contains("tags") && element["tags"].contains("waterway")) {
                
                std::string baseName = "Unnamed Waterway";
                if (element["tags"].contains("name")) {
                    baseName = element["tags"]["name"].get<std::string>();
                }

                if (element.contains("geometry") && element["geometry"].is_array()) {
                    std::vector<Point> geometry;
                    for (const auto& pt : element["geometry"]) {
                        if (pt.contains("lat") && pt.contains("lon")) {
                            geometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                        }
                    }
                    if (!geometry.empty()) {
                        map.addWaterway(Waterway(baseName, std::move(geometry)));
                    }
                } 
                else if (element.contains("members") && element["members"].is_array()) {
                    for (const auto& member : element["members"]) {
                        if (member.value("type", "") == "way" && member.contains("geometry") && member["geometry"].is_array()) {
                            std::vector<Point> memberGeometry;
                            
                            for (const auto& pt : member["geometry"]) {
                                if (pt.contains("lat") && pt.contains("lon")) {
                                    memberGeometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                                }
                            }
                            
                            if (!memberGeometry.empty()) {
                                map.addWaterway(Waterway(baseName, std::move(memberGeometry)));
                            }
                        }
                    }
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

    Map myMap;
    try {
        json data = json::parse(file);
        
        myMap = Map::fromJson(data);

        std::cout << "Successfully parsed " << myMap.getWaterways().size() << " waterways:\n" << std::endl;
    
    } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing error: " << e.what() << std::endl;
        return 1;
    } 
    
    unsigned int width = 1280;
    unsigned int height = 720;

    sf::RenderWindow window(sf::VideoMode({width, height}), "test-graphics");
    window.setFramerateLimit(60);

    sf::Texture sfTexture;
    if (!sfTexture.resize({width, height})) {
        std::cerr << "Error: Failed to initialize texture." << std::endl;
        return 1;
    }
    sf::Sprite sfSprite(sfTexture);

    BLImage img(width, height, BL_FORMAT_PRGB32);   
    
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* resizeEvent = event->getIf<sf::Event::Resized>()) {
                width = resizeEvent->size.x;
                height = resizeEvent->size.y;
                
                window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(width), static_cast<float>(height)})));
                
                img.create(width, height, BL_FORMAT_PRGB32);
                (void)sfTexture.resize({width, height});
                sfSprite.setTexture(sfTexture, true);
            }
        }
        
        BLContext ctx(img);

        ctx.clear_all();
        ctx.fill_all(BLRgba32(0xFF241E1E)); 

        myMap.render(ctx, width, height);
        ctx.end();

        BLImageData imgData;
        img.get_data(&imgData);
        
        if (imgData.stride == width * 4) {
            sfTexture.update(reinterpret_cast<const std::uint8_t*>(imgData.pixel_data), {width, height}, {0, 0});
        } else {
            const std::uint8_t* rowPtr = reinterpret_cast<const std::uint8_t*>(imgData.pixel_data);
            for (unsigned int y = 0; y < height; ++y) {
                sfTexture.update(rowPtr, {width, 1}, {0, y});
                rowPtr += imgData.stride; 
            }
        }

        window.clear();
        window.draw(sfSprite);
        window.display();
    }
    return 0;
}