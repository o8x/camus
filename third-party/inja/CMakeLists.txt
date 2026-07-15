add_library(inja INTERFACE)
target_include_directories(inja INTERFACE .)
target_sources(inja INTERFACE
		inja/inja.hpp
)
