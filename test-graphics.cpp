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
#include <SFML/Window/Mouse.hpp>
#include <cstdint>
#include <algorithm>

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

// EPSG:3544 
class WebMercator {
public:
    static constexpr double EarthRadius = 6378137.0;
    static constexpr double HalfCircumference = EarthRadius * M_PI;

    static BLPoint project(double lat, double lon) {
        double x = lon * (M_PI / 180.0) * EarthRadius;
        
        // Stop Inf at the poles! 
        if (lat > 85.05112878) lat = 85.05112878;
        if (lat < -85.05112878) lat = -85.05112878;
        
        double latRad = lat * (M_PI / 180.0);
        double y = std::log(std::tan((M_PI / 4.0) + (latRad / 2.0))) * EarthRadius;
        
        return BLPoint(x, y);
    }
};

class Camera {
private:
    BLPoint m_centre;
    float m_zoom;
public:
    Camera() : m_centre(0.0, 0.0), m_zoom(1.0) {}

    void centreOn(BLPoint worldPos, float initialZoom) {
        m_centre = worldPos;
        m_zoom = initialZoom;
    }

    void pan(float dx, float dy) {
        m_centre.x += dx / m_zoom;
        m_centre.y -= dy / m_zoom; // in Graphics an increase in y in down, not up 
    }

    void zoomAt(float factor, float anchorX, float anchorY, int width, int height) {
        BLPoint worldAnchor = screenToWorld(anchorX, anchorY, width, height);
        
        m_zoom = std::clamp<float>(m_zoom * factor, 0.01, 50.0);

        BLPoint newWorldAnchor = screenToWorld(anchorX, anchorY, width, height);
        m_centre.x += (worldAnchor.x - newWorldAnchor.x);
        m_centre.y += (worldAnchor.y - newWorldAnchor.y);
    }

    BLPoint screenToWorld(double screenX, double screenY, int width, int height) const {
        double halfW = width / 2.0;
        double halfH = height / 2.0;

        double worldX = m_centre.x + (screenX - halfW) / m_zoom;
        double worldY = m_centre.y - (screenY - halfH) / m_zoom;
        return BLPoint(worldX, worldY);
    }

    BLMatrix2D getTransformationMatrix(int width, int height) const {
        BLMatrix2D mat;
        mat.reset();
        
        mat.translate(width / 2.0, height / 2.0);
        mat.scale(m_zoom, -m_zoom);
        mat.translate(-m_centre.x, -m_centre.y);
        
        return mat;
    }

    float getPixelsPerMetre() const {
        float currentLat = 51.49f; // could be dynamic to slightly increase accuracy?
        float latCorrection = std::cos(currentLat * (M_PI / 180.0));
    
        return m_zoom / latCorrection;
    }
};

class Element {
public:
    virtual void render(BLContext& ctx, int width, int height, const Camera& camera) const = 0; 
};

class LineString : public Element {
private:
    std::vector<Point> m_geometry;
    BLRgba32 m_colour;
public:
    LineString(std::vector<Point> geometry, BLRgba32 colour) : m_geometry(std::move(geometry)), m_colour(colour) {}
    const std::vector<Point>& getGeometry() const { return m_geometry; }

    void render(BLContext& ctx, int width, int height, const Camera& camera) const override {
        if (m_geometry.empty()) return;
        
        ctx.save();

        ctx.set_transform(camera.getTransformationMatrix(width, height));
        double scaleFactor = camera.getTransformationMatrix(width, height).m00; 
        double lineThickness = 2.0 / (scaleFactor > 0.0001 ? scaleFactor : 1.0); 

        ctx.set_stroke_style(m_colour); 
        ctx.set_stroke_width(lineThickness);                 
        ctx.set_stroke_join(BL_STROKE_JOIN_ROUND);  
        ctx.set_stroke_caps(BL_STROKE_CAP_ROUND); 


        BLPath path;
        BLPoint start = WebMercator::project(m_geometry[0].getLat(), m_geometry[0].getLon());
        path.move_to(start.x, start.y);

        for (size_t i = 1; i < m_geometry.size(); ++i) {
            BLPoint next = WebMercator::project(m_geometry[i].getLat(), m_geometry[i].getLon());
            path.line_to(next.x, next.y);
           // ctx.set_fill_style(BLRgba32(0xFF0000FF)); 
           // ctx.fill_circle(next.x, next.y, lineThickness);
        }

        ctx.stroke_path(path);
        ctx.restore();

    } 
};

