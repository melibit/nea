default: api graphics 
api:
	clang++ -std=c++20 -Wall test-api.cpp -lcurl -o test-api
map:
	curl --silent --show-error -X POST \
		--data-urlencode "data@test-query.overpassql" \
		"https://maps.mail.ru/osm/tools/overpass/api/interpreter" -o test-map.json
graphics: 
	clang++ -std=c++20 -Wall test-graphics.cpp -o test-graphics	
