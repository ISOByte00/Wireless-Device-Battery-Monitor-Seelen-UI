#!/usr/bin/env python3
"""
Syncs metadata.yml and plugin/template.js into the standalone distribution bundle.
"""

import sys
import os

def read_metadata(path: str) -> dict[str, str]:
    values = {}
    for line in open(path, encoding="utf-8"):
        if ":" not in line or line.lstrip().startswith("-"):
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        if key in {"id", "displayName", "description", "icon", "target", "url"}:
            values[key] = value.strip().strip('"').strip("'")
    return values


def build_yaml(template_content: str, metadata: dict[str, str]) -> str:
    # Indent template for YAML literal block
    indented_lines = []
    for line in template_content.strip().splitlines():
        if line.strip():
            indented_lines.append("    " + line)
        else:
            indented_lines.append("")
    indented_template = "\n".join(indented_lines)

    header = f"""id: '{metadata['id']}'
metadata:
  displayName: {metadata['displayName']}
  description: {metadata['description']}
  portrait: null
  banner: null
  screenshots: []
  tags:
  - toolbar
  appTargetVersion: null
  premium: false
  extras: {{}}
  remote: null
icon: {metadata['icon']}
target: '{metadata['target']}'
plugin:
  id: ''
  scopes: []
  template: |2
"""

    footer = """
  render: null
  canvasSize: null
  tooltip: null
  badge: null
  onClick: null
  onWheelUp: null
  onWheelDown: null
  style: {}
  remoteData:
    batteryHub:
      url: __REMOTE_URL__
      requestInit: null
      updateIntervalSeconds: 3
""".replace("__REMOTE_URL__", metadata["url"])
    return header + "\n" + indented_template + footer


def main():
    seelen_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    template_path = os.path.join(seelen_root, "plugin", "plugin", "template.js")
    metadata_path = os.path.join(seelen_root, "plugin", "metadata.yml")
    output_path = os.path.join(seelen_root, "plugin", "BatteryHub.yaml")

    with open(template_path, "r", encoding="utf-8") as f:
        template_content = f.read()

    new_yaml = build_yaml(template_content, read_metadata(metadata_path))

    if "--check" in sys.argv:
        if not os.path.exists(output_path):
            print("Error: BatteryHub.yaml does not exist.", file=sys.stderr)
            sys.exit(1)
        with open(output_path, "r", encoding="utf-8") as f:
            current = f.read()
        if current.replace("\r\n", "\n") != new_yaml.replace("\r\n", "\n"):
            print("Error: BatteryHub.yaml is out of sync. Run third-party/seelen-ui/scripts/sync_plugin.py.", file=sys.stderr)
            sys.exit(1)
        print("BatteryHub.yaml is up to date.")
        sys.exit(0)

    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(new_yaml)
    print("Successfully synchronized third-party/seelen-ui/plugin/BatteryHub.yaml")


if __name__ == "__main__":
    main()
