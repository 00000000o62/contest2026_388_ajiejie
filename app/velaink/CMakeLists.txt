# Contest 2026 team 388 - VelaInk stroke engine
if(CONFIG_LVX_USE_DEMO_CONTEST2026_388_VELAINK)
  nuttx_add_application(
    NAME velaink
    SRCS velaink_main.c stroke.c gcode.c
    STACKSIZE 4096)
endif()
