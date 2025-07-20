idf_component_register(SRCS "application.cc" 
                            "main.cc" 
                            "wifi_board.cc"
                            "system_info.cc"
                            "settings.cc"
                            "application.cc"
                            "ota.cc"
                            "led/single_led.cc"
                            "board/button.cc"
                            "board/compact_wifi_board.cc"
                            "board/board.cc"
                            "protocols/protocol.cc"
                            "protocols/mqtt_protocol.cc"
                            "protocols/esp_mqtt.cc"


                        INCLUDE_DIRS "." "board" "led" "protocols"
                        EMBED_TXTFILES "my_ca_cert.pem")
                     