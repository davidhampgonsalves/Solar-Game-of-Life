idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES soc nvs_flash ulp driver CalEPD)

#idf_component_register(SRCS "demo-epaper.cpp" INCLUDE_DIRS "." REQUIRES soc nvs_flash ulp driver CalEPD)
#
# ULP support additions to component CMakeLists.txt.
#
# 1. The ULP app name must be unique (if multiple components use ULP).
set(ulp_app_name ulp_${COMPONENT_NAME})
#
# 2. Specify all assembly source files.
#    Files should be placed into a separate directory (in this case, ulp/),
#    which should not be added to COMPONENT_SRCS.
set(ulp_s_sources "ulp/adc.S")
#
# 3. List all the component source files which include automatically
#    generated ULP export file, ${ulp_app_name}.h:
set(ulp_exp_dep_srcs "main.cpp")
#
# 4. Call function to build ULP binary and embed in project using the argument
#    values above.
ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")

# (for Cale) This is just to avoid that BitBank2 components include Arduino.h
idf_build_set_property(COMPILE_OPTIONS "-D __LINUX__" APPEND) 