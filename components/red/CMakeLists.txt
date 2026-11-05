idf_component_register(
    SRCS "red.c" "servidor.c" "protocolo.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES estado esp_wifi esp_netif esp_event esp_http_server esp_hw_support
    EMBED_TXTFILES "../../web/index.html"
                   "../../web/app.js"
                   "../../web/estilo.css"
)
