idf_component_register(SRCS "application.cc" 
                            "main.cc" 
                            "wifi_board.cc"
                            "system_info.cc"
                            "settings.cc"
                            "application.cc"
                            "led/single_led.cc"
                            # "led/gpio_led.cc"
                            "board/button.cc"
                            "board/compact_wifi_board.cc"
                            "board/board.cc"



                        INCLUDE_DIRS "." "board" "led")
                     