class Polygon : public Element {
private:
    std::vector<Point> m_geometry;
    BLRgba32 m_colour;
public:
    Polygon(std::vector<Point> geometry, BLRgba32 colour) : m_geometry(std::move(geometry)), m_colour(colour) {}
    const std::vector<Point>& getGeometry() const { return m_geometry; }
};

class Waterway : public LineString {
private:
    std::string m_name;
public:
    Waterway(std::string name, std::vector<Point> geometry)
        :  LineString(std::move(geometry), BLRgba32(0xFFE3A34F)), m_name(name) {}

    const std::string& getName() const { return m_name; }
};

class Highway : public LineString {
private:
    std::string m_name;
public:
    Highway(std::string name, std::vector<Point> geometry)
        :  LineString(std::move(geometry), BLRgba32(0xFFA3A3A3)), m_name(name) {}

    const std::string& getName() const { return m_name; }
};


class ScaleBar : public Element {
private:
    float m_target_width;
    BLFont m_font; 
public:
    ScaleBar(float targetWidth, std::string fontPath) : m_target_width(targetWidth) {
        BLFontFace face;
        if (face.create_from_file(fontPath.c_str()) != BL_SUCCESS) {
            std::cerr << "Failed to Load Font From" << fontPath;
            return;
        }
        m_font.create_from_face(face, 15.0f);
    }
    void render(BLContext& ctx, int width, int height, const Camera& camera) const {
        float targetMetres = (width*m_target_width) / camera.getPixelsPerMetre();

        float chosenMetres = 1000.0;
        std::string label = "1 km";

        if (targetMetres >= 50000.0)      { chosenMetres = 50000.0; label = "50 km"; }
        else if (targetMetres >= 20000.0) { chosenMetres = 20000.0; label = "20 km"; }
        else if (targetMetres >= 10000.0) { chosenMetres = 10000.0; label = "10 km"; }
        else if (targetMetres >= 5000.0)  { chosenMetres = 5000.0;  label = "5 km";  }
        else if (targetMetres >= 2000.0)  { chosenMetres = 2000.0;  label = "2 km";  }
        else if (targetMetres >= 1000)    { chosenMetres = 1000.0;  label = "1 km";  }
        else if (targetMetres >= 500.0)   { chosenMetres = 500.0; label = "500 m"; }
        else if (targetMetres >= 200.0)   { chosenMetres = 200.0; label = "200 m"; }
        else if (targetMetres >= 100.0)   { chosenMetres = 100.0; label = "100 m"; }
        else if (targetMetres >= 50.0)    { chosenMetres = 50.0;  label = "50 m";  }
        else                              { chosenMetres = 10.0;  label = "10 m";  }

        float barWidth = chosenMetres * camera.getPixelsPerMetre();

        float paddingX = 30.0;
        float paddingY = 30.0;
        float barHeight = 6.0;
        
        float startX = width - paddingX - barWidth;
        float endX = width - paddingX;
        float barY = height - paddingY;

        ctx.save();
        
        ctx.set_fill_style(BLRgba32(0x88000000)); 
        ctx.fill_rect(startX - 10, barY - 25, barWidth + 20, barHeight + 35);

        ctx.set_stroke_style(BLRgba32(0xFFFFFFFF));
        ctx.set_stroke_width(2.0);
        
        BLPath scalePath;
        scalePath.move_to(startX, barY - barHeight);
        scalePath.line_to(startX, barY);
        scalePath.line_to(endX, barY);
        scalePath.line_to(endX, barY - barHeight);
        
        ctx.stroke_path(scalePath);
        
        ctx.set_fill_style(BLRgba32(0xFFFFFFFF));
        BLGlyphBuffer buf;
        buf.set_utf8_text(label.c_str());
        
        BLTextMetrics tm;
        m_font.get_text_metrics(buf, tm);

        float textX = startX + (barWidth / 2.0) - (tm.bounding_box.x0 + (tm.bounding_box.x1 - tm.bounding_box.x0) / 2.0); 
        ctx.fill_utf8_text(BLPoint(textX, barY - 10), m_font, label.c_str());

        ctx.restore();
    }
};

