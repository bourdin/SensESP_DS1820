#   PlatformIO pre-build script: inject secrets as compile-time defines and
#   configure OTA upload, all from a gitignored secrets.txt.
#
#   secrets.txt holds KEY=VALUE lines (see secrets.txt.example). Every key is
#   exposed to the firmware as a C string macro, e.g. WIFI_SSID="MyNetwork", so
#   nothing sensitive lives in the source. When the espota upload protocol is
#   active, OTA_PASSWORD and OTA_UPLOAD_PORT additionally configure the upload
#   (--auth and the target address). If secrets.txt is absent the build still
#   succeeds: Wi-Fi falls back to the captive-portal config and OTA stays off.

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)
import os

secrets_path = os.path.join(env["PROJECT_DIR"], "secrets.txt")

secrets = {}
if os.path.isfile(secrets_path):
    with open(secrets_path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            key = key.strip()
            if key:
                secrets[key] = value.strip()

if not secrets:
    print("load_secrets: no secrets.txt found, skipping secret injection")
else:
    # Expose every secret to the firmware as a quoted C string macro.
    # StringifyMacro safely quotes/escapes the value into a C string literal.
    defines = [(k, env.StringifyMacro(v)) for k, v in secrets.items()]
    env.Append(CPPDEFINES=defines)
    print("load_secrets: injected %s" % ", ".join(k for k, _ in secrets.items()))

    # When uploading over the air, supply the device address and auth password.
    if env.get("UPLOAD_PROTOCOL") == "espota":
        if secrets.get("OTA_UPLOAD_PORT"):
            env.Replace(UPLOAD_PORT=secrets["OTA_UPLOAD_PORT"])
        if secrets.get("OTA_PASSWORD"):
            env.Append(UPLOAD_FLAGS=["--auth=" + secrets["OTA_PASSWORD"]])
            print("load_secrets: espota upload will authenticate with OTA_PASSWORD")
