


idf_component_register(SRCS "main.cpp" "ethernet.cpp"
    "leds_ws2812.cpp"
    "leds_hub75.cpp"
    INCLUDE_DIRS "."
)




#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