class Map {
private:
    std::vector<Element*> m_elements;

public:
    void addElement(Element* element) {
        m_elements.push_back(std::move(element));
    }

    const std::vector<Element*>& getElements() const { return m_elements; }
    
    BLPoint getGeographicCentre() const {
        float midLat = (51.25f + 51.72f) / 2.0f;
        float midLon = (-0.57f + 0.37f) / 2.0f;
        return WebMercator::project(midLat, midLon);
    }

    void render(BLContext& ctx, int width, int height, const Camera& camera) const {
        if (m_elements.empty()) {
            std::cerr << "Warning: Map is empty." << std::endl;
            return;
        }
        for (const auto& element : m_elements) {
            element->render(ctx, width, height, camera);
        }
    }

    static Map fromJSON(const json& data) {
        Map map;

        if (!data.contains("elements") || !data["elements"].is_array()) {
            return map; 
        }

        for (const auto& element : data["elements"]) {
            if (!element.contains("tags"))
                continue;
                
            std::string baseName = "Unnamed";
            if (element["tags"].contains("name")) {
                baseName = element["tags"]["name"].get<std::string>();
            }

            if (element.contains("geometry") && element["geometry"].is_array()) {
                std::vector<Point> geometry;
                for (const auto& pt : element["geometry"]) {
                    if (!pt.contains("lat") || !pt.contains("lon"))
                        continue;
                    geometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                }
                if (!geometry.empty()) {
                    if (element["tags"].contains("waterway"))
                        map.addElement(new Waterway(baseName, std::move(geometry)));
                    if (element["tags"].contains("highway"))
                        map.addElement(new Highway(baseName, std::move(geometry)));
                }
            } 
            else if (element.contains("members") && element["members"].is_array()) {
                for (const auto& member : element["members"]) {
                    if (member.value("type", "") != "way")
                        continue;
                    if (member.contains("geometry") && member["geometry"].is_array()) {
                        std::vector<Point> memberGeometry;
                        
                        for (const auto& pt : member["geometry"]) {
                            if (!pt.contains("lat") || !pt.contains("lon")) 
                                continue;
                            memberGeometry.emplace_back(pt["lat"].get<float>(), pt["lon"].get<float>());
                        }
                        
                        if (!memberGeometry.empty()) {
                            if (element["tags"].contains("waterway"))
                                map.addElement(new Waterway(baseName, std::move(memberGeometry)));
                            if (element["tags"].contains("highway"))
                                map.addElement(new Highway(baseName, std::move(memberGeometry)));
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
        
        myMap = Map::fromJSON(data);

        std::cout << "Successfully parsed " << myMap.getElements().size() << " elements:" << std::endl;
        
   } catch (const json::parse_error& e) {
        std::cerr << "JSON Parsing error: " << e.what() << std::endl;
        return 1;
    } 
    
    
    Camera myCamera;
    myCamera.centreOn(myMap.getGeographicCentre(), 0.05);

    ScaleBar myScaleBar(0.125f, "fonts/HammersmithOne.ttf");

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
   
    sf::Vector2i lastPos;
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    sf::Vector2i mouseDelta;

    sf::Clock clock;
    while (window.isOpen()) {
        clock.start();
        lastPos = mousePos;
        mousePos = sf::Mouse::getPosition(window);
        mouseDelta = mousePos - lastPos;
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
            else if(const auto* scrollEvent = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (scrollEvent->delta < 0)
                    myCamera.zoomAt(1.05, mousePos.x, mousePos.y, width, height);
                if (scrollEvent->delta > 0)
                    myCamera.zoomAt(0.95, mousePos.x, mousePos.y, width, height);

            }
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            myCamera.pan(-mouseDelta.x, -mouseDelta.y);
        }
        
        BLContext ctx(img);

        ctx.clear_all();
        ctx.fill_all(BLRgba32(0xFF241E1E)); 

        myMap.render(ctx, width, height, myCamera);
        myScaleBar.render(ctx, width, height, myCamera);
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

        sf::Time time = clock.getElapsedTime();
        clock.reset();

        window.setTitle("test-graphics | " + std::to_string(time.asMilliseconds()) + "ms");
    }
    return 0;
}