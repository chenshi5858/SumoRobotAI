idf_component_register(
    SRCS
        "audio_sampler.c"
        "main.c"
        "spectrum_analyzer.c"
        "spectrum_console.c"
        "spectrum_export.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_adc esp_timer espressif__esp-dsp
)
