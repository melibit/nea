[out:json][timeout:25];

area["name"="Greater London"]["admin_level"="5"]->.searchArea;

(
  way["waterway"="river"](area.searchArea);
  relation["waterway"="river"](area.searchArea);
  way["waterway"="canal"](area.searchArea);
  relation["waterway"="canal"](area.searchArea);
  //way["leisure"="park"](area.searchArea);
  //relation["leisure"="park"](area.searchArea);
  //way["highway"~"primary|trunk"](area.searchArea);
);

out geom;
