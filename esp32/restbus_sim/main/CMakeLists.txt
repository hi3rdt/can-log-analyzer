idf_component_register(
    SRCS "main.c" "can_bus.c" "replay.c" "console_cmds.c" "gvret_csv.c" "fault_inject.c"
    INCLUDE_DIRS "."
    # The demo drive log generated from config/gateway.dbc (VehicleSpeed 0x100 with
    # alive counter + checksum, EngineData 0x200, BodyStatus 0x300, lamps 0x2F0)
    EMBED_TXTFILES "../../../samples/demo_drive.csv"
    REQUIRES driver console esp_timer
)
