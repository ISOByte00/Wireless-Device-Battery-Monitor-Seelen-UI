#!/usr/bin/env python3
"""
Syncs seelen-plugin/metadata.yml and seelen-plugin/plugin/template.js into
the standalone distribution bundle seelen-plugin/BatteryHub.yaml.
"""

import sys
import os

def build_yaml(template_content: str) -> str:
    # Indent template for YAML literal block
    indented_lines = []
    for line in template_content.strip().splitlines():
        if line.strip():
            indented_lines.append("    " + line)
        else:
            indented_lines.append("")
    indented_template = "\n".join(indented_lines)

    header = """id: '@isobyte/batteryhub'
metadata:
  displayName: BatteryHub
  description: BatteryHub Fancy Toolbar module.
  portrait: null
  banner: null
  screenshots: []
  tags:
  - toolbar
  appTargetVersion: null
  premium: false
  extras: {}
  remote: null
icon: LuBatteryMedium
target: '@seelen/fancy-toolbar'
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
      url: http://127.0.0.1:37842/seelen/state-text
      requestInit: null
      updateIntervalSeconds: 3
"""
    return header + "\n" + indented_template + footer


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    template_path = os.path.join(repo_root, "seelen-plugin", "plugin", "template.js")
    output_path = os.path.join(repo_root, "seelen-plugin", "BatteryHub.yaml")

    with open(template_path, "r", encoding="utf-8") as f:
        template_content = f.read()

    new_yaml = build_yaml(template_content)

    if "--check" in sys.argv:
        if not os.path.exists(output_path):
            print("Error: BatteryHub.yaml does not exist.", file=sys.stderr)
            sys.exit(1)
        with open(output_path, "r", encoding="utf-8") as f:
            current = f.read()
        if current.replace("\r\n", "\n") != new_yaml.replace("\r\n", "\n"):
            print("Error: BatteryHub.yaml is out of sync with plugin/template.js. Run scripts/sync_seelen_plugin.py to update it.", file=sys.stderr)
            sys.exit(1)
        print("BatteryHub.yaml is up to date.")
        sys.exit(0)

    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(new_yaml)
    print("Successfully synchronized seelen-plugin/BatteryHub.yaml")


if __name__ == "__main__":
    main()
