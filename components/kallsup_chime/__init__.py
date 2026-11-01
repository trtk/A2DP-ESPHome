"""Continuous Bluetooth Classic doorbell output for the original ESP32."""
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import binary_sensor, esp32
from esphome.core import CORE
from esphome.const import CONF_ID, CONF_ADDRESS, CONF_NAME, DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["binary_sensor"]
ns = cg.esphome_ns.namespace("kallsup_chime")
KallsupChime = ns.class_("KallsupChime", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(KallsupChime),
        cv.Optional("speaker_name", default="KALLSUP"): cv.string_strict,
        cv.Optional(CONF_ADDRESS): cv.mac_address,
        cv.Optional("connected"): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY, entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
        cv.Optional("audio_ready"): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    }).extend(cv.COMPONENT_SCHEMA),
)

def final_validate(config):
    if CORE.using_arduino:
        raise cv.Invalid("kallsup_chime requires framework: type: esp-idf")
    if esp32.get_esp32_variant() != esp32.const.VARIANT_ESP32:
        raise cv.Invalid("kallsup_chime requires the original ESP32 (not S3/C3/C6)")
    full = fv.full_config.get()
    for other in ("esp32_ble", "esp32_ble_tracker", "bluetooth_proxy", "esp32_ble_client", "esp32_ble_server"):
        if other in full:
            raise cv.Invalid(f"kallsup_chime owns the Classic Bluetooth stack; remove {other}")

FINAL_VALIDATE_SCHEMA = final_validate

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_speaker_name(config["speaker_name"]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(str(config[CONF_ADDRESS])))
    for key in ("connected", "audio_ready"):
        if key in config:
            sensor = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sensor))
    esp32.request_bluetooth()
    esp32.request_software_coexistence()
    for key, value in {
        "CONFIG_BT_ENABLED": True,
        "CONFIG_BT_BLUEDROID_ENABLED": True,
        "CONFIG_BT_CLASSIC_ENABLED": True,
        "CONFIG_BT_BLE_ENABLED": False,
        "CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY": True,
        "CONFIG_BT_A2DP_ENABLE": True,
        "CONFIG_BT_A2DP_USE_EXTERNAL_CODEC": False,
        "CONFIG_BT_SSP_ENABLED": True,
    }.items():
        esp32.add_idf_sdkconfig_option(key, value)
