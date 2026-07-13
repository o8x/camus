add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE .)
target_sources(nlohmann_json INTERFACE 
	nlohmann/json.hpp
)
